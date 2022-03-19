#pragma once
#include <stdint.h>

#define MAX_BARS 1024

// cava_in and cava_out size must be 1024 * number of channels
// cava_in assumes channels to be interleaved
// fill up the entire input bugger before executing!
extern float *cava_plan(int number_of_bars, unsigned int rate, int channels);
extern void cava_execute(int32_t *cava_in, double *cava_out);
extern void cava_destroy(void);