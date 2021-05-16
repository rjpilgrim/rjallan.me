#pragma once

enum audio_type { alsa, wav};
enum radio_type { lime, hackrf};

struct CommandLine {
	radio_type radio = lime;
	audio_type audio = wav;

	CommandLine(int argc,  char** argv);
};