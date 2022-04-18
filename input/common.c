#include "input/common.h"
#include <math.h>

#include <string.h>

int write_to_cava_input_buffers(int16_t size, int16_t buf[size], void *data) {
    if (size == 0)
        return 0;
    struct audio_data *audio = (struct audio_data *)data;

    pthread_mutex_lock(&audio->lock);
    if (audio->samples_counter + size > audio->cava_buffer_size) {
        // buffer overflow, discard what ever is in the buffer and start over
        for (uint16_t n = 0; n < audio->cava_buffer_size; n++) {
            audio->cava_in[n] = 0;
        }
        audio->samples_counter = 0;
    }
    for (uint16_t i = 0; i < size; i++) {
        audio->cava_in[i + audio->samples_counter] = buf[i];
    }
    audio->samples_counter += size;
    pthread_mutex_unlock(&audio->lock);
    return 0;
}

void reset_output_buffers(struct audio_data *data) {
    struct audio_data *audio = (struct audio_data *)data;
    pthread_mutex_lock(&audio->lock);
    for (uint16_t n = 0; n < audio->cava_buffer_size; n++) {
        audio->cava_in[n] = 0;
    }
    pthread_mutex_unlock(&audio->lock);
}