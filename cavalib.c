#include "cavalib.h"
#ifndef M_PI
#define M_PI 3.1415926535897932385
#endif
#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

int FFTbassbufferSize;
int FFTmidbufferSize;
int FFTtreblebufferSize;

double *bass_multiplier;
double *mid_multiplier;
double *treble_multiplier;

double *in_bass_r_raw, *in_bass_l_raw;
double *in_mid_r_raw, *in_mid_l_raw;
double *in_treble_r_raw, *in_treble_l_raw;
double *in_bass_r, *in_bass_l;
double *in_mid_r, *in_mid_l;
double *in_treble_r, *in_treble_l;

fftw_complex *out_bass_l, *out_bass_r;
fftw_plan p_bass_l, p_bass_r;
fftw_complex *out_mid_l, *out_mid_r;
fftw_plan p_mid_l, p_mid_r;
fftw_complex *out_treble_l, *out_treble_r;
fftw_plan p_treble_l, p_treble_r;

int FFTbuffer_lower_cut_off[MAX_BARS];
int FFTbuffer_upper_cut_off[MAX_BARS];
double eq[MAX_BARS];
int bass_cut_off_bar;
int treble_cut_off_bar;
int fftw_input_buffer_size;

int *fftw_input_buffer;

float *cava_plan(int number_of_bars, unsigned int rate) {

    int bass_cut_off = 150;
    int treble_cut_off = 2500;

    FFTbassbufferSize = MAX_BARS * 4;
    FFTmidbufferSize = MAX_BARS * 2;
    FFTtreblebufferSize = MAX_BARS;

    fftw_input_buffer_size = FFTbassbufferSize * 2;

    fftw_input_buffer = (int *)malloc(fftw_input_buffer_size * sizeof(int));

    // Hann Window calculate multipliers
    bass_multiplier = (double *)malloc(FFTbassbufferSize * sizeof(double));
    mid_multiplier = (double *)malloc(FFTmidbufferSize * sizeof(double));
    treble_multiplier = (double *)malloc(FFTtreblebufferSize * sizeof(double));
    for (int i = 0; i < FFTbassbufferSize; i++) {
        bass_multiplier[i] = 0.5 * (1 - cos(2 * M_PI * i / (FFTbassbufferSize - 1)));
    }
    for (int i = 0; i < FFTmidbufferSize; i++) {
        mid_multiplier[i] = 0.5 * (1 - cos(2 * M_PI * i / (FFTmidbufferSize - 1)));
    }
    for (int i = 0; i < FFTtreblebufferSize; i++) {
        treble_multiplier[i] = 0.5 * (1 - cos(2 * M_PI * i / (FFTtreblebufferSize - 1)));
    }
    // BASS
    // audio.FFTbassbufferSize =  audio.rate / 20; // audio.FFTbassbufferSize;

    in_bass_r = fftw_alloc_real(FFTbassbufferSize);
    in_bass_l = fftw_alloc_real(FFTbassbufferSize);
    in_bass_r_raw = fftw_alloc_real(FFTbassbufferSize);
    in_bass_l_raw = fftw_alloc_real(FFTbassbufferSize);

    out_bass_l = fftw_alloc_complex(FFTbassbufferSize / 2 + 1);
    out_bass_r = fftw_alloc_complex(FFTbassbufferSize / 2 + 1);
    memset(out_bass_l, 0, (FFTbassbufferSize / 2 + 1) * sizeof(fftw_complex));
    memset(out_bass_r, 0, (FFTbassbufferSize / 2 + 1) * sizeof(fftw_complex));

    p_bass_l = fftw_plan_dft_r2c_1d(FFTbassbufferSize, in_bass_l, out_bass_l, FFTW_MEASURE);
    p_bass_r = fftw_plan_dft_r2c_1d(FFTbassbufferSize, in_bass_r, out_bass_r, FFTW_MEASURE);

    // MID
    // FFTmidbufferSize =  rate / bass_cut_off; // FFTbassbufferSize;
    in_mid_r = fftw_alloc_real(FFTmidbufferSize);
    in_mid_l = fftw_alloc_real(FFTmidbufferSize);
    in_mid_r_raw = fftw_alloc_real(FFTmidbufferSize);
    in_mid_l_raw = fftw_alloc_real(FFTmidbufferSize);

    out_mid_l = fftw_alloc_complex(FFTmidbufferSize / 2 + 1);
    out_mid_r = fftw_alloc_complex(FFTmidbufferSize / 2 + 1);
    memset(out_mid_l, 0, (FFTmidbufferSize / 2 + 1) * sizeof(fftw_complex));
    memset(out_mid_r, 0, (FFTmidbufferSize / 2 + 1) * sizeof(fftw_complex));

    p_mid_l = fftw_plan_dft_r2c_1d(FFTmidbufferSize, in_mid_l, out_mid_l, FFTW_MEASURE);
    p_mid_r = fftw_plan_dft_r2c_1d(FFTmidbufferSize, in_mid_r, out_mid_r, FFTW_MEASURE);

    // TRIEBLE
    // FFTtreblebufferSize =  rate / treble_cut_off; // FFTbassbufferSize;
    in_treble_r = fftw_alloc_real(FFTtreblebufferSize);
    in_treble_l = fftw_alloc_real(FFTtreblebufferSize);
    in_treble_r_raw = fftw_alloc_real(FFTtreblebufferSize);
    in_treble_l_raw = fftw_alloc_real(FFTtreblebufferSize);

    out_treble_l = fftw_alloc_complex(FFTtreblebufferSize / 2 + 1);
    out_treble_r = fftw_alloc_complex(FFTtreblebufferSize / 2 + 1);
    memset(out_treble_l, 0, (FFTtreblebufferSize / 2 + 1) * sizeof(fftw_complex));
    memset(out_treble_r, 0, (FFTtreblebufferSize / 2 + 1) * sizeof(fftw_complex));

    p_treble_l = fftw_plan_dft_r2c_1d(FFTtreblebufferSize, in_treble_l, out_treble_l, FFTW_MEASURE);
    p_treble_r = fftw_plan_dft_r2c_1d(FFTtreblebufferSize, in_treble_r, out_treble_r, FFTW_MEASURE);

    memset(in_bass_r, 0, sizeof(double) * FFTbassbufferSize);
    memset(in_bass_l, 0, sizeof(double) * FFTbassbufferSize);
    memset(in_mid_r, 0, sizeof(double) * FFTmidbufferSize);
    memset(in_mid_l, 0, sizeof(double) * FFTmidbufferSize);
    memset(in_treble_r, 0, sizeof(double) * FFTtreblebufferSize);
    memset(in_treble_l, 0, sizeof(double) * FFTtreblebufferSize);
    memset(in_bass_r_raw, 0, sizeof(double) * FFTbassbufferSize);
    memset(in_bass_l_raw, 0, sizeof(double) * FFTbassbufferSize);
    memset(in_mid_r_raw, 0, sizeof(double) * FFTmidbufferSize);
    memset(in_mid_l_raw, 0, sizeof(double) * FFTmidbufferSize);
    memset(in_treble_r_raw, 0, sizeof(double) * FFTtreblebufferSize);
    memset(in_treble_l_raw, 0, sizeof(double) * FFTtreblebufferSize);

    memset(fftw_input_buffer, 0, sizeof(int) * fftw_input_buffer_size);

    // process: calculate cutoff frequencies and eq
    int lower_cut_off = 50;
    int upper_cut_off = 10000;

    // calculate frequency constant (used to distribute bars across the frequency band)
    double frequency_constant =
        log10((float)lower_cut_off / (float)upper_cut_off) / (1 / ((float)number_of_bars + 1) - 1);

    float *cut_off_frequency;
    cut_off_frequency = (float *)malloc(MAX_BARS * sizeof(float));

    float relative_cut_off[MAX_BARS];

    bool first_bar = true;
    int first_treble_bar = 0;
    int bar_buffer[number_of_bars + 1];

    for (int n = 0; n < number_of_bars + 1; n++) {
        double bar_distribution_coefficient = frequency_constant * (-1);
        bar_distribution_coefficient +=
            ((float)n + 1) / ((float)number_of_bars + 1) * frequency_constant;
        cut_off_frequency[n] = upper_cut_off * pow(10, bar_distribution_coefficient);

        if (n > 0) {
            if (cut_off_frequency[n - 1] >= cut_off_frequency[n] &&
                cut_off_frequency[n - 1] > bass_cut_off)
                cut_off_frequency[n] = cut_off_frequency[n - 1] +
                                       (cut_off_frequency[n - 1] - cut_off_frequency[n - 2]);
        }

        relative_cut_off[n] = cut_off_frequency[n] / (rate / 2);
        // remember nyquist!, per my calculations this should be rate/2
        // and nyquist freq in M/2 but testing shows it is not...
        // or maybe the nq freq is in M/4

        eq[n] = pow(cut_off_frequency[n], 1);

        // the numbers that come out of the FFT are verry high
        // the EQ is used to "normalize" them by dividing with this verry huge number
        eq[n] /= pow(2, 20);

        eq[n] /= log2(FFTbassbufferSize);

        if (cut_off_frequency[n] < bass_cut_off) {
            // BASS
            bar_buffer[n] = 1;
            FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (FFTbassbufferSize / 2);
            bass_cut_off_bar++;
            treble_cut_off_bar++;
            if (bass_cut_off_bar > 0)
                first_bar = false;

            eq[n] *= log2(FFTbassbufferSize);
        } else if (cut_off_frequency[n] > bass_cut_off && cut_off_frequency[n] < treble_cut_off) {
            // MID
            bar_buffer[n] = 2;
            FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (FFTmidbufferSize / 2);
            treble_cut_off_bar++;
            if ((treble_cut_off_bar - bass_cut_off_bar) == 1) {
                first_bar = true;
                FFTbuffer_upper_cut_off[n - 1] = relative_cut_off[n] * (FFTbassbufferSize / 2);
            } else {
                first_bar = false;
            }

            eq[n] *= log2(FFTmidbufferSize);
        } else {
            // TREBLE
            bar_buffer[n] = 3;
            FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (FFTtreblebufferSize / 2);
            first_treble_bar++;
            if (first_treble_bar == 1) {
                first_bar = true;
                FFTbuffer_upper_cut_off[n - 1] = relative_cut_off[n] * (FFTmidbufferSize / 2);
            } else {
                first_bar = false;
            }

            eq[n] *= log2(FFTtreblebufferSize);
        }

        if (n > 0) {
            if (!first_bar) {
                FFTbuffer_upper_cut_off[n - 1] = FFTbuffer_lower_cut_off[n] - 1;

                // pushing the spectrum up if the exponential function gets "clumped" in the
                // bass and caluclating new cut off frequencies
                if (FFTbuffer_lower_cut_off[n] <= FFTbuffer_lower_cut_off[n - 1]) {

                    FFTbuffer_lower_cut_off[n] = FFTbuffer_lower_cut_off[n - 1] + 1;
                    FFTbuffer_upper_cut_off[n - 1] = FFTbuffer_lower_cut_off[n] - 1;

                    if (bar_buffer[n] == 1)
                        relative_cut_off[n] =
                            (float)(FFTbuffer_lower_cut_off[n]) / ((float)FFTbassbufferSize / 2);
                    else if (bar_buffer[n] == 2)
                        relative_cut_off[n] =
                            (float)(FFTbuffer_lower_cut_off[n]) / ((float)FFTmidbufferSize / 2);
                    else if (bar_buffer[n] == 3)
                        relative_cut_off[n] =
                            (float)(FFTbuffer_lower_cut_off[n]) / ((float)FFTtreblebufferSize / 2);

                    cut_off_frequency[n] = relative_cut_off[n] * ((float)rate / 2);
                }
            } else {
                if (FFTbuffer_upper_cut_off[n - 1] <= FFTbuffer_lower_cut_off[n - 1])
                    FFTbuffer_upper_cut_off[n - 1] = FFTbuffer_lower_cut_off[n - 1] + 1;
            }
        }
    }

    return cut_off_frequency;
}

