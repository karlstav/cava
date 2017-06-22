#pragma once

#include <stdint.h>
#include <stdbool.h>

#define AUDIO_OUT_SIZE 2048
#define BUFFER_SIZE 1024

// assuming stereo
#define CHANNELS_COUNT 2
#define SAMPLE_RATE 44100

#define FRAMES_BUFFER_SIZE 200

struct audio_data {
	int audio_out_r[AUDIO_OUT_SIZE];
	int audio_out_l[AUDIO_OUT_SIZE];
	int8_t format;
	uint8_t channels;
	bool terminate; // shared variable used to terminate audio thread
	unsigned int rate;
	char *source; // alsa device, fifo path or pulse source
};
