#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


int print_raw_out(int bars, int fp, int is_bin, int bit_format, int ascii_range, char bar_delim, char frame_delim, int f[200]) {
	int i;

	if (is_bin == 1){//binary
		if (bit_format == 16 ){//16bit:

			for (i = 0; i <  bars; i++) {
				uint16_t f16;
				if (f[i] > 10000) {
					f16 = 65535;
				}
				else {
					f16 = ((float)f[i] / 10000) * 65535;
				}
				write(fp, &f16,sizeof(uint16_t));
			}

		} else {//8bit:

			for (i = 0; i <  bars; i++) {
				uint8_t f8;
				if (f[i] > 10000) {
					f8 = 255;
				}
				else {
					f8 = ((float)f[i] / 10000) * 255;
				}
				write(fp, &f8,sizeof(uint8_t));
			}

		}
	} else {//ascii:

		for (i = 0; i <  bars; i++) {
			int f_ranged = ((float)f[i] / 10000) * ascii_range;
			if (f_ranged > ascii_range) f_ranged = ascii_range;

			//finding size of number-string in byte
			int size;
			if (f_ranged != 0) size = floor (log10 (abs (f_ranged))) + 1;
			else size = 1;
			char barheight[size];

			sprintf(barheight, "%d", f_ranged);
			write(fp, barheight,sizeof(barheight));

			write(fp, &bar_delim, sizeof(bar_delim));
		}

	write(fp, &frame_delim, sizeof(frame_delim));

	}

	return 0;
}
