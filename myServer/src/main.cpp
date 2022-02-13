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
#include <fir1_1062_18140589569.hpp>
#include <thread>

#define MAX_PCM 32767

using namespace std;

static int my_channel = 0;
static double mono_band = 15000;
static double sample_rate = 2205000;
static int shifter = sample_rate/2; 
static double max_difference = ((mono_band/sample_rate) * 2 * M_PI);
static std::unique_ptr<AudioSink> audioWriter;
static std::unique_ptr<RadioStreamer> radioStreamer;


/*static double filter_coeff_one[] = {   //fir1(31, 0.20, hann(32)) - 200 khz LPF for 2205000 Hz sample rate

    0 ,  0.00007909990831961303 ,  0.0007979711131756353 ,  0.002287550802041069  , 0.00340186492199966  , 0.001992922319136589 , -0.003692853174558785 ,
    -0.013137559823147 , -0.02236573965074354 , -0.02449486179409259 , -0.01225211332268774  , 0.01843075743907188,
   0.06543407938285016 ,  0.1197465378212038  , 0.1677775120631539 ,  0.1959948319942774 ,  0.1959948319942774 ,  0.1677775120631539  ,
   0.1197465378212038 ,  0.06543407938285017  , 0.01843075743907188 , -0.01225211332268774 , -0.02449486179409261 , -0.02236573965074356 ,
  -0.013137559823147 , -0.003692853174558785  , 0.001992922319136589 ,  0.003401864921999662  , 0.002287550802041069 ,  0.0007979711131756353 ,  0.00007909990831961388,      0
};
*/


//octave:5> fir1(31,0.18140589569)
static double fir1_31_18140589569[] = { 
   0.0009770276662241166  ,  0.001828764662393893,    0.002702261934747691
    ,0.002966420113660258  ,  0.001436593359110279,   -0.002918947888720856
    ,-0.00994080766670102  ,  -0.01747103249692836,    -0.02135629892974791
    ,-0.01653538826051314  ,  0.001028630436679646,     0.03229171895800226
    , 0.07386627817995932  ,    0.1182546256077054,      0.1557091751030727
    ,  0.1771609792210559  ,    0.1771609792210559,      0.1557091751030727
    ,  0.1182546256077054  ,   0.07386627817995932,     0.03229171895800225
    ,0.001028630436679646  ,  -0.01653538826051315,    -0.02135629892974793
    ,-0.01747103249692836  ,  -0.00994080766670102,   -0.002918947888720857
    ,0.001436593359110279  ,  0.002966420113660259,     0.00270226193474769
    ,0.001828764662393895  , 0.0009770276662241166

};

/*static double filter_coeff_one[] = {   //fir1(31, 0.40, hann(32)) - 200 khz LPF for 1102500 Hz sample rate
    0 , -0.0001570993780150857 , 0.0003381932094938418 , 0.002275299083516856 , 0.00208612366364782 , -0.004208593800586378 ,
    -0.01072130155763298 , -0.003737411426019404 , 0.01745049151523847 , 0.02736395513988654 , -0.00129673748333856 , -0.05244322844396793 ,
    -0.05938547198644741 , 0.03522995587486178 , 0.2054523332385446 , 0.3417534923508179 , 0.3417534923508179 , 0.2054523332385446 ,
    0.03522995587486179 , -0.05938547198644741 , -0.05244322844396794 , -0.001296737483338559 , 0.02736395513988656 , 0.01745049151523848 ,
    -0.003737411426019404 , -0.01072130155763298 , -0.004208593800586378 , 0.00208612366364782 , 0.002275299083516856 , 0.0003381932094938418 , -0.0001570993780150874 , -0
};*/

static double fir1_31_01360544217[] = {   //b= fir1(31, 0.01360544217, hann(32)) - 15 khz LPF for  2205000 Hz sample rate

    0  , 0.0006297659406518796  ,  0.002511986542558811  ,   0.00559420103232427 ,   0.009769777358151056   ,  0.01488202164192106  ,   
    0.02073063182807278  ,   0.02708021746023739  ,   0.03367050686658642  ,   0.04022777987048493 ,
    0.04647700264810423  ,   0.05215410466516634  ,   0.05701782766715997  ,   0.06086059408579131   ,  0.06351788622624033   ,
    0.0648756961665491   ,  0.06487569616654912   ,  0.06351788622624033   ,  0.06086059408579132  ,   0.05701782766715998  ,
    0.05215410466516635  ,   0.04647700264810422   ,  0.04022777987048495   ,  0.03367050686658645  ,    0.0270802174602374  ,
    0.02073063182807278  ,   0.01488202164192106  ,  0.009769777358151061   ,  0.00559420103232427  ,  0.002511986542558811  ,
    0.0006297659406518865    ,    0
};

