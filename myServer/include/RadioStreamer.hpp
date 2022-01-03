#pragma once
class RadioStreamer {
public:
	virtual int openDevice() = 0;

	virtual int setFrequency(uint64_t frequency) = 0;

	virtual int setSampleRate(double sampleRate) = 0;

	virtual int setFilterBandwidth(uint32_t bandwidth) = 0;

	virtual int setupStream() = 0;

	virtual int startStream() = 0;

	virtual int receiveSamples(int16_t * buffer, int numberOfSamples) = 0;

	virtual int stopStream() = 0;

	virtual int destroyStream() = 0;

	virtual int closeDevice() = 0;

	virtual ~RadioStreamer() = default;

};