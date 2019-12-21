#include "input/fifo.h"
#include "input/common.h"

#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#define BUFSIZE 1024

int open_fifo(const char *path) {
    int fd = open(path, O_RDONLY);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return fd;
}

// input: FIFO
void *input_fifo(void *data) {
    struct audio_data *audio = (struct audio_data *)data;
    int time_since_last_input = 0;
    int bytes = 0;
    int16_t buf[BUFSIZE / 2];

    int fd = open_fifo(audio->source);

    while (!audio->terminate) {
        bytes = read(fd, buf, sizeof(buf));

        if (bytes < 1) { // if no bytes read sleep 10ms and zero shared buffer
            nanosleep(&(struct timespec){.tv_sec = 0, .tv_nsec = 10000000}, NULL);
            time_since_last_input++;
            if (time_since_last_input > 10) {
                reset_output_buffers(audio);
                close(fd);

                fd = open_fifo(audio->source);
                time_since_last_input = 0;
            }
        } else { // if bytes read go ahead
            time_since_last_input = 0;

            write_to_fftw_input_buffers(buf, BUFSIZE / 4, audio);
        }
    }

    close(fd);
    return 0;
}
