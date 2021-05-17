#include <LimeStreamer.hpp>
#include <iostream>

int LimeStreamer::openDevice() {
	int n;
    lms_info_str_t list[8]; //should be large enough to hold all detected devices
    
    if ((n = LMS_GetDeviceList(list)) < 0) //NULL can be passed to only get number of devices
        return -1;

    std::cout << "Devices found: " << n << std::endl; //print number of devices
    if (n < 1)
        return -1;

    //open the first device
    if (LMS_Open(&device, list[0], NULL))
        return -1;

    if (LMS_Init(device) != 0)
         return -1;

    return 0;
}

int LimeStreamer::setFrequency(uint64_t frequency) {
	//Enable RX channel
    //Channels are numbered starting at 0
    if (LMS_EnableChannel(device, LMS_CH_RX, myChannel, true) != 0)
        return -1;

    //Set center frequency to 800 MHz
    if (LMS_SetLOFrequency(device, LMS_CH_RX, myChannel, frequency) != 0)
        return -1;

    return 0;
}

int LimeStreamer::setSampleRate(double sampleRate) {
	//Set sample rate to 8 MHz, ask to use 2x oversampling in RF
    //This set sampling rate for all channels
    if (LMS_SetSampleRate(device, sampleRate, 2) != 0)
        return -1;

    LMS_Calibrate(device, LMS_CH_RX, myChannel, sampleRate, 0);

    return 0;
}

int LimeStreamer::setFilterBandwidth(uint32_t bandwidth) {

    if (LMS_SetLPFBW(device, LMS_CH_RX, myChannel, bandwidth) != 0)
        return -1;

    return 0;
}

int LimeStreamer::setupStream() {
    streamId.channel = myChannel; //channel number
    streamId.fifoSize = 1024 * 1024; //fifo size in samples
    streamId.throughputVsLatency = 1.0; //optimize for max throughput
    streamId.isTx = false; //RX channel
    streamId.dataFmt = lms_stream_t::LMS_FMT_I12; //12-bit integers
    if (LMS_SetupStream(device, &streamId) != 0)
        return -1;

    return 0;
}

int LimeStreamer::startStream() {
    if (LMS_StartStream(&streamId) != 0)
        return -1;

    return 0;
}

int LimeStreamer::receiveSamples(int16_t * buffer, int numberOfSamples) {
    return LMS_RecvStream(&streamId, buffer, numberOfSamples, &myStreamData, 1000);
}

int LimeStreamer::stopStream() {
    return LMS_StopStream(&streamId); //stream is stopped but can be started again with LMS_StartStream()
}

int LimeStreamer::destroyStream() {
    return LMS_DestroyStream(device, &streamId); //stream is deallocated and can no longer be used
}

int LimeStreamer::closeDevice() {
    return LMS_Close(device);
}
