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
#include <IQWebSocketServer.hpp>

#ifdef USE_GNU_PLOT
#include "gnuPlotPipe.h"
#endif

#define USE_WAV 0
#define STREAM_ALSA 1


#define MAX_PCM 32767


using namespace std;

/* LIME SDR Parameters */

static int my_channel = 0;
static double mono_band = 15000;
static double sample_rate = 1102500;
static double max_difference = ((mono_band/sample_rate) * 2 * M_PI);


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
    snd_pcm_close(alsa_handle);
    if (device != NULL)
        LMS_Close(device);
    exit(-1);
}

int main(int argc, char** argv)
{
	CommandLine(argc, argv);

	std::unique_ptr<AudioSink> audioWriter;

	if (CommmandLine.audio = wav) {
		audioWriter = std::make_unique<WAVWriter>();
	}
	else {
		audioWriter = std::make_unique<AlsaStreamer>();
	}

    //Find devices
    int n;
    lms_info_str_t list[8]; //should be large enough to hold all detected devices
    
    if ((n = LMS_GetDeviceList(list)) < 0) //NULL can be passed to only get number of devices
        error();

    cout << "Devices found: " << n << endl; //print number of devices
    if (n < 1)
        return -1;

    //open the first device
    if (LMS_Open(&device, list[0], NULL))
        error();

    //Initialize device with default configuration
    //Do not use if you want to keep existing configuration
    //Use LMS_LoadConfig(device, "/path/to/file.ini") to load config from INI
    if (LMS_Init(device) != 0)
        error();

    //Enable RX channel
    //Channels are numbered starting at 0
    if (LMS_EnableChannel(device, LMS_CH_RX, my_channel, true) != 0)
        error();

    //Set center frequency to 800 MHz
    if (LMS_SetLOFrequency(device, LMS_CH_RX, my_channel, 89.7e6) != 0)
        error();

    //Set sample rate to 8 MHz, ask to use 2x oversampling in RF
    //This set sampling rate for all channels
    if (LMS_SetSampleRate(device, sample_rate, 2) != 0)
        error();

    if (LMS_SetLPFBW(device, LMS_CH_RX, my_channel, 10e6) != 0)
        error(); 

    LMS_Calibrate(device, LMS_CH_RX, my_channel, 10e6, 0);

    lms_stream_meta_t my_stream_data;

    auto my_server = std::make_unique<IQWebSocketServer>(8079, "^/socket/?$");
    my_server->run();

    //Enable test signal generation
    //To receive data from RF, remove this line or change signal to LMS_TESTSIG_NONE
    //if (LMS_SetTestSignal(device, LMS_CH_RX, 0, LMS_TESTSIG_NCODIV8, 0, 0) != 0)
        //error();

    //Streaming Setup

    //Initialize stream
    lms_stream_t streamId; //stream structure
    streamId.channel = my_channel; //channel number
    streamId.fifoSize = 1024 * 1024; //fifo size in samples
    streamId.throughputVsLatency = 1.0; //optimize for max throughput
    streamId.isTx = false; //RX channel
    streamId.dataFmt = lms_stream_t::LMS_FMT_I12; //12-bit integers
    if (LMS_SetupStream(device, &streamId) != 0)
        error();

    //Initialize data buffers
    const int sampleCnt = 5000; //complex samples per buffer
    const int myCnt = sampleCnt;
    const int downSampleCnt = (myCnt)/25;
    int16_t buffer[sampleCnt * 2 + 4 * filter_order + 2]; //buffer to hold complex values (2*samples))
    
    uint32_t audio_sample_count = 0;

    //Start streaming
    LMS_StartStream(&streamId);

    double my_max = 0;

    //Streaming
    /*
    GNUPlotPipe gp;
    gp.write("set size square\n set xrange[-2050:2050]\n set yrange[-2050:2050]\n");
    //gp.write("set size square\n set xrange[-100:100]\n set yrange[-100:100]\n");
    */

    auto t1 = chrono::high_resolution_clock::now();

    while ( ((CommandLine.audio == alsa) ? true : false) || (chrono::high_resolution_clock::now() - t1 < chrono::seconds(12)) )
    {
        
        
        
        //Receive samples
        int samplesRead = LMS_RecvStream(&streamId, &(buffer[4 * filter_order + 2]), sampleCnt, &my_stream_data, 1000);
        int super_max = 0;
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
	            /*bufferFiltered[2*i-34] = buffer[2*i];
	            bufferFiltered[2*i-34+1] = buffer[2*i+1];*/
	        }


	        //my_server->writeToBuffer(*reinterpret_cast<double(*)[10000]>(&(bufferFiltered[2 * filter_order + 2])));

	        my_server->writeToBuffer((&(bufferFiltered[2 * filter_order + 2])));

	        int samplesFiltered = samplesRead+filter_order + 1;

	        for (int i = 0; i < (samplesFiltered); ++i) {
	            double first = atan2( (double) bufferFiltered[2*i+1], (double) bufferFiltered[2*i]);
	            //double second = atan2((double) buffer[2*i+3], (double) buffer[2*i+2]);
	            //double rate = second - first;
	            wrapped[i] = first;
	        }

	        unwrap_array(wrapped, unwrapped, (samplesFiltered));

	        for (int i = 0; i < (samplesFiltered-1); ++i) {
	            difference[i] = unwrapped[i+1] - unwrapped[i];
	            if (difference[i] > ((200e3/sample_rate) * 2 * M_PI)) {
	                //difference[i] = ((mono_band/sample_rate) * 2 * M_PI);
	                super_max++;
	            }
	        }


	        for (int i = filter_order; i < (samplesFiltered-1); ++i) {
	            /*convert_to_pcm[i] = (int) ceil(difference[i] * (MAX_PCM /  max_difference));
	            if (convert_to_pcm[i] > MAX_PCM) {
	                printf("I got a maxed out sample\n");
	                convert_to_pcm[i] = MAX_PCM;
	            }*/
	            for (int j = i-filter_order, x = 0; j < i; ++j, ++x) {
	                convert_to_pcm[i-filter_order] =  convert_to_pcm[i-filter_order] + difference[j] * filter_coeff_two[x];
	            }
	            if (convert_to_pcm[i-filter_order]>my_max) {
	                my_max = convert_to_pcm[i-filter_order];
	            }
	        }

	        for (int i = 0; i < ((samplesRead)/25); ++i) {
	            //downsampled_audio[i] = convert_to_pcm[i*25];
	            downsampled_audio[i] = (int16_t) ceil((convert_to_pcm[i * 25] - 0.0075) * (MAX_PCM ));
	            //downsampled_audio[i] = ceil((convert_to_pcm[i * 25]) * (MAX_PCM/3));
	            //downsampled_audio[i] = (uint16_t) convert_to_pcm[i * 25] * 20;
	            //if (convert_to_pcm[i * 25] * 20 > MAX_PCM) {
	            if (downsampled_audio[i] >= MAX_PCM) {
	                printf("I got a maxed out sample: %d from %f\n", downsampled_audio[i], convert_to_pcm[i * 25]);
	                downsampled_audio[i] = MAX_PCM;
	            }

	        }
	        fwrite(downsampled_audio, 2, ((samplesRead)/25), fp);

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

    /*
        //Plot samples
        gp.write("plot '-' with points\n");
        for (int j = 0; j < samplesRead; ++j)
            gp.writef("%i %i\n", buffer[2 * j], buffer[2 * j + 1]);
        gp.write("e\n");
        gp.flush();
    */
    
    
    }
    //Stop streaming
    LMS_StopStream(&streamId); //stream is stopped but can be started again with LMS_StartStream()
    LMS_DestroyStream(device, &streamId); //stream is deallocated and can no longer be used

    //Close device
    LMS_Close(device);

    audioWriter.release();
    
    

    return 0;
}