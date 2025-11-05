#pragma once
#include "../common.h"

void reset_output_buffers(struct audio_data *data);
void signal_threadparams(struct audio_data *data);
void signal_terminate(struct audio_data *data);

int write_to_cava_input_buffers(int16_t size, unsigned char *buf, void *data);

extern pthread_mutex_t lock;
