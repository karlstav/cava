#pragma once

#include <stdbool.h>
#include <stdio.h>

#define MAX_ERROR_LEN 1024

struct config_params {
    char *color, *bcolor, *raw_target, *audio_source,
        /**gradient_color_1, *gradient_color_2,*/ **gradient_colors, *data_format, *mono_option;
    char bar_delim, frame_delim;
    double monstercat, integral, gravity, ignore, sens;
    unsigned int lowcf, highcf;
    double *smooth;
    int smcount, customEQ, im, om, col, bgcol, autobars, stereo, is_bin, ascii_range, bit_format,
        gradient, gradient_count, fixedbars, framerate, bw, bs, autosens, overshoot, waves,
        FFTbufferSize, fifoSample;
};

struct error_s {
    char message[MAX_ERROR_LEN];
    int length;
};

bool load_config(char configPath[255], char supportedInput[255], struct config_params *p,
                 bool colorsOnly, struct error_s *error);
