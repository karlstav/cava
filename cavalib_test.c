// cavalib standalone test app, build cava first and compile with:
// gcc -c -g cavalib_test.c
// gcc -o cavalib_test cavalib_test.o cava-cavalib.o -lm -lfftw3

#include "cavalib.h"
#include <math.h>
#include <stdio.h>

#include <stdlib.h>
#define PI 3.141592654

void main() {

    printf("welcome to cavalib standalone test app\n");
    printf("planning cava 20 bars (left+right) 44100 rate 2 cahnnels, 100 target height, 60 "
           "framerate \n");
    struct cava_plan *plan = cava_init(20, 44100, 2, 100, 60);

    printf("got lower cut off frequencies:\n");

    for (int i = 0; i < 10; i++) {
        printf("%.0f \t", plan->cut_off_frequency[i]);
    }
    printf("MHz\n\n");

    printf("allocating buffers and generating sine wave for test\n\n");

    int *cava_out;
    int32_t *cava_in;

    cava_out = (int *)malloc(1024 * 2 * sizeof(int));

    cava_in = (int32_t *)malloc(1024 * 2 * 4 * sizeof(int32_t));

    for (int i = 0; i < 1024 * 2; i++) {
        cava_out[i] = 0;
    }
    for (int n = 0; n < 4096; n++) {
        cava_in[n * 2] = sin(2 * PI * 200 / 44100 * n) * 10000;
        cava_in[n * 2 + 1] = sin(2 * PI * 2000 / 44100 * n) * 10000;
    }

    printf("running cava execute 40 times\n\n");
    for (int k = 0; k < 40; k++) {
        cava_execute(cava_in, 8192, cava_out, plan);
    }

    printf("output right,  max value should be at 200Hz:\n");

    for (int i = 0; i < 10; i++) {
        printf("%d \t", cava_out[i]);
    }
    printf("MHz\n");
    printf("\noutput left, max value should be at 2000Hz:\n");

    for (int i = 10; i < 20; i++) {
        printf("%d \t", cava_out[i]);
    }
    printf("MHz\n");
    cava_destroy(plan);
}