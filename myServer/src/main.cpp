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
#include <AlsaStreamer.hpp>
#include <RadioStreamer.hpp>
#include <LimeStreamer.hpp>
#include <IQWebSocketServer.hpp>
#include <CommandLine.hpp>

#define MAX_PCM 32767

using namespace std;

static int my_channel = 0;
static double mono_band = 15000;
static double sample_rate = 1102500;
static double max_difference = ((mono_band/sample_rate) * 2 * M_PI);
static std::unique_ptr<AudioSink> audioWriter;
static std::unique_ptr<RadioStreamer> radioStreamer;

static double filter_coeff_one[] = {   //fir1(31, 0.40, hann(32)) - 200 khz LPF for 1102500 Hz sample rate
    0 , -0.0001570993780150857 , 0.0003381932094938418 , 0.002275299083516856 , 0.00208612366364782 , -0.004208593800586378 ,
    -0.01072130155763298 , -0.003737411426019404 , 0.01745049151523847 , 0.02736395513988654 , -0.00129673748333856 , -0.05244322844396793 ,
    -0.05938547198644741 , 0.03522995587486178 , 0.2054523332385446 , 0.3417534923508179 , 0.3417534923508179 , 0.2054523332385446 ,
    0.03522995587486179 , -0.05938547198644741 , -0.05244322844396794 , -0.001296737483338559 , 0.02736395513988656 , 0.01745049151523848 ,
    -0.003737411426019404 , -0.01072130155763298 , -0.004208593800586378 , 0.00208612366364782 , 0.002275299083516856 , 0.0003381932094938418 , -0.0001570993780150874 , -0
};
static double filter_coeff_two[] = {   //b= fir1(31, 0.02721088435, hann(32)) - 15 khz LPF for 1102500 Hz sample rate
    0 , 0.0005324855786808849 , 0.00218305759384411 , 0.004984952569753191 , 0.008906106000274685 , 0.01384819725874714 ,
    0.01964974236607389 , 0.02609314054247059 , 0.03291532251550697 , 0.03982140981843261 , 0.04650059106374654 ,
    0.05264326744191319 , 0.05795842626130267 , 0.06219017499587139 , 0.06513241132751785 , 0.06664071466586417 ,
    0.06664071466586417 , 0.06513241132751786 , 0.06219017499587139 , 0.0579584262613027 , 0.05264326744191319 , 0.04650059106374654 ,
    0.03982140981843263 , 0.032915322515507 , 0.02609314054247059 , 0.0196497423660739 , 0.01384819725874714 , 0.008906106000274687 , 
    0.004984952569753191 , 0.002183057593844111 , 0.0005324855786808907 , 0
};
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

	if (commandLine.audio = wav) {
		audioWriter = std::make_unique<WAVWriter>();
	}
	else {
		audioWriter = std::make_unique<AlsaStreamer>();
	}

	radioStreamer = std::make_unique<LimeStreamer>();

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
    my_server->run();

    //Initialize stream
    if (radioStreamer->setupStream() < 0)
        error();

    //Initialize data buffers
    const int sampleCnt = 5000; //complex samples per buffer
    const int myCnt = sampleCnt;
    const int downSampleCnt = (myCnt)/25;
    int16_t buffer[sampleCnt * 2 + 4 * filter_order + 2]; //buffer to hold complex values (2*samples))
    
    uint32_t audio_sample_count = 0;

    //Start streaming
    if (radioStreamer->startStream() < 0)
        error();

    auto t1 = chrono::high_resolution_clock::now();

    while ( ((commandLine.audio == alsa) ? true : false) || (chrono::high_resolution_clock::now() - t1 < chrono::seconds(12)) )
    {   
        //Receive samples
        int samplesRead = radioStreamer->receiveSamples(&(buffer[4 * filter_order + 2]), sampleCnt);

        if (samplesRead != sampleCnt) {
            printf("Unexpected sample read\n");
            break;
        }
        else if (chrono::high_resolution_clock::now() - t1 < chrono::seconds(3)) {
            memcpy(buffer, &(buffer[10000]),2*(4 * filter_order + 2));
        }
        else {  

        double bufferFiltered[sampleCnt * 2 + 2 * filter_order + 2] = {0};

        double wrapped[sampleCnt+filter_order + 1] = {0};
        double unwrapped[sampleCnt+filter_order + 1] = {0};
        double difference[myCnt+filter_order] = {0};
        double convert_to_pcm[myCnt] = {0};
        int16_t downsampled_audio[downSampleCnt] = {0};
        
        //printf("Received buffer with %d samples with first sample %lu microseconds\n", samplesRead, my_stream_data.timestamp);
        try {

	        for (int i = filter_order; i < (samplesRead+((4*filter_order + 2)/2)); ++i) {
	            for (int j = i-filter_order, x = 0; j < i; ++j, ++x) {
	                bufferFiltered[2*i-(2*filter_order)] = bufferFiltered[2*i-(2*filter_order)] + buffer[2*j] * filter_coeff_one[x];
	                bufferFiltered[2*i+1-(2*filter_order)] = bufferFiltered[2*i+1-(2*filter_order)] + buffer[2*j+1] * filter_coeff_one[x];
	            }
	        }

	        my_server->writeToBuffer((&(bufferFiltered[2 * filter_order + 2])));

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

	        for (int i = 0; i < ((samplesRead)/25); ++i) {
	            downsampled_audio[i] = (int16_t) ceil((convert_to_pcm[i * 25] - 0.0075) * (MAX_PCM ));
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
            memcpy(buffer, &(buffer[10000]),2*(4 * filter_order + 2));
        }
    
    }
    //Stop streaming
    radioStreamer->stopStream();
    radioStreamer->destroyStream();

    //Close device
    radioStreamer->closeDevice();
    radioStreamer.release();

    audioWriter.release();

    return 0;
}