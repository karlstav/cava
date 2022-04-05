#pragma once
#include <stdint.h>

// the size of the smallest input buffer (treble). this will determin the size of all other buffers.
// matches the BUFFER_SIZE in input/common.h
#define CAVA_TREBLE_BUFFER_SIZE 1024

#define CAVA_TOTAL_BUFFER_SIZE CAVA_TREBLE_BUFFER_SIZE * 4

#include <fftw3.h>

struct cava_plan {
    int FFTbassbufferSize;
    int FFTmidbufferSize;
    int FFTtreblebufferSize;
    int number_of_bars;
    int audio_channels;
    int input_buffer_size;
    int rate;
    int bass_cut_off_bar;
    int treble_cut_off_bar;
    int height;
    int sens_init;
    int autosens;
    int frame_skip;

    double sens;
    double g;
    double framerate;

    fftw_plan p_bass_l, p_bass_r;
    fftw_plan p_mid_l, p_mid_r;
    fftw_plan p_treble_l, p_treble_r;

    fftw_complex *out_bass_l, *out_bass_r;
    fftw_complex *out_mid_l, *out_mid_r;
    fftw_complex *out_treble_l, *out_treble_r;

    double *bass_multiplier;
    double *mid_multiplier;
    double *treble_multiplier;

    double *in_bass_r_raw, *in_bass_l_raw;
    double *in_mid_r_raw, *in_mid_l_raw;
    double *in_treble_r_raw, *in_treble_l_raw;
    double *in_bass_r, *in_bass_l;
    double *in_mid_r, *in_mid_l;
    double *in_treble_r, *in_treble_l;
    double *prev_cava_out, *cava_mem;
    double *input_buffer, *cava_peak;

    double *eq;

    float *cut_off_frequency;
    int *FFTbuffer_lower_cut_off;
    int *FFTbuffer_upper_cut_off;
    int *cava_fall;
};

// number_of_bars is number of wanted bars per channel
extern struct cava_plan *cava_init(int number_of_bars, unsigned int rate, int channels,
                                   double range, int autosens);

// cava_in size can be up to  4096 * number of channels, but it is recomended to only fill up some
// new samples per execution as this determines your framerate.
// 512 samples at 44100 sample rate mono, gives about 86 output frames per second.
// new_samples is the number of samples in cava_in
// if you have async reading of data this number can vary from execution to execution

// cava_out size must be number of bars * number of channels, left + right

// plan is the struct returned from cava_init

// cava_execute assumes cava_in samples to be interleaved if more than one channel
extern void cava_execute(double *cava_in, int new_samples, double *cava_out, struct cava_plan *p);

// destroys the plan, frees up memory
extern void cava_destroy(struct cava_plan *p);