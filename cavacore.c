#include "cavacore.h"
#ifndef M_PI
#define M_PI 3.1415926535897932385
#endif
#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

struct cava_plan *cava_init(int number_of_bars, unsigned int rate, int channels, int height,
                            int framerate) {

    int bass_cut_off = 150;
    int treble_cut_off = 2500;

    struct cava_plan *p;
    memset(&p, 0, sizeof(p));
    p = malloc(sizeof(int) * 11 + sizeof(double) * 3 + sizeof(fftw_plan) * 6 +
               sizeof(fftw_complex *) * 6 + sizeof(double *) * 20 + sizeof(float *) * 1 +
               sizeof(int *) * 3);

    p->number_of_bars = number_of_bars;
    p->audio_channels = channels;
    p->rate = rate;
    p->height = height;

    p->g = log10((float)p->height) * 0.05 * pow((60 / (float)framerate), 2.5);
    p->sens = 1;

    p->FFTbassbufferSize = CAVA_TREBLE_BUFFER_SIZE * 4;
    p->FFTmidbufferSize = CAVA_TREBLE_BUFFER_SIZE * 2;
    p->FFTtreblebufferSize = CAVA_TREBLE_BUFFER_SIZE;

    p->input_buffer_size = p->FFTbassbufferSize * channels;

    // because of Nyquist samplings theorem and how fftw works
    int max_bars = CAVA_TREBLE_BUFFER_SIZE / channels;

    p->input_buffer = (double *)malloc(p->input_buffer_size * sizeof(double));
    p->FFTbuffer_lower_cut_off = (int *)malloc(max_bars * sizeof(int));
    p->FFTbuffer_upper_cut_off = (int *)malloc(max_bars * sizeof(int));
    p->eq = (double *)malloc(max_bars * sizeof(double));
    p->cut_off_frequency = (float *)malloc(max_bars * sizeof(float));
    p->cava_fall = (int *)malloc(max_bars * sizeof(int));
    p->cava_mem = (double *)malloc(max_bars * sizeof(double));
    p->cava_peak = (double *)malloc(max_bars * sizeof(double));
    p->prev_cava_out = (double *)malloc(max_bars * sizeof(double));

    // Hann Window calculate multipliers
    p->bass_multiplier = (double *)malloc(p->FFTbassbufferSize * sizeof(double));
    p->mid_multiplier = (double *)malloc(p->FFTmidbufferSize * sizeof(double));
    p->treble_multiplier = (double *)malloc(p->FFTtreblebufferSize * sizeof(double));
    for (int i = 0; i < p->FFTbassbufferSize; i++) {
        p->bass_multiplier[i] = 0.5 * (1 - cos(2 * M_PI * i / (p->FFTbassbufferSize - 1)));
    }
    for (int i = 0; i < p->FFTmidbufferSize; i++) {
        p->mid_multiplier[i] = 0.5 * (1 - cos(2 * M_PI * i / (p->FFTmidbufferSize - 1)));
    }
    for (int i = 0; i < p->FFTtreblebufferSize; i++) {
        p->treble_multiplier[i] = 0.5 * (1 - cos(2 * M_PI * i / (p->FFTtreblebufferSize - 1)));
    }

    // BASS
    p->in_bass_l = fftw_alloc_real(p->FFTbassbufferSize);
    p->in_bass_l_raw = fftw_alloc_real(p->FFTbassbufferSize);
    p->out_bass_l = fftw_alloc_complex(p->FFTbassbufferSize / 2 + 1);
    p->p_bass_l =
        fftw_plan_dft_r2c_1d(p->FFTbassbufferSize, p->in_bass_l, p->out_bass_l, FFTW_MEASURE);

    // MID
    p->in_mid_l = fftw_alloc_real(p->FFTmidbufferSize);
    p->in_mid_l_raw = fftw_alloc_real(p->FFTmidbufferSize);
    p->out_mid_l = fftw_alloc_complex(p->FFTmidbufferSize / 2 + 1);
    p->p_mid_l = fftw_plan_dft_r2c_1d(p->FFTmidbufferSize, p->in_mid_l, p->out_mid_l, FFTW_MEASURE);

    // TREBLE
    p->in_treble_l = fftw_alloc_real(p->FFTtreblebufferSize);
    p->in_treble_l_raw = fftw_alloc_real(p->FFTtreblebufferSize);
    p->out_treble_l = fftw_alloc_complex(p->FFTtreblebufferSize / 2 + 1);
    p->p_treble_l =
        fftw_plan_dft_r2c_1d(p->FFTtreblebufferSize, p->in_treble_l, p->out_treble_l, FFTW_MEASURE);

    memset(p->in_bass_l, 0, sizeof(double) * p->FFTbassbufferSize);
    memset(p->in_mid_l, 0, sizeof(double) * p->FFTmidbufferSize);
    memset(p->in_treble_l, 0, sizeof(double) * p->FFTtreblebufferSize);
    memset(p->in_bass_l_raw, 0, sizeof(double) * p->FFTbassbufferSize);
    memset(p->in_mid_l_raw, 0, sizeof(double) * p->FFTmidbufferSize);
    memset(p->in_treble_l_raw, 0, sizeof(double) * p->FFTtreblebufferSize);
    memset(p->out_bass_l, 0, (p->FFTbassbufferSize / 2 + 1) * sizeof(fftw_complex));
    memset(p->out_mid_l, 0, (p->FFTmidbufferSize / 2 + 1) * sizeof(fftw_complex));
    memset(p->out_treble_l, 0, (p->FFTtreblebufferSize / 2 + 1) * sizeof(fftw_complex));
    if (p->audio_channels == 2) {
        // BASS
        p->in_bass_r = fftw_alloc_real(p->FFTbassbufferSize);
        p->in_bass_r_raw = fftw_alloc_real(p->FFTbassbufferSize);
        p->out_bass_r = fftw_alloc_complex(p->FFTbassbufferSize / 2 + 1);
        p->p_bass_r =
            fftw_plan_dft_r2c_1d(p->FFTbassbufferSize, p->in_bass_r, p->out_bass_r, FFTW_MEASURE);

        // MID
        p->in_mid_r = fftw_alloc_real(p->FFTmidbufferSize);
        p->in_mid_r_raw = fftw_alloc_real(p->FFTmidbufferSize);
        p->out_mid_r = fftw_alloc_complex(p->FFTmidbufferSize / 2 + 1);
        p->p_mid_r =
            fftw_plan_dft_r2c_1d(p->FFTmidbufferSize, p->in_mid_r, p->out_mid_r, FFTW_MEASURE);

        // TREBLE
        p->in_treble_r = fftw_alloc_real(p->FFTtreblebufferSize);
        p->in_treble_r_raw = fftw_alloc_real(p->FFTtreblebufferSize);
        p->out_treble_r = fftw_alloc_complex(p->FFTtreblebufferSize / 2 + 1);

        p->p_treble_r = fftw_plan_dft_r2c_1d(p->FFTtreblebufferSize, p->in_treble_r,
                                             p->out_treble_r, FFTW_MEASURE);

        memset(p->in_bass_r, 0, sizeof(double) * p->FFTbassbufferSize);
        memset(p->in_mid_r, 0, sizeof(double) * p->FFTmidbufferSize);
        memset(p->in_treble_r, 0, sizeof(double) * p->FFTtreblebufferSize);
        memset(p->in_bass_r_raw, 0, sizeof(double) * p->FFTbassbufferSize);
        memset(p->in_mid_r_raw, 0, sizeof(double) * p->FFTmidbufferSize);
        memset(p->in_treble_r_raw, 0, sizeof(double) * p->FFTtreblebufferSize);
        memset(p->out_bass_r, 0, (p->FFTbassbufferSize / 2 + 1) * sizeof(fftw_complex));
        memset(p->out_mid_r, 0, (p->FFTmidbufferSize / 2 + 1) * sizeof(fftw_complex));
        memset(p->out_treble_r, 0, (p->FFTtreblebufferSize / 2 + 1) * sizeof(fftw_complex));
    }

    memset(p->input_buffer, 0, sizeof(int) * p->input_buffer_size);
    memset(p->cava_fall, 0, sizeof(int) * max_bars);
    memset(p->cava_mem, 0, sizeof(int) * max_bars);
    memset(p->cava_peak, 0, sizeof(double) * max_bars);
    memset(p->prev_cava_out, 0, sizeof(int) * max_bars);

    // process: calculate cutoff frequencies and eq
    int lower_cut_off = 50;
    int upper_cut_off = 10000;

    // calculate frequency constant (used to distribute bars across the frequency band)
    double frequency_constant = log10((float)lower_cut_off / (float)upper_cut_off) /
                                (1 / ((float)p->number_of_bars + 1) - 1);

    float relative_cut_off[CAVA_TREBLE_BUFFER_SIZE];

    p->bass_cut_off_bar = -1;
    p->treble_cut_off_bar = -1;
    int first_bar = 1;
    int first_treble_bar = 0;
    int bar_buffer[p->number_of_bars + 1];

    for (int n = 0; n < p->number_of_bars + 1; n++) {
        double bar_distribution_coefficient = frequency_constant * (-1);
        bar_distribution_coefficient +=
            ((float)n + 1) / ((float)p->number_of_bars + 1) * frequency_constant;
        p->cut_off_frequency[n] = upper_cut_off * pow(10, bar_distribution_coefficient);

        if (n > 0) {
            if (p->cut_off_frequency[n - 1] >= p->cut_off_frequency[n] &&
                p->cut_off_frequency[n - 1] > bass_cut_off)
                p->cut_off_frequency[n] =
                    p->cut_off_frequency[n - 1] +
                    (p->cut_off_frequency[n - 1] - p->cut_off_frequency[n - 2]);
        }

        relative_cut_off[n] = p->cut_off_frequency[n] / (p->rate / 2);
        // remember nyquist!, per my calculations this should be rate/2
        // and nyquist freq in M/2 but testing shows it is not...
        // or maybe the nq freq is in M/4

        p->eq[n] = pow(p->cut_off_frequency[n], 1);

        // the numbers that come out of the FFT are verry high
        // the EQ is used to "normalize" them by dividing with this verry huge number
        p->eq[n] /= pow(2, 20);

        p->eq[n] /= log2(p->FFTbassbufferSize);

        if (p->cut_off_frequency[n] < bass_cut_off) {
            // BASS
            bar_buffer[n] = 1;
            p->FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (p->FFTbassbufferSize / 2);
            p->bass_cut_off_bar++;
            p->treble_cut_off_bar++;
            if (p->bass_cut_off_bar > 0)
                first_bar = 0;

            p->eq[n] *= log2(p->FFTbassbufferSize);
        } else if (p->cut_off_frequency[n] > bass_cut_off &&
                   p->cut_off_frequency[n] < treble_cut_off) {
            // MID
            bar_buffer[n] = 2;
            p->FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (p->FFTmidbufferSize / 2);
            p->treble_cut_off_bar++;
            if ((p->treble_cut_off_bar - p->bass_cut_off_bar) == 1) {
                first_bar = 1;
                p->FFTbuffer_upper_cut_off[n - 1] =
                    relative_cut_off[n] * (p->FFTbassbufferSize / 2);
            } else {
                first_bar = 0;
            }

            p->eq[n] *= log2(p->FFTmidbufferSize);
        } else {
            // TREBLE
            bar_buffer[n] = 3;
            p->FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (p->FFTtreblebufferSize / 2);
            first_treble_bar++;
            if (first_treble_bar == 1) {
                first_bar = 1;
                p->FFTbuffer_upper_cut_off[n - 1] = relative_cut_off[n] * (p->FFTmidbufferSize / 2);
            } else {
                first_bar = 0;
            }

            p->eq[n] *= log2(p->FFTtreblebufferSize);
        }

        if (n > 0) {
            if (!first_bar) {
                p->FFTbuffer_upper_cut_off[n - 1] = p->FFTbuffer_lower_cut_off[n] - 1;

                // pushing the spectrum up if the exponential function gets "clumped" in the
                // bass and caluclating new cut off frequencies
                if (p->FFTbuffer_lower_cut_off[n] <= p->FFTbuffer_lower_cut_off[n - 1]) {

                    p->FFTbuffer_lower_cut_off[n] = p->FFTbuffer_lower_cut_off[n - 1] + 1;
                    p->FFTbuffer_upper_cut_off[n - 1] = p->FFTbuffer_lower_cut_off[n] - 1;

                    if (bar_buffer[n] == 1)
                        relative_cut_off[n] = (float)(p->FFTbuffer_lower_cut_off[n]) /
                                              ((float)p->FFTbassbufferSize / 2);
                    else if (bar_buffer[n] == 2)
                        relative_cut_off[n] = (float)(p->FFTbuffer_lower_cut_off[n]) /
                                              ((float)p->FFTmidbufferSize / 2);
                    else if (bar_buffer[n] == 3)
                        relative_cut_off[n] = (float)(p->FFTbuffer_lower_cut_off[n]) /
                                              ((float)p->FFTtreblebufferSize / 2);

                    p->cut_off_frequency[n] = relative_cut_off[n] * ((float)p->rate / 2);
                }
            } else {
                if (p->FFTbuffer_upper_cut_off[n - 1] <= p->FFTbuffer_lower_cut_off[n - 1])
                    p->FFTbuffer_upper_cut_off[n - 1] = p->FFTbuffer_lower_cut_off[n - 1] + 1;
            }
        }
    }

    return p;
}