void cava_execute(int32_t *cava_in, double *cava_out, int number_of_bars, int stereo) {

    int samples = MAX_BARS;

    if (stereo) {
        number_of_bars /= 2;
        samples *= 2;
    }

    // shifting input buffer
    for (uint16_t n = fftw_input_buffer_size - 1; n >= samples; n--) {
        fftw_input_buffer[n] = fftw_input_buffer[n - samples];
    }

    // fill the input buffer
    for (uint16_t i = 0; i < samples; i++) {
        fftw_input_buffer[i] = cava_in[i];
    }

    // fill the bass, mid and treble buffers
    for (uint16_t n = 0; n < FFTbassbufferSize; n++) {
        if (stereo) {
            in_bass_l_raw[n] = fftw_input_buffer[n * 2];
            in_bass_r_raw[n] = fftw_input_buffer[n * 2 + 1];
        } else {
            in_bass_l_raw[n] = fftw_input_buffer[n];
        }
    }
    for (uint16_t n = 0; n < FFTmidbufferSize; n++) {
        if (stereo) {
            in_mid_l_raw[n] = fftw_input_buffer[n * 2];
            in_mid_r_raw[n] = fftw_input_buffer[n * 2 + 1];
        } else {
            in_mid_l_raw[n] = fftw_input_buffer[n];
        }
    }
    for (uint16_t n = 0; n < FFTtreblebufferSize; n++) {
        if (stereo) {
            in_treble_l_raw[n] = fftw_input_buffer[n * 2];
            in_treble_r_raw[n] = fftw_input_buffer[n * 2 + 1];
        } else {
            in_treble_l_raw[n] = fftw_input_buffer[n];
        }
    }

    // Hann Window
    for (int i = 0; i < FFTbassbufferSize; i++) {
        in_bass_l[i] = bass_multiplier[i] * in_bass_l_raw[i];
        in_bass_r[i] = bass_multiplier[i] * in_bass_r_raw[i];
    }
    for (int i = 0; i < FFTmidbufferSize; i++) {
        in_mid_l[i] = mid_multiplier[i] * in_mid_l_raw[i];
        in_mid_r[i] = mid_multiplier[i] * in_mid_r_raw[i];
    }
    for (int i = 0; i < FFTtreblebufferSize; i++) {
        in_treble_l[i] = treble_multiplier[i] * in_treble_l_raw[i];
        in_treble_r[i] = treble_multiplier[i] * in_treble_r_raw[i];
    }

    // process: execute FFT and sort frequency bands

    fftw_execute(p_bass_l);
    fftw_execute(p_mid_l);
    fftw_execute(p_treble_l);
    fftw_execute(p_bass_r);
    fftw_execute(p_mid_r);
    fftw_execute(p_treble_r);

    // process: separate frequency bands
    for (int n = 0; n < number_of_bars; n++) {

        double temp_l = 0;
        double temp_r = 0;

        // process: add upp FFT values within bands
        for (int i = FFTbuffer_lower_cut_off[n]; i <= FFTbuffer_upper_cut_off[n]; i++) {

            if (n <= bass_cut_off_bar) {

                temp_l += hypot(out_bass_l[i][0], out_bass_l[i][1]);
                temp_r += hypot(out_bass_r[i][0], out_bass_r[i][1]);

            } else if (n > bass_cut_off_bar && n <= treble_cut_off_bar) {

                temp_l += hypot(out_mid_l[i][0], out_mid_l[i][1]);
                temp_r += hypot(out_mid_r[i][0], out_mid_r[i][1]);

            } else if (n > treble_cut_off_bar) {

                temp_l += hypot(out_treble_l[i][0], out_treble_l[i][1]);
                temp_r += hypot(out_treble_r[i][0], out_treble_r[i][1]);
            }
        }

        // getting average multiply with eq
        temp_l /= FFTbuffer_upper_cut_off[n] - FFTbuffer_lower_cut_off[n] + 1;
        temp_l *= eq[n];

        temp_r /= FFTbuffer_upper_cut_off[n] - FFTbuffer_lower_cut_off[n] + 1;
        temp_r *= eq[n];

        cava_out[n] = (int)temp_l;

        cava_out[n + number_of_bars] = (int)temp_r;
    }
}

void cava_destroy(void) {

    fftw_free(in_bass_r);
    fftw_free(in_bass_l);
    fftw_free(out_bass_r);
    fftw_free(out_bass_l);
    fftw_destroy_plan(p_bass_l);
    fftw_destroy_plan(p_bass_r);

    fftw_free(in_mid_r);
    fftw_free(in_mid_l);
    fftw_free(out_mid_r);
    fftw_free(out_mid_l);
    fftw_destroy_plan(p_mid_l);
    fftw_destroy_plan(p_mid_r);

    fftw_free(in_treble_r);
    fftw_free(in_treble_l);
    fftw_free(out_treble_r);
    fftw_free(out_treble_l);
    fftw_destroy_plan(p_treble_l);
    fftw_destroy_plan(p_treble_r);

    free(fftw_input_buffer);
    free(bass_multiplier);
    free(mid_multiplier);
    free(treble_multiplier);
}