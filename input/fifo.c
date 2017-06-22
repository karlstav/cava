#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "fifo.h"

void* input_fifo(void* data) {
	struct audio_data *audio = (struct audio_data *)data;

	int file_descriptor = open(audio->source, O_RDONLY);
	int flags = fcntl(file_descriptor, F_GETFL, 0);
	fcntl(file_descriptor, F_SETFL, flags | O_NONBLOCK);

	int audio_out_index = 0;
	int attempts_count = 0;
	signed char buf[BUFFER_SIZE];
	while (1) {
		int bytes = read(file_descriptor, buf, sizeof(buf));
		if (bytes == -1) { // if no bytes read sleep 10ms and zero shared buffer
			struct timespec req = { .tv_sec = 0, .tv_nsec = 10000000 };
			nanosleep (&req, NULL);
			attempts_count++;
			if (attempts_count > 10) {
				memset(audio->audio_out_l, 0,
					AUDIO_OUT_SIZE * sizeof(audio->audio_out_l));
				memset(audio->audio_out_r, 0,
					AUDIO_OUT_SIZE * sizeof(audio->audio_out_l));
				attempts_count = 0;
			}
		} else { // if bytes read go ahead
			attempts_count = 0;
			for (int q = 0; q < (BUFFER_SIZE / 4); q++) {
				int tempr = (buf[4 * q + 3] << 2);
				int lo = (buf[4 * q + 2] >> 6);
				if (lo < 0)
					lo = abs(lo) + 1;
				if (tempr >= 0)
					tempr += lo;
				else
					tempr -= lo;
				int templ = (buf[ 4 * q + 1] << 2);
				lo = (buf[ 4 * q] >> 6);
				if (lo < 0)
					lo = abs(lo) + 1;
				if (templ >= 0)
					templ += lo;
				else
					templ -= lo;

				if (audio->channels == 1)
					audio->audio_out_l[audio_out_index] = (tempr + templ) / 2;
				else { //stereo storing channels in buffer
					audio->audio_out_l[audio_out_index] = templ;
					audio->audio_out_r[audio_out_index] = tempr;
					}

				audio_out_index++;
				audio_out_index %= AUDIO_OUT_SIZE;
			}
		}
		if (audio->terminate) {
			close(file_descriptor);
			break;
		}
	}
	return NULL;
}
