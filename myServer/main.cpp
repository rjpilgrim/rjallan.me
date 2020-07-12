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
#include <alsa/asoundlib.h>
#include <IQWebSocketServer.hpp>
#ifdef USE_GNU_PLOT
#include "gnuPlotPipe.h"
#endif

#define USE_WAV 0
#define STREAM_ALSA 0


#define MAX_PCM 32767


using namespace std;

/* LIME SDR Parameters */

static int my_channel = 0;
static double mono_band = 15000;
static double sample_rate = 1102500;
static double max_difference = ((mono_band/sample_rate) * 2 * M_PI);

static const char wav_chunk_id[] = {0x52, 0x49, 0x46, 0x46};
static uint32_t wav_chunk_size = 0;
static const char wav_format[] = {0x57, 0x41, 0x56, 0x45};
static const char wav_subchunk_one_id[] = {0x66, 0x6d, 0x74, 0x20};
static uint32_t wav_subchunk_one_size = 16;
static uint16_t wav_audio_format = 1;
static uint16_t wav_num_channels = 1;
static uint32_t wav_sample_rate = (int) (sample_rate / 25);
static uint32_t wav_byte_rate = wav_sample_rate * 2; //2 = 16/8 = bits_per_sample / bits_per_byte
static uint16_t wav_block_align = 2;
static uint16_t wav_bits_per_sample = 16;
static const char wav_subchunk_two_id[] = {0x64, 0x61, 0x74, 0x61};
static uint32_t wav_subchunk_two_size = 0;

/* ALSA LIB PARAMETERS */


static char *audio_device = "hw:2,1,0";			/* playback device */
static snd_pcm_format_t alsa_format = SND_PCM_FORMAT_S16_LE;	/* sample format */
static unsigned int alsa_rate = 44100;			/* stream rate */
static unsigned int alsa_channels = 1;			/* count of channels */
static unsigned int alsa_buffer_time = 50000;//10000000;		/* ring buffer length in us */
static unsigned int alsa_period_time = 4535; //100000;		/* period time in us */ 10
static int alsa_resample = 1;				/* enable alsa-lib resampling */
static int alsa_period_event = 1;				/* produce poll event after each period */

static snd_pcm_sframes_t alsa_buffer_size;
static snd_pcm_sframes_t alsa_period_size;
static snd_output_t *alsa_output = NULL;

snd_pcm_t *alsa_handle;


static int xrun_recovery(snd_pcm_t *handle, int err)
{
	if (err == -EPIPE) {	/* under-run */
		err = snd_pcm_prepare(handle);
		if (err < 0)
			printf("Can't recovery from underrun, prepare failed: %s\n", snd_strerror(err));
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN) {
            struct timespec tim, tim2;
            tim.tv_sec = 0;
            tim.tv_nsec = 50000L;
			nanosleep(&tim, &tim2);	/* wait until the suspend flag is released */
        }    
		if (err < 0) {
			err = snd_pcm_prepare(handle);
			if (err < 0)
				printf("Can't recovery from suspend, prepare failed: %s\n", snd_strerror(err));
		}
		return 0;
	}
	return err;
}