void cava_execute(double *cava_in, int new_samples, double *cava_out, struct cava_plan *p) {

    // do not overflow
    if (new_samples > p->input_buffer_size) {
        new_samples = p->input_buffer_size;
    }

    int silence = 1;
    if (new_samples > 0) {
        // shifting input buffer
        for (uint16_t n = p->input_buffer_size - 1; n >= new_samples; n--) {
            p->input_buffer[n] = p->input_buffer[n - new_samples];
        }

        // fill the input buffer
        for (uint16_t n = 0; n < new_samples; n++) {
            p->input_buffer[new_samples - n - 1] = cava_in[n];
            if (cava_in[n]) {
                silence = 0;
            }
        }
    }

    // fill the bass, mid and treble buffers
    for (uint16_t n = 0; n < p->FFTbassbufferSize; n++) {
        if (p->audio_channels == 2) {
            p->in_bass_l_raw[n] = p->input_buffer[n * 2];
            p->in_bass_r_raw[n] = p->input_buffer[n * 2 + 1];
        } else {
            p->in_bass_l_raw[n] = p->input_buffer[n];
        }
    }
    for (uint16_t n = 0; n < p->FFTmidbufferSize; n++) {
        if (p->audio_channels == 2) {
            p->in_mid_l_raw[n] = p->input_buffer[n * 2];
            p->in_mid_r_raw[n] = p->input_buffer[n * 2 + 1];
        } else {
            p->in_mid_l_raw[n] = p->input_buffer[n];
        }
    }
    for (uint16_t n = 0; n < p->FFTtreblebufferSize; n++) {
        if (p->audio_channels == 2) {
            p->in_treble_l_raw[n] = p->input_buffer[n * 2];
            p->in_treble_r_raw[n] = p->input_buffer[n * 2 + 1];
        } else {
            p->in_treble_l_raw[n] = p->input_buffer[n];
        }
    }

    // Hann Window
    for (int i = 0; i < p->FFTbassbufferSize; i++) {
        p->in_bass_l[i] = p->bass_multiplier[i] * p->in_bass_l_raw[i];
        if (p->audio_channels == 2)
            p->in_bass_r[i] = p->bass_multiplier[i] * p->in_bass_r_raw[i];
    }
    for (int i = 0; i < p->FFTmidbufferSize; i++) {
        p->in_mid_l[i] = p->mid_multiplier[i] * p->in_mid_l_raw[i];
        if (p->audio_channels == 2)
            p->in_mid_r[i] = p->mid_multiplier[i] * p->in_mid_r_raw[i];
    }
    for (int i = 0; i < p->FFTtreblebufferSize; i++) {
        p->in_treble_l[i] = p->treble_multiplier[i] * p->in_treble_l_raw[i];
        if (p->audio_channels == 2)
            p->in_treble_r[i] = p->treble_multiplier[i] * p->in_treble_r_raw[i];
    }

    // process: execute FFT and sort frequency bands

    fftw_execute(p->p_bass_l);
    fftw_execute(p->p_mid_l);
    fftw_execute(p->p_treble_l);
    if (p->audio_channels == 2) {
        fftw_execute(p->p_bass_r);
        fftw_execute(p->p_mid_r);
        fftw_execute(p->p_treble_r);
    }

    // process: separate frequency bands
    for (int n = 0; n < p->number_of_bars; n++) {

        double temp_l = 0;
        double temp_r = 0;

        // process: add upp FFT values within bands
        for (int i = p->FFTbuffer_lower_cut_off[n]; i <= p->FFTbuffer_upper_cut_off[n]; i++) {

            if (n <= p->bass_cut_off_bar) {
                temp_l += hypot(p->out_bass_l[i][0], p->out_bass_l[i][1]);
                if (p->audio_channels == 2)
                    temp_r += hypot(p->out_bass_r[i][0], p->out_bass_r[i][1]);

            } else if (n > p->bass_cut_off_bar && n <= p->treble_cut_off_bar) {

                temp_l += hypot(p->out_mid_l[i][0], p->out_mid_l[i][1]);
                if (p->audio_channels == 2)
                    temp_r += hypot(p->out_mid_r[i][0], p->out_mid_r[i][1]);

            } else if (n > p->treble_cut_off_bar) {

                temp_l += hypot(p->out_treble_l[i][0], p->out_treble_l[i][1]);
                if (p->audio_channels == 2)
                    temp_r += hypot(p->out_treble_r[i][0], p->out_treble_r[i][1]);
            }
        }

        // getting average multiply with eq
        temp_l /= p->FFTbuffer_upper_cut_off[n] - p->FFTbuffer_lower_cut_off[n] + 1;
        temp_l *= p->eq[n] * p->sens;
        cava_out[n] = temp_l;

        if (p->audio_channels == 2) {
            temp_r /= p->FFTbuffer_upper_cut_off[n] - p->FFTbuffer_lower_cut_off[n] + 1;
            temp_r *= p->eq[n] * p->sens;
            cava_out[n + p->number_of_bars] = temp_r;
        }
    }
    // process [smoothing]
    int overshoot = 0;
    for (int n = 0; n < p->number_of_bars * p->audio_channels; n++) {

        // process [smoothing]: falloff
        if (cava_out[n] < p->prev_cava_out[n]) {
            cava_out[n] = p->cava_peak[n] - (p->g * p->cava_fall[n] * p->cava_fall[n]);
            if (cava_out[n] < 0)
                cava_out[n] = 0;
            p->cava_fall[n]++;
        } else {
            p->cava_peak[n] = cava_out[n];
            p->cava_fall[n] = 0;
        }
        p->prev_cava_out[n] = cava_out[n];

        // process [smoothing]: integral
        cava_out[n] = p->cava_mem[n] * 0.77 + cava_out[n];
        p->cava_mem[n] = cava_out[n];
        int diff = p->height - cava_out[n];
        if (diff < 0)
            diff = 0;
        double div = 1 / (diff + 1);
        p->cava_mem[n] = p->cava_mem[n] * (1 - div / 20);

        // check if we overshoot target height
        if (cava_out[n] > p->height) {
            overshoot = 1;
        }
    }

    // calculating automatic sense adjustment
    if (overshoot) {
        p->sens = p->sens * 0.98;
        p->sens_init = 0;
    } else {
        if (!silence) {
            p->sens = p->sens * 1.001;
            if (p->sens_init)
                p->sens = p->sens * 1.1;
        }
    }
}

