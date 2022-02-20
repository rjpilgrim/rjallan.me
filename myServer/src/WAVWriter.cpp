#include <WAVWriter.hpp>

void WAVWriter::initialize_wav_file(FILE * myFile) {
	fwrite(wav_chunk_id, 1, 4, myFile);
    fwrite(&wav_chunk_size, 4, 1, myFile);
    fwrite(wav_format, 1, 4, myFile);
    fwrite(wav_subchunk_one_id, 1, 4, myFile);
    fwrite(&wav_subchunk_one_size, 4, 1, myFile);
    fwrite(&wav_audio_format, 2, 1, myFile);
    fwrite(&wav_num_channels, 2, 1, myFile);
    fwrite(&wav_sample_rate, 4, 1, myFile);
    fwrite(&wav_byte_rate, 4, 1, myFile);
    fwrite(&wav_block_align, 2, 1, myFile);
    fwrite(&wav_bits_per_sample, 2, 1, myFile);
    fwrite(wav_subchunk_two_id, 1, 4, myFile);
    fwrite(&wav_subchunk_two_size, 4, 1, myFile);
}

WAVWriter::WAVWriter(std::string file_name, int sample_rate, int samples_per_period) {
	fp = fopen("./radio.wav", "wb+");




	this->samples_per_period = samples_per_period;
	wav_sample_rate = sample_rate;
	wav_byte_rate = sample_rate * 2;

    initialize_wav_file(fp);
}

int WAVWriter::writeSamples(const int16_t * samples) {
    //printf("HERE IS samples_per_period: %d\n", samples_per_period);
    //printf("here is first ample: %d\n", samples[0]);
	int written = fwrite(samples, 2, samples_per_period, fp);

	audio_sample_count += samples_per_period;
	return written;
}

WAVWriter::~WAVWriter() {
    printf("Here is samples written: %d\n", audio_sample_count);
	wav_chunk_size = 36 + (audio_sample_count * 2);
    wav_subchunk_two_size = (audio_sample_count * 2);

    
    fseek(fp, 4, SEEK_SET);
    fwrite(&wav_chunk_size, 4, 1, fp);
    fseek(fp, 40, SEEK_SET);
    fwrite(&wav_subchunk_two_size, 4, 1, fp);

    fclose(fp);
}