#include "cavacore.h"
#ifndef M_PI
#define M_PI 3.1415926535897932385
#endif
#include <fftw3.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#ifdef __ANDROID__
#include <jni.h>
struct cava_plan *plan;
double *cava_in;
double *cava_out;
#endif

struct cava_plan *cava_init(int number_of_bars, unsigned int rate, int channels, int autosens,
                            double noise_reduction, int low_cut_off, int high_cut_off) {
    struct cava_plan *p = malloc(sizeof(struct cava_plan));
    p->status = 0;

    // sanity checks:
    if (channels < 1 || channels > 2) {
        snprintf(p->error_message, 1024,
                 "cava_init called with illegal number of channels: %d, number of channels "
                 "supported are "
                 "1 and 2",
                 channels);
        p->status = -1;
        return p;
    }
    if (rate < 1 || rate > 384000) {
        snprintf(p->error_message, 1024, "cava_init called with illegal sample rate: %d\n", rate);
        p->status = -1;
        return p;
    }

    int fft_buffer_size = 512;

    if (rate > 8125 && rate <= 16250)
        fft_buffer_size *= 2;
    else if (rate > 16250 && rate <= 32500)
        fft_buffer_size *= 4;
    else if (rate > 32500 && rate <= 75000)
        fft_buffer_size *= 8;
    else if (rate > 75000 && rate <= 150000)
        fft_buffer_size *= 16;
    else if (rate > 150000 && rate <= 300000)
        fft_buffer_size *= 32;
    else if (rate > 300000)
        fft_buffer_size *= 64;

    if (number_of_bars < 1) {
        snprintf(p->error_message, 1024,
                 "cava_init called with illegal number of bars: %d, number of channels must be "
                 "positive integer\n",
                 number_of_bars);
        p->status = -1;
        return p;
    }

    if (number_of_bars > fft_buffer_size / 2 + 1) {
        snprintf(p->error_message, 1024,
                 "cava_init called with illegal number of bars: %d, for %d sample rate number of "
                 "bars can't be more than %d\n",
                 number_of_bars, rate, fft_buffer_size / 2 + 1);
        p->status = -1;
        return p;
    }
    if (low_cut_off < 1 || high_cut_off < 1) {
        snprintf(p->error_message, 1024, "low_cut_off must be a positive value\n");
        p->status = -1;
        return p;
    }
    if (low_cut_off >= high_cut_off) {
        snprintf(p->error_message, 1024, "high_cut_off must be a higher than low_cut_off\n");
        p->status = -1;
        return p;
    }
    if ((unsigned int)high_cut_off > rate / 2) {
        snprintf(p->error_message, 1024,
                 "high_cut_off can't be higher than sample rate / 2. (Nyquist Sampling Theorem)\n");
        p->status = -1;
        return p;
    }

    p->number_of_bars = number_of_bars;
    p->audio_channels = channels;
    p->rate = rate;
    p->autosens = 1;
    p->sens_init = 1;
    p->sens = 1.0;
    p->autosens = autosens;
    p->framerate = 75;
    p->frame_skip = 1;
    p->noise_reduction = noise_reduction;

    int fftw_flag = FFTW_MEASURE;
#ifdef __ANDROID__
    fftw_flag = FFTW_ESTIMATE;
#endif

    p->FFTbassbufferSize = fft_buffer_size * 2;
    p->FFTbufferSize = fft_buffer_size;

    p->input_buffer_size = p->FFTbassbufferSize * channels;

    p->input_buffer = (double *)malloc(p->input_buffer_size * sizeof(double));

    p->FFTbuffer_lower_cut_off = (int *)malloc((number_of_bars + 1) * sizeof(int));
    p->FFTbuffer_upper_cut_off = (int *)malloc((number_of_bars + 1) * sizeof(int));
    p->eq = (double *)malloc((number_of_bars + 1) * sizeof(double));
    p->cut_off_frequency = (float *)malloc((number_of_bars + 1) * sizeof(float));

    p->cava_fall = (double *)malloc(number_of_bars * channels * sizeof(double));
    p->cava_mem = (double *)malloc(number_of_bars * channels * sizeof(double));
    p->cava_peak = (double *)malloc(number_of_bars * channels * sizeof(double));
    p->prev_cava_out = (double *)malloc(number_of_bars * channels * sizeof(double));

    // Hann Window calculate multipliers
    p->bass_multiplier = (double *)malloc(p->FFTbassbufferSize * sizeof(double));
    p->multiplier = (double *)malloc(p->FFTbufferSize * sizeof(double));
    for (int i = 0; i < p->FFTbassbufferSize; i++) {
        p->bass_multiplier[i] = 0.5 * (1 - cos(2 * M_PI * i / (p->FFTbassbufferSize - 1)));
    }
    for (int i = 0; i < p->FFTbufferSize; i++) {
        p->multiplier[i] = 0.5 * (1 - cos(2 * M_PI * i / (p->FFTbufferSize - 1)));
    }

    // BASS
    p->in_bass_l = fftw_alloc_real(p->FFTbassbufferSize);
    p->in_bass_l_raw = fftw_alloc_real(p->FFTbassbufferSize);
    p->out_bass_l = fftw_alloc_complex(p->FFTbassbufferSize / 2 + 1);
    p->p_bass_l =
        fftw_plan_dft_r2c_1d(p->FFTbassbufferSize, p->in_bass_l, p->out_bass_l, fftw_flag);

    // MID + TREBLE
    p->in_l = fftw_alloc_real(p->FFTbufferSize);
    p->in_l_raw = fftw_alloc_real(p->FFTbufferSize);
    p->out_l = fftw_alloc_complex(p->FFTbufferSize / 2 + 1);
    p->p_l = fftw_plan_dft_r2c_1d(p->FFTbufferSize, p->in_l, p->out_l, fftw_flag);

    memset(p->in_bass_l, 0, sizeof(double) * p->FFTbassbufferSize);
    memset(p->in_l, 0, sizeof(double) * p->FFTbufferSize);
    memset(p->in_bass_l_raw, 0, sizeof(double) * p->FFTbassbufferSize);
    memset(p->in_l_raw, 0, sizeof(double) * p->FFTbufferSize);
    memset(p->out_bass_l, 0, (p->FFTbassbufferSize / 2 + 1) * sizeof(fftw_complex));
    memset(p->out_l, 0, (p->FFTbufferSize / 2 + 1) * sizeof(fftw_complex));
    if (p->audio_channels == 2) {
        // BASS
        p->in_bass_r = fftw_alloc_real(p->FFTbassbufferSize);
        p->in_bass_r_raw = fftw_alloc_real(p->FFTbassbufferSize);
        p->out_bass_r = fftw_alloc_complex(p->FFTbassbufferSize / 2 + 1);
        p->p_bass_r =
            fftw_plan_dft_r2c_1d(p->FFTbassbufferSize, p->in_bass_r, p->out_bass_r, fftw_flag);

        // MID + TREBLE
        p->in_r = fftw_alloc_real(p->FFTbufferSize);
        p->in_r_raw = fftw_alloc_real(p->FFTbufferSize);
        p->out_r = fftw_alloc_complex(p->FFTbufferSize / 2 + 1);

        p->p_r = fftw_plan_dft_r2c_1d(p->FFTbufferSize, p->in_r, p->out_r, fftw_flag);

        memset(p->in_bass_r, 0, sizeof(double) * p->FFTbassbufferSize);
        memset(p->in_r, 0, sizeof(double) * p->FFTbufferSize);
        memset(p->in_bass_r_raw, 0, sizeof(double) * p->FFTbassbufferSize);
        memset(p->in_r_raw, 0, sizeof(double) * p->FFTbufferSize);
        memset(p->out_bass_r, 0, (p->FFTbassbufferSize / 2 + 1) * sizeof(fftw_complex));
        memset(p->out_r, 0, (p->FFTbufferSize / 2 + 1) * sizeof(fftw_complex));
    }

    memset(p->input_buffer, 0, sizeof(double) * p->input_buffer_size);

    memset(p->cava_fall, 0, sizeof(int) * number_of_bars * channels);
    memset(p->cava_mem, 0, sizeof(double) * number_of_bars * channels);
    memset(p->cava_peak, 0, sizeof(double) * number_of_bars * channels);
    memset(p->prev_cava_out, 0, sizeof(double) * number_of_bars * channels);

    // process: calculate cutoff frequencies and eq
    int lower_cut_off = low_cut_off;
    int upper_cut_off = high_cut_off;
    int bass_cut_off = 100;

    // calculate frequency constant (used to distribute bars across the frequency band)
    double frequency_constant = log10((float)lower_cut_off / (float)upper_cut_off) /
                                (1 / ((float)p->number_of_bars + 1) - 1);

    float *relative_cut_off = (float *)malloc((p->number_of_bars + 1) * sizeof(float));

    p->bass_cut_off_bar = 0;
    int first_bar = 1;

    float min_bandwidth = p->rate / p->FFTbassbufferSize;

    for (int n = 0; n < p->number_of_bars + 1; n++) {
        double bar_distribution_coefficient = frequency_constant * (-1);
        bar_distribution_coefficient +=
            ((float)n + 1) / ((float)p->number_of_bars + 1) * frequency_constant;
        p->cut_off_frequency[n] = upper_cut_off * pow(10, bar_distribution_coefficient);

        if (n > 0) {
            if (p->cut_off_frequency[n - 1] >= p->cut_off_frequency[n])
                p->cut_off_frequency[n] = p->cut_off_frequency[n - 1] + min_bandwidth;
        }

        // remember nyquist!
        relative_cut_off[n] = p->cut_off_frequency[n] / (p->rate / 2);

        if (p->cut_off_frequency[n] < bass_cut_off) {
            // BASS
            p->FFTbuffer_lower_cut_off[n] = relative_cut_off[n] * (p->FFTbassbufferSize / 2);
            p->bass_cut_off_bar++;
            if (p->bass_cut_off_bar > 1)
                first_bar = 0;

            if (p->FFTbuffer_lower_cut_off[n] > p->FFTbassbufferSize / 2) {
                p->FFTbuffer_lower_cut_off[n] = p->FFTbassbufferSize / 2;
            }
        } else {
            // MID + TREBLE
            p->FFTbuffer_lower_cut_off[n] =
                ceil(relative_cut_off[n] * (float)(p->FFTbufferSize / 2));
            if (n == p->bass_cut_off_bar) {
                first_bar = 1;
                if (n > 0) {
                    p->FFTbuffer_upper_cut_off[n - 1] =
                        relative_cut_off[n] * (p->FFTbassbufferSize / 2) - 1;
                }
            } else {
                first_bar = 0;
            }

            if (p->FFTbuffer_lower_cut_off[n] > p->FFTbufferSize / 2) {
                p->FFTbuffer_lower_cut_off[n] = p->FFTbufferSize / 2;
            }
        }

        if (n > 0) {
            if (!first_bar) {
                p->FFTbuffer_upper_cut_off[n - 1] = p->FFTbuffer_lower_cut_off[n] - 1;

                // pushing the spectrum up if the exponential function gets "clumped" in the
                // bass and calculating new cut off frequencies
                if (p->FFTbuffer_lower_cut_off[n] <= p->FFTbuffer_lower_cut_off[n - 1]) {

                    // check if there is room for more first
                    int room_for_more = 0;

                    if (n < p->bass_cut_off_bar) {
                        if (p->FFTbuffer_lower_cut_off[n - 1] + 1 < p->FFTbassbufferSize / 2 + 1)
                            room_for_more = 1;
                    } else {
                        if (p->FFTbuffer_lower_cut_off[n - 1] + 1 < p->FFTbufferSize / 2 + 1)
                            room_for_more = 1;
                    }

                    if (room_for_more) {
                        // push the spectrum up
                        p->FFTbuffer_lower_cut_off[n] = p->FFTbuffer_lower_cut_off[n - 1] + 1;
                        p->FFTbuffer_upper_cut_off[n - 1] = p->FFTbuffer_lower_cut_off[n] - 1;
                    }
                }
            } else {
                if (p->FFTbuffer_upper_cut_off[n - 1] < p->FFTbuffer_lower_cut_off[n - 1])
                    p->FFTbuffer_upper_cut_off[n - 1] = p->FFTbuffer_lower_cut_off[n - 1] + 1;
            }
        }
        // calculate actual cut off frequency
        if (n < p->bass_cut_off_bar)
            relative_cut_off[n] =
                (float)(p->FFTbuffer_lower_cut_off[n]) / ((float)p->FFTbassbufferSize / 2);
        else
            relative_cut_off[n] =
                (float)(p->FFTbuffer_lower_cut_off[n]) / ((float)p->FFTbufferSize / 2);

        p->cut_off_frequency[n] = relative_cut_off[n] * ((float)p->rate / 2);
    }

    // hard coded eq
    for (int n = 0; n < p->number_of_bars; n++) {

        // the numbers that come out of the FFT are very high
        // the EQ is used to "normalize" them by dividing with this very huge number
        p->eq[n] = 1 / pow(2, 28);

        // need to boost the EQ for higher frequencies
        p->eq[n] *= pow(p->cut_off_frequency[n + 1], 0.85);

        if (n < p->bass_cut_off_bar) {
            p->eq[n] /= log2(p->FFTbassbufferSize);
        } else {
            p->eq[n] /= log2(p->FFTbufferSize);
        }

        p->eq[n] /= p->FFTbuffer_upper_cut_off[n] - p->FFTbuffer_lower_cut_off[n] + 1;
    }
    free(relative_cut_off);
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
        for (int n = p->input_buffer_size - 1; n >= new_samples; n--) {
            p->input_buffer[n] = p->input_buffer[n - new_samples];
        }

        // fill the input buffer
        for (int n = 0; n < new_samples; n++) {
            p->input_buffer[new_samples - n - 1] = cava_in[n];
            if (cava_in[n]) {
                silence = 0;
            }
        }
    } else {
        p->frame_skip++;
    }

    // fill the bass, mid and treble buffers
    for (int n = 0; n < p->FFTbassbufferSize; n++) {
        if (p->audio_channels == 2) {
            p->in_bass_r_raw[n] = p->input_buffer[n * 2];
            p->in_bass_l_raw[n] = p->input_buffer[n * 2 + 1];
        } else {
            p->in_bass_l_raw[n] = p->input_buffer[n];
        }
    }
    for (int n = 0; n < p->FFTbufferSize; n++) {
        if (p->audio_channels == 2) {
            p->in_r_raw[n] = p->input_buffer[n * 2];
            p->in_l_raw[n] = p->input_buffer[n * 2 + 1];
        } else {
            p->in_l_raw[n] = p->input_buffer[n];
        }
    }

    // Hann Window
    for (int i = 0; i < p->FFTbassbufferSize; i++) {
        p->in_bass_l[i] = p->bass_multiplier[i] * p->in_bass_l_raw[i];
        if (p->audio_channels == 2)
            p->in_bass_r[i] = p->bass_multiplier[i] * p->in_bass_r_raw[i];
    }
    for (int i = 0; i < p->FFTbufferSize; i++) {
        p->in_l[i] = p->multiplier[i] * p->in_l_raw[i];
        if (p->audio_channels == 2)
            p->in_r[i] = p->multiplier[i] * p->in_r_raw[i];
    }

    // process: execute FFT and sort frequency bands

    fftw_execute(p->p_bass_l);
    fftw_execute(p->p_l);
    if (p->audio_channels == 2) {
        fftw_execute(p->p_bass_r);
        fftw_execute(p->p_r);
    }

    // process: separate frequency bands
    for (int n = 0; n < p->number_of_bars; n++) {

        double temp_l = 0;
        double temp_r = 0;

        // process: add upp FFT values within bands
        for (int i = p->FFTbuffer_lower_cut_off[n]; i <= p->FFTbuffer_upper_cut_off[n]; i++) {

            if (n < p->bass_cut_off_bar) {
                temp_l += hypot(p->out_bass_l[i][0], p->out_bass_l[i][1]);
                if (p->audio_channels == 2)
                    temp_r += hypot(p->out_bass_r[i][0], p->out_bass_r[i][1]);

            } else {
                temp_l += hypot(p->out_l[i][0], p->out_l[i][1]);
                if (p->audio_channels == 2)
                    temp_r += hypot(p->out_r[i][0], p->out_r[i][1]);
            }
        }

        // getting average multiply with eq
        temp_l *= p->eq[n];
        cava_out[n] = temp_l;

        if (p->audio_channels == 2) {
            temp_r *= p->eq[n];
            cava_out[n + p->number_of_bars] = temp_r;
        }
    }

    // applying sens or getting max value
    if (p->autosens) {
        for (int n = 0; n < p->number_of_bars * p->audio_channels; n++) {
            cava_out[n] *= p->sens;
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
                p->cava_peak[n] * (1.0 - (p->cava_fall[n] * p->cava_fall[n] * gravity_mod));

            if (cava_out[n] < 0.0)
                cava_out[n] = 0.0;
            p->cava_fall[n] += 0.028;
        } else {
            p->cava_peak[n] = cava_out[n];
            p->cava_fall[n] = 0.0;
        }
        p->prev_cava_out[n] = cava_out[n];

        // process [smoothing]: integral
        cava_out[n] = p->cava_mem[n] * p->noise_reduction + cava_out[n];
        p->cava_mem[n] = cava_out[n];
        if (p->autosens) {
            // check if we overshoot target height
            if (cava_out[n] > 1.0) {
                overshoot = 1;
                cava_out[n] = 1.0;
            }
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

    free(p->input_buffer);
    free(p->bass_multiplier);
    free(p->multiplier);
    free(p->eq);
    free(p->cut_off_frequency);
    free(p->FFTbuffer_lower_cut_off);
    free(p->FFTbuffer_upper_cut_off);
    free(p->cava_fall);
    free(p->cava_mem);
    free(p->cava_peak);
    free(p->prev_cava_out);

    fftw_free(p->in_bass_l);
    fftw_free(p->in_bass_l_raw);
    fftw_free(p->out_bass_l);
    fftw_destroy_plan(p->p_bass_l);

    fftw_free(p->in_l);
    fftw_free(p->in_l_raw);
    fftw_free(p->out_l);
    fftw_destroy_plan(p->p_l);

    if (p->audio_channels == 2) {
        fftw_free(p->in_bass_r);
        fftw_free(p->in_bass_r_raw);
        fftw_free(p->out_bass_r);
        fftw_destroy_plan(p->p_bass_r);

        fftw_free(p->in_r);
        fftw_free(p->out_r);
        fftw_free(p->in_r_raw);
        fftw_destroy_plan(p->p_r);
    }
}

#ifdef __ANDROID__
JNIEXPORT jfloatArray JNICALL Java_com_karlstav_cava_MyGLRenderer_InitCava(
    JNIEnv *env, jobject thiz, jint number_of_bars_set, jint refresh_rate, jint lower_cut_off,
    jint higher_cut_off) {
    jfloatArray cuttOffFreq = (*env)->NewFloatArray(env, number_of_bars_set + 1);
    float noise_reduction = pow((float)refresh_rate / 130, 0.75);

    plan =
        cava_init(number_of_bars_set, 44100, 1, 1, noise_reduction, lower_cut_off, higher_cut_off);
    cava_in = (double *)malloc(plan->FFTbassbufferSize * sizeof(double));
    cava_out = (double *)malloc(plan->number_of_bars * sizeof(double));
    (*env)->SetFloatArrayRegion(env, cuttOffFreq, 0, plan->number_of_bars + 1,
                                plan->cut_off_frequency);
    return cuttOffFreq;
}

JNIEXPORT jdoubleArray JNICALL Java_com_karlstav_cava_MyGLRenderer_ExecCava(JNIEnv *env,
                                                                            jobject thiz,
                                                                            jdoubleArray cava_input,
                                                                            jint new_samples) {

    jdoubleArray cavaReturn = (*env)->NewDoubleArray(env, plan->number_of_bars);

    cava_in = (*env)->GetDoubleArrayElements(env, cava_input, NULL);

    cava_execute(cava_in, new_samples, cava_out, plan);
    (*env)->SetDoubleArrayRegion(env, cavaReturn, 0, plan->number_of_bars, cava_out);
    (*env)->ReleaseDoubleArrayElements(env, cava_input, cava_in, JNI_ABORT);

    return cavaReturn;
}

JNIEXPORT int JNICALL Java_com_karlstav_cava_CavaCoreTest_InitCava(JNIEnv *env, jobject thiz,
                                                                   jint number_of_bars_set) {

    plan = cava_init(number_of_bars_set, 44100, 1, 1, 0.7, 50, 10000);
    return 1;
}

JNIEXPORT jdoubleArray JNICALL Java_com_karlstav_cava_CavaCoreTest_ExecCava(JNIEnv *env,
                                                                            jobject thiz,
                                                                            jdoubleArray cava_input,
                                                                            jint new_samples) {

    jdoubleArray cavaReturn = (*env)->NewDoubleArray(env, plan->number_of_bars);

    cava_in = (*env)->GetDoubleArrayElements(env, cava_input, NULL);

    cava_execute(cava_in, new_samples, cava_out, plan);
    (*env)->SetDoubleArrayRegion(env, cavaReturn, 0, plan->number_of_bars, cava_out);
    (*env)->ReleaseDoubleArrayElements(env, cava_input, cava_in, JNI_ABORT);

    return cavaReturn;
}
JNIEXPORT void JNICALL Java_com_karlstav_cava_MyGLRenderer_DestroyCava(JNIEnv *env, jobject thiz) {
    cava_destroy(plan);
}
#endif
