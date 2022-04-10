#include "cavacore.h"
#ifndef M_PI
#define M_PI 3.1415926535897932385
#endif
#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CAVA_TREBLE_BUFFER_SIZE 1024

struct cava_plan *cava_init(int number_of_bars, unsigned int rate, int channels, int autosens,
                            double noise_reduction, int low_cut_off, int high_cut_off) {

    // sanity checks:
    if (channels < 1 || channels > 2) {
        fprintf(stderr,
                "cava_init called with illegal number of channels: %d, number of channels "
                "supported are "
                "1 and 2\n",
                channels);
        exit(1);
    }
    if (rate < 1 || rate > 384000) {
        fprintf(stderr, "cava_init called with illegal sample rate: %d\n", rate);
        exit(1);
    }

    int treble_buffer_size = 128;

    if (rate > 8125 && rate <= 16250)
        treble_buffer_size = 256;
    else if (rate > 16250 && rate <= 32500)
        treble_buffer_size = 512;
    else if (rate > 32500 && rate <= 75000)
        treble_buffer_size = 1024;
    else if (rate > 75000 && rate <= 150000)
        treble_buffer_size = 2048;
    else if (rate > 150000 && rate <= 300000)
        treble_buffer_size = 4096;
    else if (rate > 300000)
        treble_buffer_size = 8096;

    if (number_of_bars < 1) {
        fprintf(stderr,
                "cava_init called with illegal number of bars: %d, number of channels must be "
                "positive integer\n",
                number_of_bars);
        exit(1);
    }

    if (number_of_bars > treble_buffer_size / 2 + 1) {
        fprintf(stderr,
                "cava_init called with illegal number of bars: %d, for %d sample rate number of "
                "bars can't be more than %d "
                "positive integer\n",
                number_of_bars, rate, treble_buffer_size / 2 + 1);
        exit(1);
    }
    if (low_cut_off < 0 || high_cut_off < 0) {
        fprintf(stderr, "low_cut_off must be a positive value\n");
        exit(1);
    }
    if (low_cut_off >= high_cut_off) {
        fprintf(stderr, "high_cut_off must be a higher than low_cut_off\n");
        exit(1);
    }
    if ((unsigned int)high_cut_off > rate / 2) {
        fprintf(stderr,
                "high_cut_off can't be higher than sample rate / 2. (Nyquist Sampling Theorem)\n");
        exit(1);
    }

    struct cava_plan *p = malloc(sizeof(struct cava_plan));
    p->number_of_bars = number_of_bars;
    p->audio_channels = channels;
    p->rate = rate;
    p->autosens = 1;
    p->sens = 1;
    p->autosens = autosens;
    p->framerate = 75;
    p->frame_skip = 1;
    p->average_max = 0;
    p->noise_reduction = noise_reduction;

    p->g = log10((float)p->height) * 0.05;

    p->FFTbassbufferSize = treble_buffer_size * 4;
    p->FFTmidbufferSize = treble_buffer_size * 2;
    p->FFTtreblebufferSize = treble_buffer_size;

    p->input_buffer_size = p->FFTbassbufferSize * channels;

    p->input_buffer = (double *)malloc(p->input_buffer_size * sizeof(double));

    p->FFTbuffer_lower_cut_off = (int *)malloc((number_of_bars + 1) * sizeof(int));
    p->FFTbuffer_upper_cut_off = (int *)malloc((number_of_bars + 1) * sizeof(int));
    p->eq = (double *)malloc((number_of_bars + 1) * sizeof(double));
    p->cut_off_frequency = (float *)malloc((number_of_bars + 1) * sizeof(float));

    p->cava_fall = (int *)malloc(number_of_bars * channels * sizeof(int));
    p->cava_mem = (double *)malloc(number_of_bars * channels * sizeof(double));
    p->cava_peak = (double *)malloc(number_of_bars * channels * sizeof(double));
    p->prev_cava_out = (double *)malloc(number_of_bars * channels * sizeof(double));

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

    memset(p->input_buffer, 0, sizeof(double) * p->input_buffer_size);

    memset(p->cava_fall, 0, sizeof(int) * number_of_bars * channels);
    memset(p->cava_mem, 0, sizeof(double) * number_of_bars * channels);
    memset(p->cava_peak, 0, sizeof(double) * number_of_bars * channels);
    memset(p->prev_cava_out, 0, sizeof(double) * number_of_bars * channels);

    // process: calculate cutoff frequencies and eq
    int lower_cut_off = low_cut_off;
    int upper_cut_off = high_cut_off;
    int bass_cut_off = 150;
    int treble_cut_off = 2500;

    // calculate frequency constant (used to distribute bars across the frequency band)
    double frequency_constant = log10((float)lower_cut_off / (float)upper_cut_off) /
                                (1 / ((float)p->number_of_bars + 1) - 1);

    float relative_cut_off[p->FFTtreblebufferSize];

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
            if (p->FFTbuffer_lower_cut_off[n] > p->FFTbassbufferSize / 2) {
                p->FFTbuffer_lower_cut_off[n] = p->FFTbassbufferSize / 2;
            }
        } else if (p->cut_off_frequency[n] > bass_cut_off &&
                   p->cut_off_frequency[n] < treble_cut_off) {
            // MID
            bar_buffer[n] = 2;
            p->FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (p->FFTmidbufferSize / 2);
            p->treble_cut_off_bar++;
            if ((p->treble_cut_off_bar - p->bass_cut_off_bar) == 1) {
                first_bar = 1;
                if (n > 0) {
                    p->FFTbuffer_upper_cut_off[n - 1] =
                        relative_cut_off[n] * (p->FFTbassbufferSize / 2);
                }
            } else {
                first_bar = 0;
            }

            p->eq[n] *= log2(p->FFTmidbufferSize);
            if (p->FFTbuffer_lower_cut_off[n] > p->FFTmidbufferSize / 2) {
                p->FFTbuffer_lower_cut_off[n] = p->FFTmidbufferSize / 2;
            }
        } else {
            // TREBLE
            bar_buffer[n] = 3;
            p->FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (p->FFTtreblebufferSize / 2);
            first_treble_bar++;
            if (first_treble_bar == 1) {
                first_bar = 1;
                if (n > 0) {
                    p->FFTbuffer_upper_cut_off[n - 1] =
                        relative_cut_off[n] * (p->FFTmidbufferSize / 2);
                }
            } else {
                first_bar = 0;
            }

            p->eq[n] *= log2(p->FFTtreblebufferSize);
            if (p->FFTbuffer_lower_cut_off[n] > p->FFTtreblebufferSize / 2) {
                p->FFTbuffer_lower_cut_off[n] = p->FFTtreblebufferSize / 2;
            }
        }

        if (n > 0) {
            if (!first_bar) {
                p->FFTbuffer_upper_cut_off[n - 1] = p->FFTbuffer_lower_cut_off[n] - 1;

                // pushing the spectrum up if the exponential function gets "clumped" in the
                // bass and caluclating new cut off frequencies
                if (p->FFTbuffer_lower_cut_off[n] <= p->FFTbuffer_lower_cut_off[n - 1]) {

                    // check if there is room for more first
                    int room_for_more = 0;

                    if (bar_buffer[n] == 1) {
                        if (p->FFTbuffer_lower_cut_off[n - 1] + 1 < p->FFTbassbufferSize / 2 + 1)
                            room_for_more = 1;
                    } else if (bar_buffer[n] == 2) {
                        if (p->FFTbuffer_lower_cut_off[n - 1] + 1 < p->FFTmidbufferSize / 2 + 1)
                            room_for_more = 1;
                    } else if (bar_buffer[n] == 3) {
                        if (p->FFTbuffer_lower_cut_off[n - 1] + 1 < p->FFTtreblebufferSize / 2 + 1)
                            room_for_more = 1;
                    }

                    if (room_for_more) {
                        // push the spectrum up
                        p->FFTbuffer_lower_cut_off[n] = p->FFTbuffer_lower_cut_off[n - 1] + 1;
                        p->FFTbuffer_upper_cut_off[n - 1] = p->FFTbuffer_lower_cut_off[n] - 1;

                        // calculate new cut off frequency
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
        p->framerate -= p->framerate / 64;
        p->framerate += (double)((p->rate * p->audio_channels * p->frame_skip) / new_samples) / 64;
        p->frame_skip = 1;
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
    } else {
        p->frame_skip++;
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
        temp_l *= p->eq[n];
        cava_out[n] = temp_l;

        if (p->audio_channels == 2) {
            temp_r /= p->FFTbuffer_upper_cut_off[n] - p->FFTbuffer_lower_cut_off[n] + 1;
            temp_r *= p->eq[n];
            cava_out[n + p->number_of_bars] = temp_r;
        }
    }

    // applying sens or getting max value
    for (int n = 0; n < p->number_of_bars * p->audio_channels; n++) {
        if (p->autosens) {
            cava_out[n] *= p->sens;
        } else {
            if (cava_out[n] > p->average_max) {
                p->average_max -= p->average_max / 64;
                p->average_max += cava_out[n] / 64;
            }
        }
    }
    // process [smoothing]
    int overshoot = 0;
    double gravity_mod = pow((60 / p->framerate), 2.5) * 1.54 / p->noise_reduction;

    if (gravity_mod < 1)
        gravity_mod = 1;

    for (int n = 0; n < p->number_of_bars * p->audio_channels; n++) {

        // process [smoothing]: falloff
        if (cava_out[n] < p->prev_cava_out[n] && p->noise_reduction > 0.1) {
            cava_out[n] =
                p->cava_peak[n] * (1000 - (p->cava_fall[n] * p->cava_fall[n] * gravity_mod)) / 1000;

            //    p->cava_peak[n] / (gravity_mod * log10(p->cava_fall[n]));
            if (cava_out[n] < 0)
                cava_out[n] = 0;
            p->cava_fall[n]++;
        } else {
            p->cava_peak[n] = cava_out[n];
            p->cava_fall[n] = 0;
        }
        p->prev_cava_out[n] = cava_out[n];

        // process [smoothing]: integral
        cava_out[n] = p->cava_mem[n] * p->noise_reduction + cava_out[n];
        p->cava_mem[n] = cava_out[n];
        if (p->autosens) {
            double diff = 1000 - cava_out[n];
            if (diff < 0)
                diff = 0;
            double div = 1 / (diff + 1);
            p->cava_mem[n] = p->cava_mem[n] * (1 - div / 20);

            // check if we overshoot target height
            if (cava_out[n] > 1000) {
                overshoot = 1;
            }
            cava_out[n] /= 1000;
        }
    }

    // calculating automatic sense adjustment
    if (p->autosens) {
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
        fftw_free(p->in_treble_r_raw);
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