#pragma once
#include <alsa/asoundlib.h>
#include <string>
#include <AudioSink.hpp>

class AlsaStreamer : public AudioSink {
public:
	AlsaStreamer() = delete;
	AlsaStreamer(const AlsaStreamer&) = delete;
	AlsaStreamer& operator=(const AlsaStreamer &) = delete;
	AlsaStreamer(AlsaStreamer &&) = delete;
	AlsaStreamer & operator=(AlsaStreamer &&) = delete;


	AlsaStreamer(std::string audio_device = "hw:0,1,0", int sample_rate = 44100, int samples_per_period = 200);
    ~AlsaStreamer();

	int writeSamples(const short * samples) override;


private:
	snd_pcm_t *alsa_handle;
	int samples_per_period = 200;

    snd_pcm_format_t alsa_format = SND_PCM_FORMAT_S16_LE;	/* sample format */
    unsigned int alsa_rate = 44100;			/* stream rate */
    unsigned int alsa_channels = 1;			/* count of channels */
    unsigned int alsa_buffer_time = 50000;//10000000;		/* ring buffer length in us */
    unsigned int alsa_period_time = 4535; //100000;		/* period time in us */ 10
    int alsa_resample = 1;				/* enable alsa-lib resampling */
    int alsa_period_event = 1;				/* produce poll event after each period */
    snd_pcm_sframes_t alsa_buffer_size;
    snd_pcm_sframes_t alsa_period_size;
    snd_output_t *alsa_output = NULL;

    int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams);

    int xrun_recovery(snd_pcm_t *handle, int err);

    int set_hwparams(snd_pcm_t *handle,
		snd_pcm_hw_params_t *params,
		snd_pcm_access_t access);

};
