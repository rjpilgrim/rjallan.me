class AudioSink {
public:
	virtual int writeSamples(const short * samples) = 0;
}
