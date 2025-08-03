#pragma once

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <unistd.h>
#endif

// number of samples to read from audio source per channel
#define BUFFER_SIZE 512

struct audio_data {
    double *cava_in;

    int input_buffer_size;
    int cava_buffer_size;

    int format;
    unsigned int rate;
    unsigned int channels;
    int threadparams; // shared variable used to prevent main thread from cava_init before input
                      // threads have finalized parameters (0=allow cava_init, 1=disallow)
    char *source;     // alsa device, fifo path or pulse source
    int im;           // input mode alsa, fifo, pulse, portaudio, shmem or sndio
    int terminate;    // shared variable used to terminate audio thread
    char error_message[1024];
    int samples_counter;
    int IEEE_FLOAT;  // format for 32bit (0=int, 1=float)
    int autoconnect; // auto connect to audio source (0=off, 1=once at startup, 2=regularly)
    int active;      // actively monitor sources when the graph is idle
    int remix;       // remix the incoming stream to this many channels
    int virtual;     // set node.virtual to avoid recording notifications
    pthread_mutex_t lock;
};

void reset_output_buffers(struct audio_data *data);
void signal_threadparams(struct audio_data *data);
void signal_terminate(struct audio_data *data);

int write_to_cava_input_buffers(int16_t size, unsigned char *buf, void *data);

extern pthread_mutex_t lock;
