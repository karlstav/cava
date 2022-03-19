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

#define MAX_BARS 1024

struct audio_data {
    int32_t *cava_in;

    int FFTbassbufferSize;
    int FFTmidbufferSize;
    int FFTtreblebufferSize;
    int input_buffer_size;

    int format;
    unsigned int rate;
    char *source; // alsa device, fifo path or pulse source
    int im;       // input mode alsa, fifo or pulse
    unsigned int channels;
    bool left, right, average;
    int terminate; // shared variable used to terminate audio thread
    char error_message[1024];
    int samples_counter;
};

void reset_output_buffers(struct audio_data *data);

int write_to_cava_input_buffers(int16_t size, int16_t buf[size], void *data);

extern pthread_mutex_t lock;
