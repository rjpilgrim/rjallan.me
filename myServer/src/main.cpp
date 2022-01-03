/**
    @file   main.cpp
    @author Ryan Allan
    @brief  the main loop for server: reads lime samples, writes samples to buffer for web socket, and converts samples to streamable audio format
 */
#include "lime/LimeSuite.h"
#include <iostream>
#include <chrono>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <AudioSink.hpp>
#include <WAVWriter.hpp>
#ifdef __linux__
#include <AlsaStreamer.hpp>
#endif
#include <RadioStreamer.hpp>
#include <LimeStreamer.hpp>
#include <HackRFStreamer.hpp>
#include <IQWebSocketServer.hpp>
#include <CommandLine.hpp>

#define MAX_PCM 32767

using namespace std;

static int my_channel = 0;
static double mono_band = 15000;
static double sample_rate = 2205000;
static double max_difference = ((mono_band/sample_rate) * 2 * M_PI);
static std::unique_ptr<AudioSink> audioWriter;
static std::unique_ptr<RadioStreamer> radioStreamer;


static double filter_coeff_one[] = {   //fir1(31, 0.20, hann(32)) - 200 khz LPF for 2205000 Hz sample rate

    0 ,  0.000079099908320 ,  0.000797971113176 ,  0.002287550802041  , 0.003401864922000  , 0.001992922319137 , -0.003692853174559 ,
    -0.013137559823147 , -0.022365739650744 , -0.024494861794093 , -0.012252113322688  , 0.018430757439072,
   0.065434079382850 ,  0.119746537821204  , 0.167777512063154 ,  0.195994831994277 ,  0.195994831994277 ,  0.167777512063154  ,
   0.119746537821204 ,  0.065434079382850  , 0.018430757439072 , -0.012252113322688 , -0.024494861794093 , -0.022365739650744 ,
  -0.013137559823147 , -0.003692853174559  , 0.001992922319137 ,  0.003401864922000  , 0.002287550802041 ,  0.000797971113176 ,  0.000079099908320 ,      0
};

/*static double filter_coeff_one[] = {   //fir1(31, 0.40, hann(32)) - 200 khz LPF for 1102500 Hz sample rate
    0 , -0.0001570993780150857 , 0.0003381932094938418 , 0.002275299083516856 , 0.00208612366364782 , -0.004208593800586378 ,
    -0.01072130155763298 , -0.003737411426019404 , 0.01745049151523847 , 0.02736395513988654 , -0.00129673748333856 , -0.05244322844396793 ,
    -0.05938547198644741 , 0.03522995587486178 , 0.2054523332385446 , 0.3417534923508179 , 0.3417534923508179 , 0.2054523332385446 ,
    0.03522995587486179 , -0.05938547198644741 , -0.05244322844396794 , -0.001296737483338559 , 0.02736395513988656 , 0.01745049151523848 ,
    -0.003737411426019404 , -0.01072130155763298 , -0.004208593800586378 , 0.00208612366364782 , 0.002275299083516856 , 0.0003381932094938418 , -0.0001570993780150874 , -0
};*/

static double filter_coeff_two[] = {   //b= fir1(31, 0.01360544217, hann(32)) - 15 khz LPF for 1102500 Hz sample rate

    0  , 0.0006297659406518796  ,  0.002511986542558811  ,   0.00559420103232427 ,   0.009769777358151056   ,  0.01488202164192106  ,   
    0.02073063182807278  ,   0.02708021746023739  ,   0.03367050686658642  ,   0.04022777987048493 ,
    0.04647700264810423  ,   0.05215410466516634  ,   0.05701782766715997  ,   0.06086059408579131   ,  0.06351788622624033   ,
    0.0648756961665491   ,  0.06487569616654912   ,  0.06351788622624033   ,  0.06086059408579132  ,   0.05701782766715998  ,
    0.05215410466516635  ,   0.04647700264810422   ,  0.04022777987048495   ,  0.03367050686658645  ,    0.0270802174602374  ,
    0.02073063182807278  ,   0.01488202164192106  ,  0.009769777358151061   ,  0.00559420103232427  ,  0.002511986542558811  ,
    0.0006297659406518865    ,    0
};

