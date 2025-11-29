/*
Copyright (c) 2022 Karl Stavestrand <karl@stavestrand.no>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifdef __cplusplus
extern "C" {
#endif
#pragma once
#include <stdint.h>

#include <fftw3.h>

// cava_plan, parameters used internally by cavacore, do not modify these directly
// only the cut off frequencies is of any potential interest to read out,
// the rest should most likely be hidden somehow
struct cava_plan {
    int FFTbassbufferSize;
    int FFTbufferSize;
    int number_of_bars;
    int audio_channels;
    int input_buffer_size;
    int rate;
    int bass_cut_off_bar;
    int sens_init;
    int autosens;
    int frame_skip;
    int status;
    char error_message[1024];

    double sens;
    double framerate;
    double noise_reduction;

    fftw_plan p_bass_l, p_bass_r;
    fftw_plan p_l, p_r;

    fftw_complex *out_bass_l, *out_bass_r;
    fftw_complex *out_l, *out_r;

    double *bass_multiplier;
    double *multiplier;

    double *in_bass_r_raw, *in_bass_l_raw;
    double *in_r_raw, *in_l_raw;
    double *in_bass_r, *in_bass_l;
    double *in_r, *in_l;
    double *prev_cava_out, *cava_mem;
    double *input_buffer, *cava_peak;

    double *eq;

    float *cut_off_frequency;
    int *FFTbuffer_lower_cut_off;
    int *FFTbuffer_upper_cut_off;
    double *cava_fall;
};

// cava_init, initialize visualization, takes the following parameters:

// number_of_bars, number of wanted bars per channel

// rate, sample rate of input signal

// channels, number of interleaved channels in input

// autosens, toggle automatic sensitivity adjustment 1 = on, 0 = off
// on, gives a dynamically adjusted output signal from 0 to 1
// the output is continuously adjusted to use the entire range
// off, will pass the raw values from cava directly to the output
// the max values will then be dependent on the input

// noise_reduction, adjust noise reduction filters. 0 - 1, recommended 0.77
// the raw visualization is very noisy, this factor adjusts the integral
// and gravity filters inside cavacore to keep the signal smooth
// 1 will be very slow and smooth, 0 will be fast but noisy.

// low_cut_off, high_cut_off cut off frequencies for visualization in Hz
// recommended: 50, 10000

// returns a cava_plan to be used by cava_execute. If cava_plan.status is 0 all is OK.
// If cava_plan.status is -1, cava_init was called with an illegal parameter, see error string in
// cava_plan.error_message
extern struct cava_plan *cava_init(int number_of_bars, unsigned int rate, int channels,
                                   int autosens, double noise_reduction, int low_cut_off,
                                   int high_cut_off);

// cava_execute, executes visualization

// cava_in, input buffer can be any size. internal buffers in cavacore is
// 4096 * number of channels at 44100 samples rate, if new_samples is greater
// then samples will be discarded. However it is recommended to use less
// new samples per execution as this determines your framerate.
// 512 samples at 44100 sample rate mono, gives about 86 frames per second.

// new_samples, the number of samples in cava_in to be processed per execution
// in case of async reading of data this number is allowed to vary from execution to execution

// cava_out, output buffer. Size must be number of bars * number of channels. Bars will
// be sorted from lowest to highest frequency. If stereo input channels are configured
// then all left channel bars will be first then the right.

// plan, the cava_plan struct returned from cava_init

// cava_execute assumes cava_in samples to be interleaved if more than one channel
// only up to two channels are supported.
extern void cava_execute(double *cava_in, int new_samples, double *cava_out,
                         struct cava_plan *plan);

// cava_destroy, destroys the plan, frees up memory
extern void cava_destroy(struct cava_plan *plan);

#ifdef __cplusplus
}
#endif
