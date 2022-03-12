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
#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#endif
#include <RadioStreamer.hpp>
#include <LimeStreamer.hpp>
#include <HackRFStreamer.hpp>
#include <IQWebSocketServer.hpp>
#include <CommandLine.hpp>
#include <filters/filters.hpp>

#include <thread>
#include <gnuradio/filter/firdes.h>
#include <gnuradio/filter/fir_filter.h>
#include <sin1024.hpp>

#define MAX_PCM 32767

using namespace std;

static int my_channel = 0;
static double mono_band = 15000;
static double tunedFrequnecy = 93.3e6;
static double sample_rate = 17640000;
static int sample_rate_int = sample_rate;
static double frequencyShift = 2000000; 
static int frequencyShiftInt = frequencyShift;
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

#define TAN_MAP_RES 0.003921569 /* (smallest non-zero value in table) */
#define RAD_PER_DEG 0.017453293
#define TAN_MAP_SIZE 255

static float fast_atan_table[257] = {
    0.000000e+00, 3.921549e-03, 7.842976e-03, 1.176416e-02, 1.568499e-02, 1.960533e-02,
    2.352507e-02, 2.744409e-02, 3.136226e-02, 3.527947e-02, 3.919560e-02, 4.311053e-02,
    4.702413e-02, 5.093629e-02, 5.484690e-02, 5.875582e-02, 6.266295e-02, 6.656816e-02,
    7.047134e-02, 7.437238e-02, 7.827114e-02, 8.216752e-02, 8.606141e-02, 8.995267e-02,
    9.384121e-02, 9.772691e-02, 1.016096e-01, 1.054893e-01, 1.093658e-01, 1.132390e-01,
    1.171087e-01, 1.209750e-01, 1.248376e-01, 1.286965e-01, 1.325515e-01, 1.364026e-01,
    1.402496e-01, 1.440924e-01, 1.479310e-01, 1.517652e-01, 1.555948e-01, 1.594199e-01,
    1.632403e-01, 1.670559e-01, 1.708665e-01, 1.746722e-01, 1.784728e-01, 1.822681e-01,
    1.860582e-01, 1.898428e-01, 1.936220e-01, 1.973956e-01, 2.011634e-01, 2.049255e-01,
    2.086818e-01, 2.124320e-01, 2.161762e-01, 2.199143e-01, 2.236461e-01, 2.273716e-01,
    2.310907e-01, 2.348033e-01, 2.385093e-01, 2.422086e-01, 2.459012e-01, 2.495869e-01,
    2.532658e-01, 2.569376e-01, 2.606024e-01, 2.642600e-01, 2.679104e-01, 2.715535e-01,
    2.751892e-01, 2.788175e-01, 2.824383e-01, 2.860514e-01, 2.896569e-01, 2.932547e-01,
    2.968447e-01, 3.004268e-01, 3.040009e-01, 3.075671e-01, 3.111252e-01, 3.146752e-01,
    3.182170e-01, 3.217506e-01, 3.252758e-01, 3.287927e-01, 3.323012e-01, 3.358012e-01,
    3.392926e-01, 3.427755e-01, 3.462497e-01, 3.497153e-01, 3.531721e-01, 3.566201e-01,
    3.600593e-01, 3.634896e-01, 3.669110e-01, 3.703234e-01, 3.737268e-01, 3.771211e-01,
    3.805064e-01, 3.838825e-01, 3.872494e-01, 3.906070e-01, 3.939555e-01, 3.972946e-01,
    4.006244e-01, 4.039448e-01, 4.072558e-01, 4.105574e-01, 4.138496e-01, 4.171322e-01,
    4.204054e-01, 4.236689e-01, 4.269229e-01, 4.301673e-01, 4.334021e-01, 4.366272e-01,
    4.398426e-01, 4.430483e-01, 4.462443e-01, 4.494306e-01, 4.526070e-01, 4.557738e-01,
    4.589307e-01, 4.620778e-01, 4.652150e-01, 4.683424e-01, 4.714600e-01, 4.745676e-01,
    4.776654e-01, 4.807532e-01, 4.838312e-01, 4.868992e-01, 4.899573e-01, 4.930055e-01,
    4.960437e-01, 4.990719e-01, 5.020902e-01, 5.050985e-01, 5.080968e-01, 5.110852e-01,
    5.140636e-01, 5.170320e-01, 5.199904e-01, 5.229388e-01, 5.258772e-01, 5.288056e-01,
    5.317241e-01, 5.346325e-01, 5.375310e-01, 5.404195e-01, 5.432980e-01, 5.461666e-01,
    5.490251e-01, 5.518738e-01, 5.547124e-01, 5.575411e-01, 5.603599e-01, 5.631687e-01,
    5.659676e-01, 5.687566e-01, 5.715357e-01, 5.743048e-01, 5.770641e-01, 5.798135e-01,
    5.825531e-01, 5.852828e-01, 5.880026e-01, 5.907126e-01, 5.934128e-01, 5.961032e-01,
    5.987839e-01, 6.014547e-01, 6.041158e-01, 6.067672e-01, 6.094088e-01, 6.120407e-01,
    6.146630e-01, 6.172755e-01, 6.198784e-01, 6.224717e-01, 6.250554e-01, 6.276294e-01,
    6.301939e-01, 6.327488e-01, 6.352942e-01, 6.378301e-01, 6.403565e-01, 6.428734e-01,
    6.453808e-01, 6.478788e-01, 6.503674e-01, 6.528466e-01, 6.553165e-01, 6.577770e-01,
    6.602282e-01, 6.626701e-01, 6.651027e-01, 6.675261e-01, 6.699402e-01, 6.723452e-01,
    6.747409e-01, 6.771276e-01, 6.795051e-01, 6.818735e-01, 6.842328e-01, 6.865831e-01,
    6.889244e-01, 6.912567e-01, 6.935800e-01, 6.958943e-01, 6.981998e-01, 7.004964e-01,
    7.027841e-01, 7.050630e-01, 7.073330e-01, 7.095943e-01, 7.118469e-01, 7.140907e-01,
    7.163258e-01, 7.185523e-01, 7.207701e-01, 7.229794e-01, 7.251800e-01, 7.273721e-01,
    7.295557e-01, 7.317307e-01, 7.338974e-01, 7.360555e-01, 7.382053e-01, 7.403467e-01,
    7.424797e-01, 7.446045e-01, 7.467209e-01, 7.488291e-01, 7.509291e-01, 7.530208e-01,
    7.551044e-01, 7.571798e-01, 7.592472e-01, 7.613064e-01, 7.633576e-01, 7.654008e-01,
    7.674360e-01, 7.694633e-01, 7.714826e-01, 7.734940e-01, 7.754975e-01, 7.774932e-01,
    7.794811e-01, 7.814612e-01, 7.834335e-01, 7.853982e-01, 7.853982e-01
};


