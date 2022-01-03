#pragma once

enum audio_type { alsa, wav};
enum radio_type { lime, hackrf};
enum server_toggle {on, off};

struct CommandLine {
	radio_type radio = lime;
	audio_type audio = wav;
	server_toggle server = on;

	CommandLine(int argc,  char** argv);
};