#pragma once
#include <stdint.h>

#define MAX_BARS 1024

// cava_in and cava_out size must be 1024 for mono and 1024 * 2 for stereo
float *cava_plan(int number_of_bars, unsigned int rate);
void cava_execute(int32_t *cava_in, double *cava_out, int number_of_bars, int stereo);
void cava_destroy(void);