/*****************************************************************************
 Function: Arc tangent

 Syntax: angle = fast_atan2(y, x);
 float y y component of input vector
 float x x component of input vector
 float angle angle of vector (x, y) in radians

 Description: This function calculates the angle of the vector (x,y)
 based on a table lookup and linear interpolation. The table uses a
 256 point table covering -45 to +45 degrees and uses symmetry to
 determine the final angle value in the range of -180 to 180
 degrees. Note that this function uses the small angle approximation
 for values close to zero. This routine calculates the arc tangent
 with an average error of +/- 3.56e-5 degrees (6.21e-7 radians).
*****************************************************************************/

float fast_atan2f(float y, float x)
{
    float x_abs, y_abs, z;
    float alpha, angle, base_angle;
    int index;

    /* normalize to +/- 45 degree range */
    y_abs = fabsf(y);
    x_abs = fabsf(x);
    /* don't divide by zero! */
    if (!((y_abs > 0.0f) || (x_abs > 0.0f)))
        return 0.0;

    if (y_abs < x_abs)
        z = y_abs / x_abs;
    else
        z = x_abs / y_abs;

    /* when ratio approaches the table resolution, the angle is */
    /* best approximated with the argument itself... */
    if (z < TAN_MAP_RES)
        base_angle = z;
    else {
        /* find index and interpolation value */
        alpha = z * (float)TAN_MAP_SIZE;
        index = ((int)alpha) & 0xff;
        alpha -= (float)index;
        /* determine base angle based on quadrant and */
        /* add or subtract table value from base angle based on quadrant */
        base_angle = fast_atan_table[index];
        base_angle += (fast_atan_table[index + 1] - fast_atan_table[index]) * alpha;
    }

    if (x_abs > y_abs) { /* -45 -> 45 or 135 -> 225 */
        if (x >= 0.0) {  /* -45 -> 45 */
            if (y >= 0.0)
                angle = base_angle; /* 0 -> 45, angle OK */
            else
                angle = -base_angle; /* -45 -> 0, angle = -angle */
        } else {                     /* 135 -> 180 or 180 -> -135 */
            angle = 3.14159265358979323846;
            if (y >= 0.0)
                angle -= base_angle; /* 135 -> 180, angle = 180 - angle */
            else
                angle = base_angle - angle; /* 180 -> -135, angle = angle - 180 */
        }
    } else {            /* 45 -> 135 or -135 -> -45 */
        if (y >= 0.0) { /* 45 -> 135 */
            angle = 1.57079632679489661923;
            if (x >= 0.0)
                angle -= base_angle; /* 45 -> 90, angle = 90 - angle */
            else
                angle += base_angle; /* 90 -> 135, angle = 90 + angle */
        } else {                     /* -135 -> -45 */
            angle = -1.57079632679489661923;
            if (x >= 0.0)
                angle += base_angle; /* -90 -> -45, angle = -90 + angle */
            else
                angle -= base_angle; /* -135 -> -90, angle = -90 - angle */
        }
    }

#ifdef ZERO_TO_TWOPI
    if (angle < 0)
        return (angle + TWOPI);
    else
        return (angle);
#else
    return (angle);
#endif
}


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
    const int sampleCnt = 10400; 

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
    if (radioStreamer->setFrequency(tunedFrequnecy/*89.7e6*/) < 0)
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

    int indexSignal = 0;
    float sineSignal = 0;

    double max_Sample = 0;
    double min_Sample = 0;
    
    FILE * rawCSV;
    FILE * shiftedCSV;
    FILE * filteredCSV;
    FILE * sineCSV;
    FILE * rawFFT;
    FILE * shiftedFFT;
    int csvCounter = 0;
    int filteredCSVCounter = 0;
    int sineCSVCounter = 0;
    bool rawFFTWritten = false;
    bool shiftedFFTWritten = false;
    printf("OPEN FILES\n");
    rawCSV = fopen("./raw.csv","w");
    shiftedCSV = fopen("./shifted.csv","w");
    filteredCSV = fopen("./filtered.csv","w");
    sineCSV = fopen("./sine.csv", "w");
    rawFFT = fopen("./rawFFT.csv","w");
    shiftedFFT = fopen("./shiftedFFT.csv","w");
    printf("OPENED FILES\n");


    /*
    double normalized_fir1_1062_18140589569[1063];
    double fmax = fir1_1062_18140589569[531];

    for (int i = 532; i < 1063; i++) {
        fmax += 2 * fir1_1062_18140589569[i];
    }

    double gain = 1.0 / fmax;

    for (int i = 0; i < 1063; i++) {
        normalized_fir1_1062_18140589569[i] = gain * fir1_1062_18140589569[i];
    }
    */


    auto normalizedFilterOne = normalized_fir1_32_02267573696();
    auto normalizedFilterTwo = normalized_fir1_32_00170068027();
    const double * filter_coeff_one = normalizedFilterOne.data();
    const double * filter_coeff_two = normalizedFilterTwo.data();
    const int filterLengthOne = 33;
    const int filterLengthTwo = 33;

    const int stageOneDecimation = 40;
    const int stageTwoDecimation = 10;

    bool sinFlipper = false;

    //Initialize data buffers
    //complex samples per buffer
    const int afterFirstFilterCnt = sampleCnt/stageOneDecimation;
    const int downSampleCnt = (sampleCnt/stageOneDecimation)/stageTwoDecimation;
    int bufferCount = sampleCnt * 2 + 2 * filterLengthOne + 2 * filterLengthTwo * stageOneDecimation;
    int bufferOffset = 2 * filterLengthOne + 2 * filterLengthTwo * stageOneDecimation;
    int16_t buffer[bufferCount]; //buffer to hold complex values (2*samples))
    printf("Here is last index: %d\n", sampleCnt * 2 + 2 * filterLengthOne + 2 * filterLengthTwo * stageOneDecimation);

    

    double bufferShifted[bufferCount];
    
    uint32_t audio_sample_count = 0;

    fftw_complex *fft_in;
    fftw_complex *fft_out;
    fftw_plan fft_plan;

    fft_in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 2048);
    fft_out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * 2048);
    fft_plan = fft_plan = fftw_plan_dft_1d(2048, fft_in, fft_out, FFTW_FORWARD, FFTW_MEASURE);

    //Start streaming
    if (radioStreamer->startStream() < 0)
        error();

    double bufferFiltered[sampleCnt * 2 + 2 * filterLengthTwo * stageOneDecimation + 2 * stageOneDecimation];
    double decimatedFilter[afterFirstFilterCnt * 2 + filterLengthTwo * 2 + 1 * 2];
    double realDecimatedFilter[afterFirstFilterCnt + filterLengthTwo + 1];
    double imagDecimatedFilter[afterFirstFilterCnt + filterLengthTwo + 1];
    double wrapped[afterFirstFilterCnt + filterLengthTwo + 1];
    double unwrapped[afterFirstFilterCnt + filterLengthTwo + 1];
    double difference[afterFirstFilterCnt + filterLengthTwo];
    double demodded[afterFirstFilterCnt + filterLengthTwo];
    double convert_to_pcm[afterFirstFilterCnt];
    double compare_to_pcm[afterFirstFilterCnt];
    int16_t downsampled_audio[downSampleCnt];

    const float shiftFactor = frequencyShift/sample_rate;
    printf("Here is shiftfactor: %f\n", shiftFactor);

    auto t1 = chrono::high_resolution_clock::now();
    while ( ((commandLine.audio == alsa) ? true : false) || (chrono::high_resolution_clock::now() - t1 < chrono::seconds(12)) )
    {   
        //Receive samples
        int samplesRead = radioStreamer->receiveSamples(&(buffer[bufferOffset]), sampleCnt);

        //printf("Here is realCosine: %f\n", cos(2*M_PI*((double) cosineIndexInphase/shifter)));
        //printf("Here is imagCosine: %f\n", cos(2*M_PI*((double) cosineIndexQuadrature/shifter)));
        /*
        if (!rawFFTWritten) {
            for (int i = 0; i<2048; i++) {
                fft_in[i][0] = buffer[bufferOffset + 2*i] * hann2048[i];
                fft_in[i][1] = 1 * buffer[bufferOffset + 2*i + 1] * hann2048[i];

            }
            fftw_execute(fft_plan);
            for (int i = 0; i<2048; i++) {
                std::complex<double> bin(fft_out[i][0], fft_out[i][1]);
                double modulus = std::abs(bin);
                fprintf(rawFFT, "%d,%f,\n",i,modulus);
            }
            rawFFTWritten = true;
        }
        */
        for (int i = 0; i < (samplesRead); ++i) {
            float junk;
            sineSignal = modf( sineSignal, &junk); //remainder(((float) indexSignal) * shiftFactor, 1);
            int inPhaseIndex =  1024 * sineSignal;
            int quadratureIndex = ((int)(1024 * sineSignal) + 256) % 1024;
            if (inPhaseIndex >= 1024) {
                inPhaseIndex = 1023;
            }
            if (quadratureIndex >= 1024) {
                quadratureIndex = 1023;
            }

            //double realCosine = sinFlipper ? -1.0 : 1.0;
            const float & imagCosine = sin1024[inPhaseIndex];
            //double realCosine = sin(2 * M_PI * frequencyShift/sample_rate * indexSignal);
            //double imagCosine = 0.0;
            const float & realCosine = sin1024[quadratureIndex];
            //double imagCosine = cos(2 * M_PI * frequencyShift/sample_rate * indexSignal);
            /*if (sineCSVCounter < 50000) {
                fprintf(sineCSV, "%d,%f,%f\n",sineCSVCounter,realCosine, imagCosine);
                sineCSVCounter++;
            }*/
            //printf("BEFORE BUFFER ACESS\n");
            float  bufferReal = ((float) buffer[bufferOffset + 2*i])/128.0;
            float  bufferImag = ((float) buffer[bufferOffset + 2*i+1])/128.0;
            //printf("AFTER BUFFER ACESS\n");
            
            /*if (csvCounter < 50000) {
                fprintf(rawCSV, "%d,%f,%f\n",csvCounter,bufferReal, bufferImag);
            }*/
            
            //printf("Here is bufferReal: %f\n", bufferReal);
            //printf("Here is bufferImag: %f\n", bufferImag);

            bufferShifted[bufferOffset + 2*i] = bufferReal * realCosine - bufferImag * imagCosine;
            bufferShifted[bufferOffset + 2*i+1] = bufferReal * imagCosine + bufferImag * realCosine;
            
            /*if (csvCounter < 50000) {
                fprintf(shiftedCSV,"%d,%f,%f\n",csvCounter,bufferShifted[2 * filterLengthOne + 2 * filterLengthTwo + 2 + 2*i], bufferShifted[2 * filterLengthOne + 2 * filterLengthTwo + 2 + 2*i+1]);
                csvCounter++;
            }*/
            
            //cosineIndexInphase = (cosineIndexInphase + 1) % sample_rate;
            //cosineIndexQuadrature = (cosineIndexQuadrature + 1) % sample_rate;
            //sinFlipper = !sinFlipper;
            //indexSignal = (indexSignal + 1) % sample_rate_int;
            sineSignal += shiftFactor;
        }
/*
        if (!shiftedFFTWritten) {
            for (int i = 0; i<2048; i++) {
                fft_in[i][0] = bufferShifted[bufferOffset + 2*i] * hann2048[i];
                fft_in[i][1] = 1 * bufferShifted[bufferOffset + 2*i + 1]  * hann2048[i];
            }
            fftw_execute(fft_plan);
            for (int i = 0; i<2048; i++) {
                std::complex<double> bin(fft_out[i][0], fft_out[i][1]);
                double modulus = std::abs(bin);
                fprintf(shiftedFFT, "%d,%f,\n",i,modulus);
            }
            shiftedFFTWritten = true;
        }
*/     
        //printf("HERE IS BUFFERSHIFTED: %f and imag: %f\n", bufferShifted[4 * filter_order + 2], bufferShifted[4 * filter_order + 2+1]);

        if (samplesRead != sampleCnt) {
            printf("Unexpected sample read, here is read: %d\n", samplesRead);
            break;
        }
        else if (chrono::high_resolution_clock::now() - t1 < chrono::seconds(3)) {
            memcpy(buffer, &(buffer[sampleCnt * 2]), (bufferOffset) * sizeof(int16_t));
            memcpy(bufferShifted, &(bufferShifted[sampleCnt * 2]), (bufferOffset)* sizeof(double));
        }
        else {  
        //printf("START REAL LOOP\n");
        memset(bufferFiltered, 0, (sampleCnt * 2 + 2 * filterLengthTwo * stageOneDecimation + 2 * stageOneDecimation) * sizeof(double));

        memset(decimatedFilter, 0, (afterFirstFilterCnt * 2 + filterLengthTwo * 2 + 2) * sizeof(double));

        memset(realDecimatedFilter, 0, (afterFirstFilterCnt  + filterLengthTwo + 1) * sizeof(double));

        memset(imagDecimatedFilter, 0, (afterFirstFilterCnt + filterLengthTwo + 1) * sizeof(double));

        memset(wrapped, 0, (afterFirstFilterCnt+filterLengthTwo + 1) * sizeof(double));

        memset(unwrapped, 0, (afterFirstFilterCnt+filterLengthTwo + 1) * sizeof(double));

        memset(difference, 0, (afterFirstFilterCnt+filterLengthTwo) * sizeof(double));

        memset(demodded, 0, (afterFirstFilterCnt+filterLengthTwo) * sizeof(double));

        memset(convert_to_pcm, 0, afterFirstFilterCnt * sizeof(double));

        memset(downsampled_audio, 0, downSampleCnt * sizeof(int16_t));
    
        //printf("Received buffer with %d samples with first sample %lu microseconds\n", samplesRead, my_stream_data.timestamp);
        try {
            //printf("Here is buffer value: %d\n", buffer[0]);
            //printf("here is buffer imag value: %d\n", buffer[1]);
            //printf("Here is bufferShifted value: %lf\n", bufferShifted[0]);
/*
	        for (int i = filterLengthOne; i < (samplesRead+((2 * filterLengthOne + 2 * filterLengthTwo * stageOneDecimation + 2 * stageOneDecimation)/2)); ++i) {
#ifdef __APPLE__
                vDSP_dotprD(&bufferShifted[2*i-2*filterLengthOne], 2, filter_coeff_one, 1, &bufferFiltered[2*i-(2*filterLengthOne)], filterLengthOne);
                vDSP_dotprD(&bufferShifted[2*i + 1 -2*filterLengthOne], 2, filter_coeff_one, 1, &bufferFiltered[2*i + 1-(2*filterLengthOne)], filterLengthOne);
#else
	            for (int j = i-filterLengthOne, x = 0; j < i; ++j, ++x) {
	                bufferFiltered[2*i-(2*filterLengthOne)] = bufferFiltered[2*i-(2*filterLengthOne)] + bufferShifted[2*j] * filter_coeff_one[x];
	                bufferFiltered[2*i+1-(2*filterLengthOne)] = bufferFiltered[2*i+1-(2*filterLengthOne)] + bufferShifted[2*j+1] * filter_coeff_one[x];
                    
	            }
#endif
                filteredCSVCounter++;
	        }

for (int i = 0, j = 0;
                i < (samplesRead * 2 + (2 * filterLengthTwo * stageOneDecimation + 2 * stageOneDecimation)), j < afterFirstFilterCnt * 2 + filterLengthTwo * 2 + 2;
                i = i + stageOneDecimation, j++) {
                decimatedFilter[j] = bufferFiltered[i];
            }
*/
/*
#ifdef __APPLE__
            vDSP_desampD(bufferShifted, 2*stageOneDecimation, filter_coeff_one, realDecimatedFilter, afterFirstFilterCnt  + filterLengthTwo + 1, filterLengthOne);
            vDSP_desampD(&bufferShifted[1], 2*stageOneDecimation, filter_coeff_one, imagDecimatedFilter, afterFirstFilterCnt  + filterLengthTwo + 1, filterLengthOne);
#else
            
            for (int n = 0; n < afterFirstFilterCnt  + filterLengthTwo + 1; n++) {
                double realSum = 0;
                double imagSum = 0;
                for (p = 0;  p < filterLengthOne; ++p) {
                    realSum += bufferShifted[n * stageOneDecimation * 2 + 2*p] * filter_coeff_one[p];
                    imagSum =+ bufferShifted[n * stageOneDecimation * 2 + 1 + 2*p] * filter_coeff_one[p];
                }
                decimatedFilter[2*n] = realSum;
                decimatedFilter[2*n+1] = imagSum;
            }
#endif
*/
            for (int n = 0; n < afterFirstFilterCnt  + filterLengthTwo + 1; n++) {
#ifdef __APPLE__
                vDSP_dotprD(&bufferShifted[n * stageOneDecimation * 2], 2, filter_coeff_one, 1, &realDecimatedFilter[n], filterLengthOne);
                vDSP_dotprD(&bufferShifted[n * stageOneDecimation * 2 + 1] , 2, filter_coeff_one, 1, &imagDecimatedFilter[n], filterLengthOne);
#else
                double realSum = 0;
                double imagSum = 0;
                for (int p = 0;  p < filterLengthOne; ++p) {
                    realSum += bufferShifted[n * stageOneDecimation * 2 + 2*p] * filter_coeff_one[p];
                    imagSum += bufferShifted[n * stageOneDecimation * 2 + 1 + 2*p] * filter_coeff_one[p];
                }
                decimatedFilter[2*n] = realSum;
                decimatedFilter[2*n+1] = imagSum;
#endif
            }
            if (commandLine.server == on) {
	           my_server->writeToBuffer((&(bufferFiltered[2 * filterLengthTwo + 2])));
            }

	        int samplesFiltered = afterFirstFilterCnt+filterLengthTwo + 1;
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
#ifdef __APPLE__
            for (int i = 0; i < (samplesFiltered-1); ++i) {
                /*if (filteredCSVCounter < 1000) {
                    fprintf(filteredCSV, "%d,%f,%f\n",filteredCSVCounter,realDecimatedFilter[i], imagDecimatedFilter[i]);
                    filteredCSVCounter++;
                }*/
                double &undelayedReal = realDecimatedFilter[i];
                double &undelayedComplex = imagDecimatedFilter[i];
                double &delayedReal = realDecimatedFilter[i+1];
                double &delayedComplex = imagDecimatedFilter[i+1];
                double productReal = undelayedReal * delayedReal - ((-undelayedComplex) * delayedComplex);
                double productComplex = undelayedReal * delayedComplex + ((-undelayedComplex) * delayedReal);
                demodded[i] = (double) fast_atan2f((float)productComplex, (float)productReal);
            }
#else       
            for (int i = 0; i < (samplesFiltered-1); ++i) {
                double &undelayedReal = decimatedFilter[2*i];
                double &undelayedComplex = decimatedFilter[2*i+1];
                double &delayedReal = decimatedFilter[2*i+2];
                double &delayedComplex = decimatedFilter[2*i+3];
                double productReal = undelayedReal * delayedReal - (-1 * undelayedComplex) * delayedComplex;
                double productComplex = undelayedReal * delayedComplex + (-1 * undelayedComplex) * delayedReal;
                demodded[i] = (double) fast_atan2f((float)productComplex, (float)productReal);
            }
#endif
            //printf("Here demodded: %lf\n", demodded[0]);

	        for (int i = filterLengthTwo; i < (samplesFiltered-1); ++i) {
#ifdef __APPLE__
                vDSP_dotprD(&demodded[i-filterLengthTwo],1,filter_coeff_two, 1, &convert_to_pcm[i-filterLengthTwo], filterLengthTwo);
#else
	            for (int j = i-filterLengthTwo, x = 0; j < i; ++j, ++x) {
	                convert_to_pcm[i-filterLengthTwo] =  convert_to_pcm[i-filterLengthTwo] + demodded[j] * filter_coeff_two[x];
	            }
#endif
	        }

            //printf("Here convert to pcm: %lf\n", convert_to_pcm[0]);

	        for (int i = 0; i < ((afterFirstFilterCnt)/stageTwoDecimation); ++i) {
	            downsampled_audio[i] = (int16_t) ((convert_to_pcm[i * stageTwoDecimation]) * (MAX_PCM/2));
	            if (downsampled_audio[i] >= MAX_PCM) {
                    printf("GOT MAX PCM\n");
	                downsampled_audio[i] = MAX_PCM;
	            }
	        }
	        if (audioWriter->writeSamples((int16_t *) downsampled_audio) < 0) {
	            break;
	        }

        }
        catch (int e) {
            printf("I got an exception: %d\n", e);
            break;
        }
            memcpy(buffer, &(buffer[sampleCnt * 2]), (bufferOffset) * sizeof(int16_t));
            memcpy(bufferShifted, &(bufferShifted[sampleCnt * 2]), (bufferOffset)* sizeof(double));
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
    fclose(sineCSV);
    fclose(rawFFT);
    fclose(shiftedFFT);
    using namespace std::chrono_literals;
    std::this_thread::sleep_for(100ms);

    return 0;
}