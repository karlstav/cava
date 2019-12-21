#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MAX_ERROR_LEN 1024

#ifdef PORTAUDIO
#define HAS_PORTAUDIO true
#else
#define HAS_PORTAUDIO false
#endif

#ifdef ALSA
#define HAS_ALSA true
#else
#define HAS_ALSA false
#endif

#ifdef PULSE
#define HAS_PULSE true
#else
#define HAS_PULSE false
#endif

#ifdef SNDIO
#define HAS_SNDIO true
#else
#define HAS_SNDIO false
#endif

#ifdef SHMEM
#define HAS_SHMEM true
#else
#define HAS_SHMEM false
#endif

// These are in order of least-favourable to most-favourable choices, in case
// multiple are supported and configured.
enum input_method {
    INPUT_FIFO,
    INPUT_PORTAUDIO,
    INPUT_ALSA,
    INPUT_PULSE,
    INPUT_SNDIO,
    INPUT_SHMEM,
    INPUT_MAX
};

struct config_params {
    char *color, *bcolor, *raw_target, *audio_source,
        /**gradient_color_1, *gradient_color_2,*/ **gradient_colors, *data_format, *mono_option;
    char bar_delim, frame_delim;
    double monstercat, integral, gravity, ignore, sens;
    unsigned int lowcf, highcf;
    double *smooth;
    enum input_method im;
    int smcount, customEQ, om, col, bgcol, autobars, stereo, is_bin, ascii_range, bit_format,
        gradient, gradient_count, fixedbars, framerate, bw, bs, autosens, overshoot, waves,
        FFTbufferSize, fifoSample, fifoSampleBits;
};

struct error_s {
    char message[MAX_ERROR_LEN];
    int length;
};

bool load_config(char configPath[PATH_MAX], struct config_params *p, bool colorsOnly,
                 struct error_s *error);
