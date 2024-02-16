#pragma once

#include "../cavacore.h"
#include "../config.h"
#include "../input/common.h"

#ifdef SDL
#include "sdl_cava.h"
#endif

#ifdef SDL_GLSL
#include "sdl_glsl.h"
#endif

#include "terminal_noncurses.h"

#ifndef _MSC_VER
#ifdef NCURSES
#include "terminal_bcircle.h"
#include "terminal_ncurses.h"
#include <curses.h>
#endif

#include "noritake.h"
#include "raw.h"

#endif

struct audio_raw {
    int *bars;
    int *previous_frame;
    float *bars_left;
    float *bars_right;
    float *bars_raw;
    double *cava_out;
    int *dimension_bar;
    int *dimension_value;
    double userEQ_keys_to_bars_ratio;
    int channels;
    int number_of_bars;
    int output_channels;
    int height;
    int lines;
    int width;
    int remainder;
};

int audio_raw_init(struct audio_data *audio, struct audio_raw *audio_raw, struct config_params *prm,
                   struct cava_plan *plan);

int audio_raw_fetch(struct audio_raw *audio_raw, struct config_params *prm, int *re_paint,
                    struct cava_plan *plan);

int audio_raw_clean(struct audio_raw *audio_raw);
int audio_raw_destroy(struct audio_raw *audio_raw);