/*static double filter_coeff_two[] = {   //b= fir1(31, 0.02721088435, hann(32)) - 15 khz LPF for 1102500 Hz sample rate
    0 , 0.0005324855786808849 , 0.00218305759384411 , 0.004984952569753191 , 0.008906106000274685 , 0.01384819725874714 ,
    0.01964974236607389 , 0.02609314054247059 , 0.03291532251550697 , 0.03982140981843261 , 0.04650059106374654 ,
    0.05264326744191319 , 0.05795842626130267 , 0.06219017499587139 , 0.06513241132751785 , 0.06664071466586417 ,
    0.06664071466586417 , 0.06513241132751786 , 0.06219017499587139 , 0.0579584262613027 , 0.05264326744191319 , 0.04650059106374654 ,
    0.03982140981843263 , 0.032915322515507 , 0.02609314054247059 , 0.0196497423660739 , 0.01384819725874714 , 0.008906106000274687 , 
    0.004984952569753191 , 0.002183057593844111 , 0.0005324855786808907 , 0
};*/
static int filterLengthOne = 32;
static int filterLengthTwo = 32;

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
    audioWriter.reset();
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
    if (radioStreamer->setFrequency(90202500/*89.7e6*/) < 0)
        error();

    if (radioStreamer->setSampleRate(sample_rate) < 0)
        error(); 

    //if (radioStreamer->setFilterBandwidth(10e6) < 0)
    //	error();


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
    int16_t buffer[sampleCnt * 2 + 2 * filterLengthOne + 2 * filterLengthTwo + 2]; //buffer to hold complex values (2*samples))

    double bufferShifted[sampleCnt * 2 + 2 * filterLengthOne + 2 * filterLengthTwo + 2];
    
    uint32_t audio_sample_count = 0;

    //Start streaming
    if (radioStreamer->startStream() < 0)
        error();

    

    double bufferFiltered[sampleCnt * 2 + 2 * filterLengthTwo + 2];

    double wrapped[sampleCnt+filterLengthTwo + 1];
    double unwrapped[sampleCnt+filterLengthTwo + 1];
    double difference[myCnt+filterLengthTwo];
    double demodded[myCnt+filterLengthTwo];
    double convert_to_pcm[myCnt];
    int16_t downsampled_audio[downSampleCnt];

    int cosineIndexInphase = 0;
    int cosineIndexQuadrature = shifter/4;

    double max_Sample = 0;
    double min_Sample = 0;
    
    FILE * rawCSV;
    FILE * shiftedCSV;
    FILE * filteredCSV;
    int csvCounter = 0;
    int filteredCSVCounter = 0;
    printf("OPEN FILES\n");
    rawCSV = fopen("./raw.csv","w");
    shiftedCSV = fopen("./shifted.csv","w");
    filteredCSV = fopen("./filtered.csv","w");
    printf("OPENED FILES\n");

    double normalized_fir1_1062_18140589569[1063];
    double fmax = fir1_1062_18140589569[531];

    for (int i = 532; i < 1063; i++) {
        fmax += 2 * fir1_1062_18140589569[i];
    }

    double gain = 1.0 / fmax;

    for (int i = 0; i < 1063; i++) {
        normalized_fir1_1062_18140589569[i] = gain * fir1_1062_18140589569[i];
    }

    double normalized_fir1_31_18140589569[32];
    fmax = 0;

    for (int i = 0; i < 32; i++) {
        fmax += fir1_31_18140589569[i];
    }

    gain = 1.0 / fmax;

    for (int i = 0; i < 32; i++) {
        normalized_fir1_31_18140589569[i] = gain * fir1_31_18140589569[i];
    }

    double normalized_fir1_31_01360544217[32];
    fmax = 0;

    for (int i = 0; i < 32; i++) {
        fmax += fir1_31_01360544217[i];
    }

    gain = 1.0 / fmax;

    for (int i = 0; i < 32; i++) {
        normalized_fir1_31_01360544217[i] = gain * fir1_31_01360544217[i];
    }

    double * filter_coeff_one = normalized_fir1_31_18140589569;
    double * filter_coeff_two = normalized_fir1_31_01360544217;

    bool sinFlipper = false;

    auto t1 = chrono::high_resolution_clock::now();

    while ( ((commandLine.audio == alsa) ? true : false) || (chrono::high_resolution_clock::now() - t1 < chrono::seconds(12)) )
    {   
        //Receive samples
        int samplesRead = radioStreamer->receiveSamples(&(buffer[2 * filterLengthOne + 2 * filterLengthTwo + 2]), sampleCnt);

        //printf("Here is realCosine: %f\n", cos(2*M_PI*((double) cosineIndexInphase/shifter)));
        //printf("Here is imagCosine: %f\n", cos(2*M_PI*((double) cosineIndexQuadrature/shifter)));
        for (int i = 0; i < (samplesRead); ++i) {
            double doubleInphase = (double) cosineIndexInphase;
            double doubleQuadrature = (double) cosineIndexQuadrature;
            double realCosine = sinFlipper ? -1.0 : 1.0;//cos(2*M_PI*(doubleInphase/shifter));
            double imagCosine = 0.0;//cos(2*M_PI*(doubleQuadrature/shifter));
            double bufferReal = ((double) buffer[2 * filterLengthOne + 2 * filterLengthTwo + 2 + 2*i]) * (1.0/128.0);
            double bufferImag = ((double) buffer[2 * filterLengthOne + 2 * filterLengthTwo + 2 + 2*i+1]) * (1.0/128.0);
            /*
            if (csvCounter < 50000) {
                fprintf(rawCSV, "%d,%f,%f\n",csvCounter,bufferReal, bufferImag);
            }
            */
            //printf("Here is bufferReal: %f\n", bufferReal);
            //printf("Here is bufferImag: %f\n", bufferImag);

            bufferShifted[2 * filterLengthOne + 2 * filterLengthTwo + 2 + 2*i] = bufferReal * realCosine - bufferImag * imagCosine;
            bufferShifted[2 * filterLengthOne + 2 * filterLengthTwo + 2 + 2*i+1] = bufferReal * imagCosine + bufferImag * realCosine;
            
            /*if (csvCounter < 50000) {
                fprintf(shiftedCSV,"%d,%f,%f\n",csvCounter,bufferShifted[2 * filterLengthOne + 2 * filterLengthTwo + 2 + 2*i], bufferShifted[2 * filterLengthOne + 2 * filterLengthTwo + 2 + 2*i+1]);
                csvCounter++;
            }*/
            
            cosineIndexInphase = (cosineIndexInphase + 1) % shifter;
            cosineIndexQuadrature = (cosineIndexQuadrature + 1) % shifter;
            
            sinFlipper = !sinFlipper;
        }

        

        //printf("HERE IS BUFFERSHIFTED: %f and imag: %f\n", bufferShifted[4 * filter_order + 2], bufferShifted[4 * filter_order + 2+1]);

        if (samplesRead != sampleCnt) {
            printf("Unexpected sample read, here is read: %d\n", samplesRead);
            break;
        }
        else if (chrono::high_resolution_clock::now() - t1 < chrono::seconds(3)) {
            memcpy(buffer, &(buffer[10000]), (2 * filterLengthOne + 2 * filterLengthTwo + 2) * sizeof(int16_t));
            memcpy(bufferShifted, &(bufferShifted[10000]), (2 * filterLengthOne + 2 * filterLengthTwo + 2)* sizeof(int16_t));
        }
        else {  



        memset(bufferFiltered, 0, 2 * sampleCnt + 2 * filterLengthTwo + 2 * sizeof(double));

        memset(wrapped, 0, sampleCnt+filterLengthTwo + 1 * sizeof(double));

        memset(unwrapped, 0, sampleCnt+filterLengthTwo + 1 * sizeof(double));

        memset(difference, 0, myCnt+filterLengthTwo * sizeof(double));

        memset(demodded, 0, myCnt+filterLengthTwo * sizeof(double));

        memset(convert_to_pcm, 0, myCnt * sizeof(double));

        memset(downsampled_audio, 0, downSampleCnt * sizeof(int16_t));
        
        //printf("Received buffer with %d samples with first sample %lu microseconds\n", samplesRead, my_stream_data.timestamp);
        try {
            //printf("Here is buffer value: %d\n", buffer[0]);
            //printf("here is buffer imag value: %d\n", buffer[1]);
            //printf("Here is bufferShifted value: %lf\n", bufferShifted[0]);
	        for (int i = filterLengthOne; i < (samplesRead+((2 * filterLengthOne + 2 * filterLengthTwo + 2)/2)); ++i) {
	            for (int j = i-filterLengthOne, x = 0; j < i; ++j, ++x) {
	                bufferFiltered[2*i-(2*filterLengthOne)] = bufferFiltered[2*i-(2*filterLengthOne)] + bufferShifted[2*j] * filter_coeff_one[x];
	                bufferFiltered[2*i+1-(2*filterLengthOne)] = bufferFiltered[2*i+1-(2*filterLengthOne)] + bufferShifted[2*j+1] * filter_coeff_one[x];
                    /*if (filteredCSVCounter < 50000) {
                        fprintf(filteredCSV,"%d,%f,%f\n",filteredCSVCounter,bufferFiltered[2*i-(2*filterLengthOne)], bufferFiltered[2*i+1-(2*filterLengthOne)]);
                        filteredCSVCounter++;
                    }*/
	            }
	        }

            if (commandLine.server == on) {
	           my_server->writeToBuffer((&(bufferFiltered[2 * filterLengthTwo + 2])));
            }

	        int samplesFiltered = samplesRead+filterLengthTwo + 1;
            /*

	        for (int i = 0; i < (samplesFiltered); ++i) {
	            double first = atan2( (double) bufferFiltered[2*i+1], (double) bufferFiltered[2*i]);
	            wrapped[i] = first;
	        }

	        unwrap_array(wrapped, unwrapped, (samplesFiltered));

	        for (int i = 0; i < (samplesFiltered-1); ++i) {
	            difference[i] = unwrapped[i+1] - unwrapped[i];
	        }
            */
            
            for (int i = 0; i < (samplesFiltered-1); ++i) {
                double &undelayedReal = bufferFiltered[2*i];
                double &undelayedComplex = bufferFiltered[2*i+1];
                double &delayedReal = bufferFiltered[2*i+2];
                double &delayedComplex = bufferFiltered[2*i+3];
                double productReal = undelayedReal * delayedReal - (-1 * undelayedComplex) * delayedComplex;
                double productComplex = undelayedReal * delayedComplex + (-1 * undelayedComplex) * delayedReal;
                demodded[i] = atan2(productComplex, productReal);
            }
            
            //printf("Here demodded: %lf\n", demodded[0]);

	        for (int i = filterLengthTwo; i < (samplesFiltered-1); ++i) {
	            for (int j = i-filterLengthTwo, x = 0; j < i; ++j, ++x) {
	                convert_to_pcm[i-filterLengthTwo] =  convert_to_pcm[i-filterLengthTwo] + demodded[j] * filter_coeff_two[x];
                    if (convert_to_pcm[i-filterLengthTwo] > max_Sample) {
                        max_Sample = convert_to_pcm[i-filterLengthTwo];
                    }
                    if (convert_to_pcm[i-filterLengthTwo] < min_Sample) {
                        min_Sample = convert_to_pcm[i-filterLengthTwo];
                    }
	            }
	        }

            //printf("Here convert to pcm: %lf\n", convert_to_pcm[0]);

	        for (int i = 0; i < ((samplesRead)/50); ++i) {
	            downsampled_audio[i] = (int16_t) ceil((convert_to_pcm[i * 50] /*- 0.0075*/) * (MAX_PCM/100));
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
            memcpy(buffer, &(buffer[10000]), (2 * filterLengthOne + 2 * filterLengthTwo + 2) * sizeof(int16_t));
            memcpy(bufferShifted, &(bufferShifted[10000]), (2 * filterLengthOne + 2 * filterLengthTwo + 2) * sizeof(int16_t));
        }
    
    }
    printf("STOP STREAM\n");
    printf("here is max_Sample: %f\n", max_Sample);
    printf("here is min_Sample: %f\n", min_Sample);
    //Stop streaming
    radioStreamer->stopStream();
    radioStreamer->destroyStream();

    //Close device
    radioStreamer->closeDevice();
    radioStreamer.reset();

    audioWriter.reset();
    fclose(rawCSV);
    fclose(shiftedCSV);
    fclose(filteredCSV);
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);

    return 0;
}