// cavalib standalone test app, compile with:
// gcc -c -g cavalib_test.c
// gcc -o cavalib_test cavalib_test.o cava-cavalib.o -lm -lfftw3

#include "cavalib.h"
#include <math.h>
#include <stdio.h>

#include <stdlib.h>
#define PI 3.141592654

void main() {
    float *cut_off_frequency;
    cut_off_frequency = (float *)malloc(MAX_BARS * sizeof(float));

    printf("welcome to cavalib standalone test app\n");
    printf("planning cava 20 bars (left+right) 44100 rate 2 cahnnels\n");
    cut_off_frequency = cava_plan(20, 44100, 2);

    printf("got lower cut off frequencies:\n");

    for (int i = 0; i < 10; i++) {
        printf("%.0f \t", cut_off_frequency[i]);
    }
    printf("\n\n");

    printf("allocating buffers and generating sine wave for test\n\n");

    double *cava_out;
    int32_t *cava_in;

    cava_out = (double *)malloc(1024 * 2 * sizeof(double));

    cava_in = (int32_t *)malloc(1024 * 2 * sizeof(int32_t));

    for (int i = 0; i < 1024 * 2; i++) {
        cava_out[i] = 0;
    }

    for (int i = 0; i < 1024; i++) {
        cava_in[i * 2] = sin(2 * PI * 200 / 44100 * i) * 100000;
        cava_in[i * 2 + 1] = sin(2 * PI * 2000 / 44100 * i) * 100000;
    }

    printf("running cava execute 4 times to make sure all buffers are used\n\n");

    for (int i = 0; i < 4; i++) {
        cava_execute(cava_in, cava_out);
    }

    printf("output right,  max value should be at 200Hz:\n");

    for (int i = 0; i < 10; i++) {
        printf("%.0f \t", cava_out[i]);
    }
    printf("\noutput left, max value should be at 2000Hz:\n");

    for (int i = 10; i < 20; i++) {
        printf("%.0f \t", cava_out[i]);
    }
    printf("\n");
    cava_destroy();
}