/*static double filter_coeff_two[] = {   //b= fir1(31, 0.02721088435, hann(32)) - 15 khz LPF for 2205000 Hz sample rate
    0 , 0.0005324855786808849 , 0.00218305759384411 , 0.004984952569753191 , 0.008906106000274685 , 0.01384819725874714 ,
    0.01964974236607389 , 0.02609314054247059 , 0.03291532251550697 , 0.03982140981843261 , 0.04650059106374654 ,
    0.05264326744191319 , 0.05795842626130267 , 0.06219017499587139 , 0.06513241132751785 , 0.06664071466586417 ,
    0.06664071466586417 , 0.06513241132751786 , 0.06219017499587139 , 0.0579584262613027 , 0.05264326744191319 , 0.04650059106374654 ,
    0.03982140981843263 , 0.032915322515507 , 0.02609314054247059 , 0.0196497423660739 , 0.01384819725874714 , 0.008906106000274687 , 
    0.004984952569753191 , 0.002183057593844111 , 0.0005324855786808907 , 0
};*/
static int filter_order = 32;

void unwrap_array(double *in, double *out, int len) {
    out[0] = in[0];
    for (int i = 1; i < len; i++) {
        double d = in[i] - in[i-1];
        d = d > M_PI ? d - 2 * M_PI : (d < -M_PI ? d + 2 * M_PI : d);
        out[i] = out[i-1] + d;
    }
}

//Device structure, should be initialize to NULL
lms_device_t* device = NULL;

int error()
{
    audioWriter.release();
    if (device != NULL)
        radioStreamer->closeDevice();
    exit(-1);
}

