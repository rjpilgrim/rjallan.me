#include "lime/LimeSuite.h"

class LimeStreamer : RadioStreamer {
public:
	int openDevice() override;
	int setFrequency(uint64_t frequency) override;
	int setSampleRate(double sampleRate) override;
	int setFilterBandwidth(uint32_t bandwidth) override;
	int setupStream() override;
	int startStream() override;
	int receiveSamples(int16_t * buffer, int number_of_samples) override;
	int stopStream() override;
	int destroyStream() override;
	int closeDevice() override;

private:
	lms_device_t* device = NULL;
	lms_stream_t streamId; 
	lms_stream_meta_t my_stream_data;
}