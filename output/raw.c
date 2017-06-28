#include <stdio.h>
#include <stdint.h>

#define TEN_THSND 10000
#define TEN_THSND_F 10000.0
#define BIT_16 16
#define BIT_8 8

#define NORMALIZE_AND_WRITE(type) _NORMALIZE_AND_WRITE(type)
#define _NORMALIZE_AND_WRITE(type) \
	for (int i = 0; i < bars_count; i++) { \
		uint##type##_t f_##type = UINT##type##_MAX; \
		if (f[i] < TEN_THSND) \
			f_##type *= f[i] / TEN_THSND_F; \
		write(fd, &f_##type, sizeof(uint##type##_t)); \
	}

int print_raw_out(int bars_count, int fd, int is_binary, int bit_format,
int ascii_range, char bar_delim, char frame_delim, const int const f[200]) {
	if (is_binary) {
		switch (bit_format) {
		case BIT_16:
			NORMALIZE_AND_WRITE(BIT_16);
			break;
		case BIT_8:
			NORMALIZE_AND_WRITE(BIT_8);
			break;
		}
	} else { // ascii
		for (int i = 0; i < bars_count; i++) {
			int f_ranged = (f[i] / TEN_THSND_F) * ascii_range;
			if (f_ranged > ascii_range)
				f_ranged = ascii_range;

			// finding size of number-string in byte
			int bar_height_size = 2; // a number + \0
			if (f_ranged != 0)
				bar_height_size += floor (log10 (f_ranged));

			char bar_height[bar_height_size];
			snprintf(bar_height, bar_height_size, "%d", f_ranged);

			write(fd, bar_height, bar_height_size - 1);
			write(fd, &bar_delim, sizeof(bar_delim));
		}
	write(fd, &frame_delim, sizeof(frame_delim));
	}
	return 0;
}