int main(int argc, char** argv)
{
	CommandLine commandLine(argc, argv);
    const int sampleCnt = 5000; 

#ifdef _linux_

	if (commandLine.audio == wav) {
		audioWriter = std::make_unique<WAVWriter>("./radio.wav", 44100, sampleCnt/(sample_rate/44100));
	}
	else {
		audioWriter = std::make_unique<AlsaStreamer>("hw:0,1,0", 44100, sampleCnt/(sample_rate/44100));
	}
#else
        audioWriter = std::make_unique<WAVWriter>("./radio.wav", 44100, sampleCnt/(sample_rate/44100));
#endif

    if (commandLine.radio == lime) {
        radioStreamer = std::make_unique<LimeStreamer>();
    }
    else {
        radioStreamer = std::make_unique<HackRFStreamer>();
    }
	

    //open radio device
    if (radioStreamer->openDevice() < 0)
        error();

    //Set center frequency to 800 MHz
    if (radioStreamer->setFrequency(89.7e6) < 0)
        error();

    if (radioStreamer->setSampleRate(sample_rate) < 0)
        error(); 

    if (radioStreamer->setFilterBandwidth(10e6) < 0)
    	error();

    auto my_server = std::make_unique<IQWebSocketServer>(8079, "^/socket/?$");
    if (commandLine.server == on) {
        my_server->run();
    }

    //Initialize stream
    if (radioStreamer->setupStream() < 0)
        error();

    //Initialize data buffers
    //complex samples per buffer
    const int myCnt = sampleCnt;
    const int downSampleCnt = (myCnt)/50;
    int16_t buffer[sampleCnt * 2 + 4 * filter_order + 2]; //buffer to hold complex values (2*samples))
    
    uint32_t audio_sample_count = 0;

    //Start streaming
    if (radioStreamer->startStream() < 0)
        error();

    auto t1 = chrono::high_resolution_clock::now();

    double bufferFiltered[sampleCnt * 2 + 2 * filter_order + 2];

    double wrapped[sampleCnt+filter_order + 1];
    double unwrapped[sampleCnt+filter_order + 1];
    double difference[myCnt+filter_order];
    double convert_to_pcm[myCnt];
    int16_t downsampled_audio[downSampleCnt];

    while ( ((commandLine.audio == alsa) ? true : false) || (chrono::high_resolution_clock::now() - t1 < chrono::seconds(12)) )
    {   
        //Receive samples
        int samplesRead = radioStreamer->receiveSamples(&(buffer[4 * filter_order + 2]), sampleCnt);

        if (samplesRead != sampleCnt) {
            printf("Unexpected sample read, here is read: %d\n", samplesRead);
            break;
        }
        else if (chrono::high_resolution_clock::now() - t1 < chrono::seconds(3)) {
            memcpy(buffer, &(buffer[10000]),2*(4 * filter_order + 2));
        }
        else {  

        memset(bufferFiltered, 0, sampleCnt * 2 + 2 * filter_order + 2 * sizeof(double));

        memset(wrapped, 0, sampleCnt+filter_order + 1 * sizeof(double));

        memset(unwrapped, 0, sampleCnt+filter_order + 1 * sizeof(double));

        memset(difference, 0, myCnt+filter_order * sizeof(double));

        memset(convert_to_pcm, 0, myCnt * sizeof(double));

        memset(downsampled_audio, 0, downSampleCnt * sizeof(int16_t));
        
        //printf("Received buffer with %d samples with first sample %lu microseconds\n", samplesRead, my_stream_data.timestamp);
        try {

	        for (int i = filter_order; i < (samplesRead+((4*filter_order + 2)/2)); ++i) {
	            for (int j = i-filter_order, x = 0; j < i; ++j, ++x) {
	                bufferFiltered[2*i-(2*filter_order)] = bufferFiltered[2*i-(2*filter_order)] + buffer[2*j] * filter_coeff_one[x];
	                bufferFiltered[2*i+1-(2*filter_order)] = bufferFiltered[2*i+1-(2*filter_order)] + buffer[2*j+1] * filter_coeff_one[x];
	            }
	        }

            if (commandLine.server == on) {
	           my_server->writeToBuffer((&(bufferFiltered[2 * filter_order + 2])));
            }

	        int samplesFiltered = samplesRead+filter_order + 1;

	        for (int i = 0; i < (samplesFiltered); ++i) {
	            double first = atan2( (double) bufferFiltered[2*i+1], (double) bufferFiltered[2*i]);
	            wrapped[i] = first;
	        }

	        unwrap_array(wrapped, unwrapped, (samplesFiltered));

	        for (int i = 0; i < (samplesFiltered-1); ++i) {
	            difference[i] = unwrapped[i+1] - unwrapped[i];
	        }

	        for (int i = filter_order; i < (samplesFiltered-1); ++i) {
	            for (int j = i-filter_order, x = 0; j < i; ++j, ++x) {
	                convert_to_pcm[i-filter_order] =  convert_to_pcm[i-filter_order] + difference[j] * filter_coeff_two[x];
	            }
	        }

	        for (int i = 0; i < ((samplesRead)/50); ++i) {
	            downsampled_audio[i] = (int16_t) ceil((convert_to_pcm[i * 50] - 0.0075) * (MAX_PCM ));
	            if (downsampled_audio[i] >= MAX_PCM) {
	                downsampled_audio[i] = MAX_PCM;
	            }
	        }

	        if (audioWriter->writeSamples((short *) downsampled_audio) < 0) {
	            break;
	        }

        }
        catch (int e) {
            printf("I got an exception: %d\n", e);
            break;
        }
            memcpy(buffer, &(buffer[10000]), 2*(4 * filter_order + 2));
        }
    
    }
    printf("STOP STREAM\n");
    //Stop streaming
    radioStreamer->stopStream();
    radioStreamer->destroyStream();

    //Close device
    radioStreamer->closeDevice();
    radioStreamer.release();

    audioWriter.release();

    return 0;
}