static int set_hwparams(snd_pcm_t *handle,
			snd_pcm_hw_params_t *params,
			snd_pcm_access_t access)
{
	unsigned int rrate;
	snd_pcm_uframes_t size;
	int err, dir;

	/* choose all parameters */
	err = snd_pcm_hw_params_any(handle, params);
	if (err < 0) {
		printf("Broken configuration for playback: no configurations available: %s\n", snd_strerror(err));
		return err;
	}
	/* set hardware resampling */
	err = snd_pcm_hw_params_set_rate_resample(handle, params, alsa_resample);
	if (err < 0) {
		printf("Resampling setup failed for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access(handle, params, access);
	if (err < 0) {
		printf("Access type not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the sample format */
	err = snd_pcm_hw_params_set_format(handle, params, alsa_format);
	if (err < 0) {
		printf("Sample format not available for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* set the count of channels */
	err = snd_pcm_hw_params_set_channels(handle, params, alsa_channels);
	if (err < 0) {
		printf("Channels count (%u) not available for playbacks: %s\n", alsa_channels, snd_strerror(err));
		return err;
	}
	/* set the stream rate */
	rrate = alsa_rate;
	err = snd_pcm_hw_params_set_rate_near(handle, params, &rrate, 0);
	if (err < 0) {
		printf("Rate %uHz not available for playback: %s\n", alsa_rate, snd_strerror(err));
		return err;
	}
	if (rrate != alsa_rate) {
		printf("Rate doesn't match (requested %uHz, get %iHz)\n", alsa_rate, err);
		return -EINVAL;
	}
	/* set the buffer time */
	err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &alsa_buffer_time, &dir);
	if (err < 0) {
		printf("Unable to set buffer time %u for playback: %s\n", alsa_buffer_time, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_buffer_size(params, &size);
	if (err < 0) {
		printf("Unable to get buffer size for playback: %s\n", snd_strerror(err));
		return err;
	}
	alsa_buffer_size = size;
	/* set the period time */
	err = snd_pcm_hw_params_set_period_time_near(handle, params, &alsa_period_time, &dir);
	if (err < 0) {
		printf("Unable to set period time %u for playback: %s\n", alsa_period_time, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_period_size(params, &size, &dir);
    printf("here is period size: %d\n", size);
	if (err < 0) {
		printf("Unable to get period size for playback: %s\n", snd_strerror(err));
		return err;
	}
	alsa_period_size = size;
	/* write the parameters to device */
	err = snd_pcm_hw_params(handle, params);
	if (err < 0) {
		printf("Unable to set hw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
{
	int err;

	/* get the current swparams */
	err = snd_pcm_sw_params_current(handle, swparams);
	if (err < 0) {
		printf("Unable to determine current swparams for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* start the transfer when the buffer is almost full: */
	/* (buffer_size / avail_min) * avail_min */
	err = snd_pcm_sw_params_set_start_threshold(handle, swparams, (alsa_buffer_size / alsa_period_size) * alsa_period_size);
	if (err < 0) {
		printf("Unable to set start threshold mode for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* allow the transfer when at least period_size samples can be processed */
	/* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
	err = snd_pcm_sw_params_set_avail_min(handle, swparams, alsa_period_event ? alsa_buffer_size : alsa_period_size);
	if (err < 0) {
		printf("Unable to set avail min for playback: %s\n", snd_strerror(err));
		return err;
	}
	/* enable period events when requested */
	if (alsa_period_event) {
		err = snd_pcm_sw_params_set_period_event(handle, swparams, 1);
		if (err < 0) {
			printf("Unable to set period event: %s\n", snd_strerror(err));
			return err;
		}
	}
	/* write the parameters to the playback device */
	err = snd_pcm_sw_params(handle, swparams);
	if (err < 0) {
		printf("Unable to set sw params for playback: %s\n", snd_strerror(err));
		return err;
	}
	return 0;
}



/*static double filter_coeff[] = {0.0089931  , 0.0130025 ,  0.0243939 ,  0.0415180  , 0.0618060 ,  0.0821686,


   0.0994795  , 0.1110679  , 0.1151410  , 0.1110679  , 0.0994795 ,  0.0821686,


   0.0618060  , 0.0415180  , 0.0243939  , 0.0130025  , 0.0089931};*/



/*static double filter_coeff[] =  {0.0084133,   0.0123980,   0.0236427  , 0.0407934 ,  0.0614051,   0.0823389,


   0.1002953 ,  0.1123879  , 0.1166509  , 0.1123879  , 0.1002953  , 0.0823389,


   0.0614051 ,  0.0407934  , 0.0236427  , 0.0123980  , 0.0084133};*/
/*HAMMING WINDOWS

static double filter_coeff_one[] = {

   0.00090174 , -0.00122372 , -0.00259346 ,  0.00015865 ,  0.00593905 ,  0.00505216,
  -0.00779030 , -0.01661391  , 0.00055036  , 0.03073152 , 0.02468720 , -0.03482811,
  -0.07667645 ,  0.00092075  , 0.19799610 ,  0.37278845  , 0.37278845  , 0.19799610,
   0.00092075 , -0.07667645 , -0.03482811  , 0.02468720  , 0.03073152  , 0.00055036,
  -0.01661391 , -0.00779030 ,  0.00505216  , 0.00593905  , 0.00015865 , -0.00259346,
  -0.00122372  , 0.00090174};

static double filter_coeff_two[] =  {

   0.0035064 ,  0.0041117,   0.0056349  , 0.0081237  , 0.0115681 ,  0.0158981,
   0.0209849 ,  0.0266465,   0.0326567  , 0.0387574  , 0.0446729 ,  0.0501256,
   0.0548529 ,  0.0586223,   0.0612460  , 0.0625919  , 0.0625919 ,  0.0612460,
   0.0586223 ,  0.0548529,   0.0501256  , 0.0446729  , 0.0387574 ,  0.0326567,
   0.0266465 ,  0.0209849,   0.0158981  , 0.0115681  , 0.0081237 ,  0.0056349,
   0.0041117 ,  0.0035064};

*/

/*HANN FILTERS*/
/*static double filter_coeff_one[] = {   //fir1(31, 0.40, hann(32)) - 200 khz LPF for 1 MHZ Sample Rate
0 , -0.0001399197965195086 , -0.0008950810629727927 , 8742626383513622e-06 , 0.004135852013672227 , 0.004006272853916026 ,
-0.006678827421515506 , -0.01497058466941438 , 0.000512691552787154 , 0.02929364983182565 , 0.02391674781831519 ,
-0.03413285432352392 , -0.0757609307646617 , 0.0009148289740002024 , 0.1974081154536128  ,0.3723026132766434,
0.3723026132766434 , 0.1974081154536128 , 0.0009148289740002029 , -0.07576093076466171 , -0.03413285432352392,
0.02391674781831519 , 0.02929364983182567 , 0.0005126915527871541 , -0.01497058466941438 , -0.006678827421515507 ,
0.004006272853916026 , 0.004135852013672227 , 8.742626383513622e-05 , -0.0008950810629727927 , -0.0001399197965195101,  0};

static double filter_coeff_two[] = {  //b= fir1(31, 0.030, hann(32)) - 15 khz LPF for 1 MHZ Sample Rate
    0 , 0.0005051411679387109 , 0.002089594353849786 , 0.004810076198912362 , 0.008655822211457952 , 0.0135458518415099 ,
    0.0193307527750162 , 0.02579898865630902 , 0.03268742235850073 , 0.03969545120993109 , 0.04650189261729837 ,
    0.05278355716806539 , 0.05823431650233354 , 0.062583424936726 , 0.06561189111971738,  0.0671658168824335,
    0.0671658168824335 , 0.06561189111971739 , 0.062583424936726 , 0.05823431650233355 , 0.05278355716806541,
    0.04650189261729837 , 0.03969545120993111 , 0.03268742235850076,0.02579898865630902  ,0.0193307527750162,
    0.0135458518415099 , 0.008655822211457954 , 0.004810076198912363 , 0.002089594353849786 , 0.0005051411679387163 , 0};

*/

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



void initialize_wav_file(FILE * myFile) {
    fwrite(wav_chunk_id, 1, 4, myFile);
    fwrite(&wav_chunk_size, 4, 1, myFile);
    fwrite(wav_format, 1, 4, myFile);
    fwrite(wav_subchunk_one_id, 1, 4, myFile);
    fwrite(&wav_subchunk_one_size, 4, 1, myFile);
    fwrite(&wav_audio_format, 2, 1, myFile);
    fwrite(&wav_num_channels, 2, 1, myFile);
    fwrite(&wav_sample_rate, 4, 1, myFile);
    fwrite(&wav_byte_rate, 4, 1, myFile);
    fwrite(&wav_block_align, 2, 1, myFile);
    fwrite(&wav_bits_per_sample, 2, 1, myFile);
    fwrite(wav_subchunk_two_id, 1, 4, myFile);
    fwrite(&wav_subchunk_two_size, 4, 1, myFile);
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
    snd_pcm_close(alsa_handle);
    if (device != NULL)
        LMS_Close(device);
    exit(-1);
}

int main(int argc, char** argv)
{
#if USE_WAV    
    FILE *fp;

    fp = fopen("./radio.wav", "wb+");

    initialize_wav_file(fp);
#endif

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

    double myBandWidthBefore;
    double bandWidthAfter;

    if (LMS_GetLPFBW(device, LMS_CH_RX, my_channel, &myBandWidthBefore) != 0)
        error();

    if (LMS_SetLPFBW(device, LMS_CH_RX, my_channel, 10e6/*2.51e6*/) != 0)
        error(); 

    if (LMS_GetLPFBW(device, LMS_CH_RX, my_channel, &bandWidthAfter) != 0)
        error();


    LMS_SetNormalizedGain(device, LMS_CH_RX, my_channel, 0.9);

    double mygain;

    unsigned int gaindB;


    LMS_Calibrate(device, LMS_CH_RX, my_channel, 10e6, 0);

    LMS_GetNormalizedGain(device, LMS_CH_RX, my_channel, &mygain);

    printf("here is my gain after calib: %f\n", mygain);

    lms_stream_meta_t my_stream_data;

    auto my_server = std::make_unique<IQWebSocketServer>(8079, "^/socket/?$");
    my_server->run();

    //Enable test signal generation
    //To receive data from RF, remove this line or change signal to LMS_TESTSIG_NONE
    //if (LMS_SetTestSignal(device, LMS_CH_RX, 0, LMS_TESTSIG_NCODIV8, 0, 0) != 0)
        //error();


    //ALSA SETUP

    
	int err, morehelp;
	snd_pcm_hw_params_t *alsa_hwparams;
	snd_pcm_sw_params_t *alsa_swparams;
	int method = 0;
	short *alsa_samples;
	snd_pcm_channel_area_t *alsa_areas;

	snd_pcm_hw_params_alloca(&alsa_hwparams);
	snd_pcm_sw_params_alloca(&alsa_swparams);

    err = snd_output_stdio_attach(&alsa_output, stdout, 0);
	if (err < 0) {
		printf("Output failed: %s\n", snd_strerror(err));
		return 0;
	}

    if ((err = snd_pcm_open(&alsa_handle, audio_device, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK)) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
        error();
		return 0;
	}

    if ((err = set_hwparams(alsa_handle, alsa_hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
		printf("Setting of hwparams failed: %s\n", snd_strerror(err));
		error();
	}
	if ((err = set_swparams(alsa_handle, alsa_swparams)) < 0) {
		printf("Setting of swparams failed: %s\n", snd_strerror(err));
		error();
	}
	

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
#if USE_WAV
    double my_avg = 0;
    double global_avg = 0;
    int count = 0;
    long big_freq_count = 0;
    long normal = 0;
#endif
    //Streaming
#ifdef USE_GNU_PLOT
    /*
    GNUPlotPipe gp;
    gp.write("set size square\n set xrange[-2050:2050]\n set yrange[-2050:2050]\n");
    //gp.write("set size square\n set xrange[-100:100]\n set yrange[-100:100]\n");
    */
#endif
    auto t1 = chrono::high_resolution_clock::now();
#if USE_WAV
    while ((chrono::high_resolution_clock::now() - t1 < chrono::seconds(12)))
#else
    while (true)
#endif
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
#if USE_WAV
        count++;
        my_avg = 0;
#endif
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
#if USE_WAV
            if (convert_to_pcm[i-filter_order]>0.1) {
                big_freq_count++;
            }
            else {
                normal++;
            }
            my_avg += convert_to_pcm[i-filter_order];            
        }
        my_avg = my_avg / (samplesFiltered-1);
        global_avg += my_avg;
#else
        }
#endif

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
#if USE_WAV    
            audio_sample_count++;
#endif
        }

#if USE_WAV
        fwrite(downsampled_audio, 2, ((samplesRead)/25), fp);
#else
#if STREAM_ALSA
        alsa_samples = (short *) downsampled_audio;
        int cptr = 200;
        while (cptr > 0) {
            //err = 4000;
            //printf("Trying write\n");
			err = snd_pcm_writei(alsa_handle, alsa_samples, cptr);
            //printf("wrote this many: %d\n", err);
			if (err == -EAGAIN)
				continue;
			if (err < 0) {
                printf("GOt an error in pcm write: %d\n", err);
				if (xrun_recovery(alsa_handle, err) < 0) {
					printf("Write error: %s\n", snd_strerror(err));
                    break;
				}
			}
            else {
			    alsa_samples += err * alsa_channels;
			    cptr -= err;
            }
		}
        //err = 1;
        if (err < 0) {
            break;
        }
#endif
#endif
        }
        catch (int e) {
            printf("I got an exception: %d\n", e);
            break;
        }
            memcpy(buffer, &(buffer[10000]),2*(4 * filter_order + 2));
        }

	//I and Q samples are interleaved in buffer: IQIQIQ...
        //printf("Received %d samples\n", samplesRead);
	/*
		INSERT CODE FOR PROCESSING RECEIVED SAMPLES
	*/
#ifdef USE_GNU_PLOT
    /*
        //Plot samples
        gp.write("plot '-' with points\n");
        for (int j = 0; j < samplesRead; ++j)
            gp.writef("%i %i\n", buffer[2 * j], buffer[2 * j + 1]);
        gp.write("e\n");
        gp.flush();
    */
    
#endif
    
    }
    //Stop streaming
    LMS_StopStream(&streamId); //stream is stopped but can be started again with LMS_StartStream()
    LMS_DestroyStream(device, &streamId); //stream is deallocated and can no longer be used

    //Close device
    LMS_Close(device);
    snd_pcm_close(alsa_handle);
    
    printf("Here is my max value after demod: %f\n", my_max);
    

#if USE_WAV
    global_avg = global_avg / count;
    printf("here is my average: %f\n", global_avg);
    printf("here are my outliers: %ld\n", big_freq_count);
    printf("Here are normal samples: %ld\n", normal);
    printf("Here is my number of audiosamples: %d\n", audio_sample_count);
    printf("Here is my .wav sample rate: %d\n", wav_sample_rate);

    wav_chunk_size = 36 + (audio_sample_count * 2);
    wav_subchunk_two_size = (audio_sample_count * 2);

    
    fseek(fp, 4, SEEK_SET);
    fwrite(&wav_chunk_size, 4, 1, fp);
    fseek(fp, 40, SEEK_SET);
    fwrite(&wav_subchunk_two_size, 4, 1, fp);

    fclose(fp);
#endif

    return 0;
}