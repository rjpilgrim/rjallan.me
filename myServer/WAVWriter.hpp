#pragma once
#include <stdio.h>
#include <AudioSink.hpp>

class WAVWriter : AudioSink {
public:
	WAVWriter() = delete;
	WAVWriter(const WAVWriter&) = delete;
	WAVWriter& operator=(const WAVWriter &) = delete;
	WAVWriter(WAVWriter &&) = delete;
	WAVWriter & operator=(WAVWriter &&) = delete;

	WAVWriter(std::string file_name = "./radio.wav", int sample_rate = 44100, int samples_per_period = 200);
	~WAVWriter();

	int writeSamples(const short * samples) override;

private:
	FILE *fp;
	int samples_per_period = 200;
	int audio_sample_count = 0;

	static constexpr char wav_chunk_id[] = {0x52, 0x49, 0x46, 0x46};
	uint32_t wav_chunk_size = 0;
	static constexpr char wav_format[] = {0x57, 0x41, 0x56, 0x45};
	static constexpr char wav_subchunk_one_id[] = {0x66, 0x6d, 0x74, 0x20};
	uint32_t wav_subchunk_one_size = 16;
	uint16_t wav_audio_format = 1;
	uint16_t wav_num_channels = 1;
	uint32_t wav_sample_rate = 44100;
	uint32_t wav_byte_rate = wav_sample_rate * 2; //2 = 16/8 = bits_per_sample / bits_per_byte
	uint16_t wav_block_align = 2;
	uint16_t wav_bits_per_sample = 16;
	static constexpr char wav_subchunk_two_id[] = {0x64, 0x61, 0x74, 0x61};
	uint32_t wav_subchunk_two_size = 0;

	void initialize_wav_file(FILE * myFile);

}