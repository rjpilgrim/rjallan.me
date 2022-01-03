#pragma once
#include <hackrf.h>
#include <chrono>
#include <RadioStreamer.hpp>
#include <mutex>
#include <condition_variable>

class HackRFStreamer : public RadioStreamer {
public:
	int openDevice() override;
	int setFrequency(uint64_t frequency) override;
	int setSampleRate(double sampleRate) override;
	int setFilterBandwidth(uint32_t bandwidth) override;
	int setupStream() override;
	int startStream() override;
	int receiveSamples(int16_t * buffer, int numberOfSamples) override;
	int stopStream() override;
	int destroyStream() override;
	int closeDevice() override;

	std::mutex buffer_mutex;
	std::condition_variable buffer_cv;
	unsigned char thread_buffer[262144*2];
	unsigned int valid_length;

private:
	hackrf_device* device = NULL;
};