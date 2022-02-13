#include <HackRFStreamer.hpp>

int HackRFStreamer::openDevice() {
	int n;
	if (hackrf_init() != 0) {
		return -1;
	}
	hackrf_device_list_t * device_list =  hackrf_device_list();

	n = hackrf_device_list_open(device_list, 0, &device);

	if (n != 0) {
		return -1;
	}
	return 0;
}

int HackRFStreamer::setFrequency(uint64_t frequency) {
    //Set center frequency
    if (hackrf_set_freq(device, frequency) != 0)
        return -1;

    return 0;
}

int HackRFStreamer::setSampleRate(double sampleRate) {

    if (hackrf_set_sample_rate(device, sampleRate) != 0)
    	return -1;

    return 0;
}

int HackRFStreamer::setFilterBandwidth(uint32_t bandwidth) {

	int compute_bandwidth = hackrf_compute_baseband_filter_bw(bandwidth);

	printf("Here is compute bandwidth: %d, and bandwidt: %d\n", compute_bandwidth, bandwidth);

    if (hackrf_set_baseband_filter_bandwidth(device, compute_bandwidth) != 0)
        return -1;

    return 0;
}

int HackRFStreamer::setupStream() {
	if (hackrf_set_amp_enable(device, 1) != 0)
		return -1;

	hackrf_set_lna_gain(device, 20);

	hackrf_set_vga_gain(device, 20);

	return hackrf_set_antenna_enable(device, 1);
	//return 0;
}

int myCallback(hackrf_transfer* transfer) {
	//printf("IN MY RF CALLBACK\n");
	HackRFStreamer * myStreamer =  (HackRFStreamer *) transfer->rx_ctx;
	std::unique_lock<std::mutex> lck(myStreamer->buffer_mutex);
	if (myStreamer->valid_length >= 262144) {
		memmove(myStreamer->thread_buffer, myStreamer->thread_buffer + transfer->valid_length, 262144);
		myStreamer->valid_length = 262144;
	}
	memcpy(myStreamer->thread_buffer+myStreamer->valid_length, transfer->buffer, transfer->valid_length);
	myStreamer->valid_length = myStreamer->valid_length + transfer->valid_length;
	//printf("FINISHED RF CALLBACK, here is my valid length: %d, and first real: %u, and first imag: %u\n", myStreamer->valid_length, transfer->buffer[0], transfer->buffer[1]);
	//printf("FINISHED RF CALLBACK, here is my valid length: %d, and first real: %u, and first imag: %u\n", myStreamer->valid_length, myStreamer->thread_buffer[0], myStreamer->thread_buffer[1]);
	lck.unlock();
	myStreamer->buffer_cv.notify_all();
	
	return 0;
}


int HackRFStreamer::startStream() {
	return hackrf_start_rx(device, &myCallback, (void *) this);
}

int HackRFStreamer::receiveSamples(int16_t * buffer, int numberOfSamples) {
	//printf("IN RECEIVE SAMPLES\n");
	std::unique_lock<std::mutex> lck(buffer_mutex);
	uint32_t samplesFilled = 0;
	while (samplesFilled < (numberOfSamples*2))
	{
		while(valid_length < (numberOfSamples*2)) {
			if (buffer_cv.wait_for(lck, std::chrono::milliseconds(1000)) == std::cv_status::timeout)
            {
                return samplesFilled;
            }
		}
		int samplesLeft = (numberOfSamples*2) - samplesFilled;
		int read_length = valid_length > samplesLeft ? samplesLeft : valid_length;
		for (int i = samplesFilled, j = 0; j < read_length; i = i + 2, j = j + 2) {
			buffer[i] = (char)thread_buffer[j];
			buffer[i+1] = (char)thread_buffer[j+1];
		}
		//memcpy(&buffer[samplesFilled], thread_buffer, read_length);
		valid_length = valid_length - read_length;
		memmove(thread_buffer, &thread_buffer[read_length], valid_length);
		samplesFilled = samplesFilled + read_length;
	}
	//printf("FINISHED RECEIVE SAMPLES, here is valid length: %d, and first real: %d, and first imag: %d\n", valid_length, buffer[0], buffer[1]);
	return samplesFilled/2;
 }

 int HackRFStreamer::stopStream() {
 	return hackrf_stop_rx(device);
 }

 int HackRFStreamer::destroyStream() {
 	return 0;
 }

 int HackRFStreamer::closeDevice() {
 	return hackrf_close(device);
 }
