#include "../config.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

int print_ntk_out(int bars_count, int fd, int bit_format, int bar_width, int bar_spacing,
                  int bar_height, int const f[]) {
    int8_t buf_8;
    int8_t val1;
    uint64_t bits;
    int8_t j;
    int8_t k;

    // Custom Noritake VFD bitmap for 3000-series
    for (int i = 0; i < bars_count; i++) {
        int f_limited = f[i];
        if (f_limited > (pow(2, bit_format) - 1))
            f_limited = pow(2, bit_format) - 1;

        val1 = f_limited >> (bar_height / 8 - 1); // Will not work above 32 for now
        bits = pow(2, val1) - 1;
        for (k = 0; k < bar_width; k++) {
            for (j = 0; j < bar_height / 8; j++) {
                buf_8 = bits >> (8 * (bar_height / 8 - 1 - j)) & 0xff;
                write(fd, &buf_8, sizeof(int8_t));
            }
        }
        buf_8 = 0;
        for (j = 0; j < bar_height / 8 * bar_spacing; j++) {
            write(fd, &buf_8, sizeof(int8_t));
        }
    }
    return 0;
}
