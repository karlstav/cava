#include "input/fifo.h"
#include "input/common.h"

#include <time.h>

int open_fifo(const char *path) {
    int fd = open(path, O_RDONLY);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return fd;
}

// input: FIFO
void *input_fifo(void *data) {
    struct audio_data *audio = (struct audio_data *)data;
    int SAMPLES_PER_BUFFER = audio->FFTtreblebufferSize * 2;
    int bytes_per_sample = audio->format / 8;
    __attribute__((aligned(sizeof(uint16_t)))) uint8_t buf[SAMPLES_PER_BUFFER * bytes_per_sample];
    uint16_t *samples =
        bytes_per_sample == 2 ? (uint16_t *)&buf : calloc(SAMPLES_PER_BUFFER, sizeof(uint16_t));

    int fd = open_fifo(audio->source);

    while (!audio->terminate) {
        int time_since_last_input = 0;
        unsigned int offset = 0;
        do {
            int num_read = read(fd, buf + offset, sizeof(buf) - offset);

            if (num_read < 1) { // if no bytes read sleep 10ms and zero shared buffer
                nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 10000000}, NULL);
                time_since_last_input++;

                if (time_since_last_input > 10) {
                    reset_output_buffers(audio);
                    close(fd);

                    fd = open_fifo(audio->source);
                    time_since_last_input = 0;
                    offset = 0;
                }
            } else {
                offset += num_read;
                time_since_last_input = 0;
            }
        } while (offset < sizeof(buf));

        switch (bytes_per_sample) {
        case 2:
            // [samples] = [buf] so there's nothing to do here.
            break;
        case 3:
            for (int i = 0; i < SAMPLES_PER_BUFFER; i++) {
                // Really, a sample is composed of buf[3i + 2] | buf[3i + 1] | buf[3i], but our FFT
                // only takes 16-bit samples. Since we need to scale them eventually, we can just
                // do so here by taking the top 2 bytes.
                samples[i] = (buf[3 * i + 2] << 8) | buf[3 * i + 1];
            }
            break;
        case 4:
            for (int i = 0; i < SAMPLES_PER_BUFFER; i++) {
                samples[i] = (buf[4 * i + 3] << 8) | buf[4 * i + 2];
            }
            break;
        }

        // We worked with unsigned ints up until now to save on sign extension, but the FFT wants
        // signed ints.
        pthread_mutex_lock(&lock);
        write_to_fftw_input_buffers(SAMPLES_PER_BUFFER / 2, (int16_t *)samples, audio);
        pthread_mutex_unlock(&lock);
    }

    close(fd);
    if (bytes_per_sample != 2) {
        free(samples);
    }

    return 0;
}
