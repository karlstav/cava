#pragma once

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

struct audio_data {
    int FFTbassbufferSize;
    int FFTmidbufferSize;
    int FFTtreblebufferSize;
    int bass_index;
    int mid_index;
    int treble_index;
    double *bass_multiplier;
    double *mid_multiplier;
    double *treble_multiplier;
    double *in_bass_r_raw, *in_bass_l_raw;
    double *in_mid_r_raw, *in_mid_l_raw;
    double *in_treble_r_raw, *in_treble_l_raw;
    double *in_bass_r, *in_bass_l;
    double *in_mid_r, *in_mid_l;
    double *in_treble_r, *in_treble_l;
    int format;
    unsigned int rate;
    char *source; // alsa device, fifo path or pulse source
    int im;       // input mode alsa, fifo or pulse
    unsigned int channels;
    bool left, right, average;
    int terminate; // shared variable used to terminate audio thread
    char error_message[1024];
};

void reset_output_buffers(struct audio_data *data);

int write_to_fftw_input_buffers(int16_t frames, int16_t buf[frames * 2], void *data);

extern pthread_mutex_t lock;
