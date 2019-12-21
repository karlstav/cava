#pragma once

#include <inttypes.h>
#include <stdbool.h>

struct audio_data {
    int FFTbassbufferSize;
    int FFTmidbufferSize;
    int FFTtreblebufferSize;
    int bass_index;
    int mid_index;
    int treble_index;
    int16_t audio_out_bass_r[65536];
    int16_t audio_out_bass_l[65536];
    int16_t audio_out_mid_r[65536];
    int16_t audio_out_mid_l[65536];
    int16_t audio_out_treble_r[65536];
    int16_t audio_out_treble_l[65536];
    int format;
    unsigned int rate;
    char *source; // alsa device, fifo path or pulse source
    int im;       // input mode alsa, fifo or pulse
    int channels;
    bool left, right, average;
    int terminate; // shared variable used to terminate audio thread
    char error_message[1024];
};

int write_to_fftw_input_buffers(int16_t buf[], int16_t frames, void *data);
