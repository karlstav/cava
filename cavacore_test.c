// cavacore standalone test app, build cavacore lib first and compile with:
// gcc -c -g cavacore_test.c
// gcc -o cavacore_test cavacore_test.o build/libcavacore.a -lm -lfftw3

#include "cavacore.h"
#include <math.h>
#include <stdio.h>

#include <stdlib.h>
#define PI 3.141592654

#ifdef PIPEWIRE
#include "input/pipewire.h"
#endif

#ifdef AUDIO_INPUT
#include "input/common.h"
#endif

void main() {

    printf("welcome to cavacore standalone test app\n");

    int bars_per_channel = 10;
    int channels = 2;
    int buffer_size = 512 * channels; // number of samples per cava execute
    int rate = 44100;
    double noise_reduction = 0.77;
    int low_cut_off = 50;
    int high_cut_off = 10000;
    double blueprint_200MHz[10] = {0, 0, 0.930, 0.004, 0, 0, 0, 0, 0, 0};
    double blueprint_2000MHz[10] = {0, 0, 0, 0, 0, 0, 0.878, 0.003, 0, 0};
#ifdef AUDIO_INPUT
    struct audio_data audio;
    memset(&audio, 0, sizeof(audio));
    audio.source = malloc(1 + strlen("auto"));
    strcpy(audio.source, "auto");
    audio.format = 16;
    audio.rate = rate;
    audio.channels = channels;
    audio.input_buffer_size = buffer_size;
    audio.cava_buffer_size = 16384;
    audio.active = 1;
    audio.remix = 0;
    audio.virtual_node = 0;
    audio.cava_in = (double *)malloc(audio.cava_buffer_size * sizeof(double));
    memset(audio.cava_in, 0, sizeof(int) * audio.cava_buffer_size);

    audio.threadparams = 0; // most input threads don't adjust the parameters
    audio.terminate = 0;
    int thr_id;
    pthread_t p_thread;
    int frame_time_msec = 10;
    struct timespec framerate_timer = {.tv_sec = 0, .tv_nsec = frame_time_msec * 1e6};
#else
    printf("running in simulation mode, no audio input\n\n");
#endif

    printf("planning visualization with %d bars per channel, %d rate, %d channels, autosens, "
           "%.2f noise reduction, %d - %d MHz bandwith.\n",
           bars_per_channel, rate, channels, noise_reduction, low_cut_off, high_cut_off);

    struct cava_plan *plan =
        cava_init(bars_per_channel, rate, channels, 1, noise_reduction, low_cut_off, high_cut_off);
    if (plan->status < 0) {
        fprintf(stderr, "Error: %s\n", plan->error_message);
        exit(1);
    }
    printf("got lower cut off frequencies:\n");

    for (int i = 0; i < bars_per_channel; i++) {
        printf("%.0f \t", plan->cut_off_frequency[i]);
    }
    printf("MHz\n\n");

    printf("allocating buffers and generating sine wave for test\n\n");

    double *cava_out;
    double *cava_in;

    cava_out = (double *)malloc(bars_per_channel * channels * sizeof(double));

    cava_in = (double *)malloc(buffer_size * sizeof(double));

    for (int i = 0; i < bars_per_channel * channels; i++) {
        cava_out[i] = 0;
    }
#ifdef PIPEWIRE
    thr_id = pthread_create(&p_thread, NULL, input_pipewire, (void *)&audio);
#endif
    printf("running cava execute 300 times (about 3.5 seconds run time)\n\n");
    for (int k = 0; k < 300; k++) {
#ifdef AUDIO_INPUT
        pthread_mutex_lock(&audio.lock);
        cava_execute(audio.cava_in, audio.samples_counter, cava_out, plan);
        pthread_mutex_unlock(&audio.lock);
        nanosleep(&framerate_timer, NULL);
#else
        // filling up 512*2 samples at a time, making sure the sinus wave is unbroken
        // 200MHz in left channel, 2000MHz in right
        // if we where using a proper audio source this would be replaced by a simple read function
        for (int n = 0; n < buffer_size / 2; n++) {
            cava_in[n * 2] = sin(2 * PI * 200 / rate * (n + (k * buffer_size / 2))) * 20000;
            cava_in[n * 2 + 1] = sin(2 * PI * 2000 / rate * (n + (k * buffer_size / 2))) * 20000;
        }
        cava_execute(cava_in, buffer_size, cava_out, plan);
#endif
    }
#ifdef AUDIO_INPUT
    pthread_mutex_lock(&audio.lock);
    audio.terminate = 1;
    pthread_mutex_unlock(&audio.lock);
    pthread_join(p_thread, NULL);
#endif
    // rounding last output to nearst 1/1000th
    for (int i = 0; i < bars_per_channel * 2; i++) {
        cava_out[i] = (double)round(cava_out[i] * 1000) / 1000;
    }

    printf("\nlast output left, max value should be at 200Hz in simulation mode:\n");
    for (int i = 0; i < bars_per_channel; i++) {
        printf("%.3f \t", cava_out[i]);
    }
    printf("MHz\n");

    printf("last output right,  max value should be at 2000Hz in simulation mode:\n");
    for (int i = 0; i < bars_per_channel; i++) {
        printf("%.3f \t", cava_out[i + bars_per_channel]);
    }
    printf("MHz\n\n");

    // checking if within 2% of blueprint
    int bp_ok = 1;
    for (int i = 0; i < bars_per_channel; i++) {
        if (cava_out[i] > blueprint_200MHz[i] * 1.02 || cava_out[i] < blueprint_200MHz[i] * 0.98)
            bp_ok = 0;
    }
    for (int i = 0; i < bars_per_channel; i++) {
        if (cava_out[i + bars_per_channel] > blueprint_2000MHz[i] * 1.02 ||
            cava_out[i + bars_per_channel] < blueprint_2000MHz[i] * 0.98)
            bp_ok = 0;
    }
    cava_destroy(plan);
    free(plan);
    free(cava_in);
    free(cava_out);
#ifndef AUDIO_INPUT
    if (bp_ok == 1) {
        printf("matching blueprint\n");
        exit(0);
    } else {
        printf("not matching blueprint\n");
        exit(1);
    }
#endif
}
