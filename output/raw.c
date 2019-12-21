#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>

int16_t buf_16;
int8_t buf_8;

int print_raw_out(int bars_count, int fd, int is_binary, int bit_format, int ascii_range,
                  char bar_delim, char frame_delim, int const f[200]) {
    if (is_binary) {
        for (int i = 0; i < bars_count; i++) {
            int f_limited = f[i];
            if (f_limited > (pow(2, bit_format) - 1))
                f_limited = pow(2, bit_format) - 1;

            switch (bit_format) {
            case 16:
                buf_16 = f_limited;
                write(fd, &buf_16, sizeof(int16_t));
                break;
            case 8:
                buf_8 = f_limited;
                write(fd, &buf_8, sizeof(int8_t));
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

            char bar_height[bar_height_size];
            snprintf(bar_height, bar_height_size, "%d", f_ranged);

            write(fd, bar_height, bar_height_size - 1);
            write(fd, &bar_delim, sizeof(bar_delim));
        }
        write(fd, &frame_delim, sizeof(frame_delim));
    }
    return 0;
}