void cava_destroy(struct cava_plan *p) {

    fftw_free(p->in_bass_l);
    fftw_free(p->in_bass_l_raw);
    fftw_free(p->out_bass_l);
    fftw_destroy_plan(p->p_bass_l);

    fftw_free(p->in_mid_l);
    fftw_free(p->in_mid_l_raw);
    fftw_free(p->out_mid_l);
    fftw_destroy_plan(p->p_mid_l);

    fftw_free(p->in_treble_l);
    fftw_free(p->in_treble_r_raw);
    fftw_free(p->in_treble_l_raw);
    fftw_free(p->out_treble_l);
    fftw_destroy_plan(p->p_treble_l);

    if (p->audio_channels == 2) {
        fftw_free(p->in_bass_r);
        fftw_free(p->in_bass_r_raw);
        fftw_free(p->out_bass_r);
        fftw_destroy_plan(p->p_bass_r);

        fftw_free(p->in_mid_r);
        fftw_free(p->in_mid_r_raw);
        fftw_free(p->out_mid_r);
        fftw_destroy_plan(p->p_mid_r);

        fftw_free(p->in_treble_r);
        fftw_free(p->out_treble_r);
        fftw_destroy_plan(p->p_treble_r);
    }

    free(p->input_buffer);
    free(p->bass_multiplier);
    free(p->mid_multiplier);
    free(p->treble_multiplier);
    free(p->eq);
    free(p->cut_off_frequency);
    free(p->FFTbuffer_lower_cut_off);
    free(p->FFTbuffer_upper_cut_off);
    free(p->cava_fall);
    free(p->cava_mem);
    free(p->cava_peak);
    free(p->prev_cava_out);
}