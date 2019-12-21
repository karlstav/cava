#include "input/fifo.h"
#include "input/common.h"

#include <fcntl.h>
#include <time.h>
#include <unistd.h>

#define BUFSIZE 1024
#define MAX_FFTBUFERSIZE
int rc;

int open_fifo(const char *path) {
    int fd = open(path, O_RDONLY);
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    return fd;
}

// input: FIFO
void *input_fifo(void *data) {
    struct audio_data *audio = (struct audio_data *)data;
    int fd;
    int i;
    int t = 0;
    int bytes = 0;
    int16_t buf[BUFSIZE / 2];
    struct timespec req = {.tv_sec = 0, .tv_nsec = 10000000};
    uint16_t frames = BUFSIZE / 4;

    fd = open_fifo(audio->source);

    while (!audio->terminate) {

        bytes = read(fd, buf, sizeof(buf));

        if (bytes < 1) { // if no bytes read sleep 10ms and zero shared buffer
            nanosleep(&req, NULL);
            t++;
            if (t > 10) {
                for (i = 0; i < audio->FFTbassbufferSize; i++)
                    audio->audio_out_bass_l[i] = 0;
                for (i = 0; i < audio->FFTbassbufferSize; i++)
                    audio->audio_out_bass_r[i] = 0;
                for (i = 0; i < audio->FFTmidbufferSize; i++)
                    audio->audio_out_mid_l[i] = 0;
                for (i = 0; i < audio->FFTmidbufferSize; i++)
                    audio->audio_out_mid_r[i] = 0;
                for (i = 0; i < audio->FFTtreblebufferSize; i++)
                    audio->audio_out_treble_l[i] = 0;
                for (i = 0; i < audio->FFTtreblebufferSize; i++)
                    audio->audio_out_treble_r[i] = 0;
                close(fd);
                fd = open_fifo(audio->source);
                t = 0;
            }
        } else { // if bytes read go ahead
            t = 0;

            write_to_fftw_input_buffers(buf, frames, audio);
        }
    }

    close(fd);
    return 0;
}
