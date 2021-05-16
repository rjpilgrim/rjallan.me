#pragma once
class AudioSink {
public:
	/*AudioSink(const AudioSink&) = default;
	virtual AudioSink& operator=(const AudioSink &) = default;
	AudioSink(AudioSink &&) = default;
	virtual AudioSink & operator=(AudioSink &&) = default;*/

	virtual int writeSamples(const short * samples) = 0;

	virtual ~AudioSink() = default;
};
