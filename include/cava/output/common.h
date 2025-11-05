#pragma once
//#include "cava/cavacore.h"
//#include "cava/config.h"
//#include "cava/input/common.h"

struct audio_raw {
    int *bars;
    int *previous_frame;
    float *bars_left;
    float *bars_right;
    float *bars_raw;
    float *previous_bars_raw;
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
                   struct cava_plan **plan);
#ifndef SDL_GLSL
int audio_raw_fetch(struct audio_raw *audio_raw, struct config_params *prm, struct cava_plan *plan);
#else
int audio_raw_fetch(struct audio_raw *audio_raw, struct config_params *prm, int *re_paint,
                    struct cava_plan *plan);
#endif

int audio_raw_clean(struct audio_raw *audio_raw);
int audio_raw_destroy(struct audio_raw *audio_raw);
void cleanup(int output_mode);
