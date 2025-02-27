#include "raw.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#ifndef _WIN32
#include <unistd.h>

int print_raw_out(int bars_count, int fd, int is_binary, int bit_format, int ascii_range,
                  char bar_delim, char frame_delim, int const f[]) {
#else
#define _CRT_SECURE_NO_WARNINGS 1

int print_raw_out(int bars_count, HANDLE hFile, int is_binary, int bit_format, int ascii_range,
                  char bar_delim, char frame_delim, int const f[]) {
#endif
    int16_t buf_16;
    int8_t buf_8;
    if (is_binary) {
        for (int i = 0; i < bars_count; i++) {
            int f_limited = f[i];
            if (f_limited > (pow(2, bit_format) - 1))
                f_limited = pow(2, bit_format) - 1;

            switch (bit_format) {
            case 16:
                buf_16 = f_limited;
#ifndef _WIN32
                write(fd, &buf_16, sizeof(int16_t));
#else
                WriteFile(hFile, &buf_16, sizeof(int16_t), NULL, NULL);
#endif
                break;
            case 8:
                buf_8 = f_limited;
#ifndef _WIN32
                write(fd, &buf_8, sizeof(int8_t));
#else
                WriteFile(hFile, &buf_8, sizeof(int8_t), NULL, NULL);
#endif
                break;
            }
        }
    } else { // ascii
        for (int i = 0; i < bars_count; i++) {
            int f_ranged = f[i];
            if (f_ranged > ascii_range)
                f_ranged = ascii_range;

            // finding size of number-string in byte
            int bar_height_size = 2; // a number + \0
            if (f_ranged != 0)
                bar_height_size += floor(log10(f_ranged));

            char *bar_height = malloc(bar_height_size);
            snprintf(bar_height, bar_height_size, "%d", f_ranged);
#ifndef _WIN32
            write(fd, bar_height, bar_height_size - 1);
            write(fd, &bar_delim, sizeof(bar_delim));
#else
            WriteFile(hFile, bar_height, bar_height_size - 1, NULL, NULL);
            WriteFile(hFile, &bar_delim, sizeof(bar_delim), NULL, NULL);
#endif
            free(bar_height);
        }
#ifndef _WIN32
        write(fd, &frame_delim, sizeof(frame_delim));
#else
        WriteFile(hFile, &frame_delim, sizeof(frame_delim), NULL, NULL);
#endif
    }
    return 0;
}
