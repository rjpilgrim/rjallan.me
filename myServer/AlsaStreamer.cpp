#include <AlsaStreamer.hpp>

int AlsaStreamer::xrun_recovery(snd_pcm_t *handle, int err)
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

int AlsaStreamer::set_hwparams(snd_pcm_t *handle,
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

int AlsaStreamer::set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams)
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


AlsaStreamer::AlsaStreamer(std::string audio_device, int sample_rate, int samples_per_period) {
	this->alsa_rate = sample_rate;
	this->samples_per_period = samples_per_period;

	snd_pcm_hw_params_t *alsa_hwparams;
	snd_pcm_sw_params_t *alsa_swparams;

	snd_pcm_hw_params_alloca(&alsa_hwparams);
	snd_pcm_sw_params_alloca(&alsa_swparams);
	int err = 0;

	if (err = snd_output_stdio_attach(&alsa_output, stdout, 0) < 0) {
		printf("Output failed: %s\n", snd_strerror(err));
		snd_pcm_close(alsa_handle);
        exit(-1);
	}

    if (err = snd_pcm_open(&alsa_handle, audio_device.c_str(), SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK) < 0) {
		printf("Playback open error: %s\n", snd_strerror(err));
        snd_pcm_close(alsa_handle);
        exit(-1);
	}

    if (err = set_hwparams(alsa_handle, alsa_hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		printf("Setting of hwparams failed: %s\n", snd_strerror(err));
		snd_pcm_close(alsa_handle);
        exit(-1);
	}
	if (err = set_swparams(alsa_handle, alsa_swparams) < 0) {
		printf("Setting of swparams failed: %s\n", snd_strerror(err));
		snd_pcm_close(alsa_handle);
        exit(-1);
	}
}

int AlsaStreamer::writeSamples(const short * samples) {
	int cptr = samples_per_period;
	int err = 0;
    while (cptr > 0) {
		err = snd_pcm_writei(alsa_handle, samples, cptr);
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
		    samples += err * alsa_channels;
		    cptr -= err;
        }
	}
	return err;
}

AlsaStreamer::~AlsaStreamer() {
	snd_pcm_close(alsa_handle);
}

