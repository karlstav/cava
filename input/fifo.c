#include "fifo.h"

int open_fifo(const char *path) {
    int fd = open(path, O_RDONLY);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return fd;
}

// input: FIFO
void *input_fifo(void *data) {
    struct audio_data *audio = (struct audio_data *)data;
    unsigned char buf[audio->input_buffer_size * audio->format / 8];

    int fd = open_fifo(audio->source);

    int test_mode = 0;

    if (strcmp(audio->source, "/dev/zero") == 0) {
        test_mode = 1;
    }

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

        write_to_cava_input_buffers(audio->input_buffer_size, buf, audio);
        if (test_mode) {
            nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 1000000},
                      NULL); // sleep 1 ms to prevent deadlock reading from /dev/zero
        }
    }

    close(fd);
    return 0;
}
