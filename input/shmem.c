#include "input/shmem.h"
#include "input/common.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef unsigned int u32_t;
typedef short s16_t;
int rc;

// See issue #375
// Hard-coded in squeezelite's output_vis.c, but
// this should be the same as mmap_area->buf_size
// if you were to dynamically allocate.
#define VIS_BUF_SIZE 16384

typedef struct {
    pthread_rwlock_t rwlock;
    u32_t buf_size;
    u32_t buf_index;
    bool running;
    u32_t rate;
    time_t updated;
    s16_t buffer[VIS_BUF_SIZE];
} vis_t;

// input: SHMEM
void *input_shmem(void *data) {
    struct audio_data *audio = (struct audio_data *)data;
    vis_t *mmap_area;
    int fd; /* file descriptor to mmaped area */
    int mmap_count = sizeof(vis_t);
    int buf_frames;
    struct timespec req = {.tv_sec = 0, .tv_nsec = 0};

    s16_t silence_buffer[VIS_BUF_SIZE];
    for (int i = 0; i < VIS_BUF_SIZE; i++)
        silence_buffer[i] = 0;

    printf("input_shmem: source: %s", audio->source);

    fd = shm_open(audio->source, O_RDWR, 0666);

    if (fd < 0) {
        printf("Could not open source '%s': %s\n", audio->source, strerror(errno));
        exit(EXIT_FAILURE);
    } else {
        mmap_area = mmap(NULL, sizeof(vis_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if ((intptr_t)mmap_area == -1) {
            printf("mmap failed - check if squeezelite is running with visualization enabled\n");
            exit(EXIT_FAILURE);
        }
    }

    while (!audio->terminate) {
        // audio rate may change between songs (e.g. 44.1kHz to 96kHz)
        audio->rate = mmap_area->rate;
        buf_frames = mmap_area->buf_size / 2;
        // reread after the whole buffer has changed
        req.tv_nsec = (1000000 / mmap_area->rate) * buf_frames;
        if (mmap_area->running) {
            // Frames are written by squeezelite to the buffer array starting from
            // buffer[buf_index + 1], looping around the whole array.
            // Thus, the starting position only affects the phase spectrum of the
            // fft, and not the power spectrum, so we can just read in the
            // whole buffer.
            pthread_mutex_lock(&lock);
            write_to_fftw_input_buffers(buf_frames, mmap_area->buffer, audio);
            pthread_mutex_unlock(&lock);
            nanosleep(&req, NULL);
        } else {
            write_to_fftw_input_buffers(buf_frames, silence_buffer, audio);
            nanosleep(&req, NULL);
        }
    }

    // cleanup
    if (fd > 0) {
        if (close(fd) != 0) {
            printf("Could not close file descriptor %d: %s", fd, strerror(errno));
        }
    } else {
        printf("Wrong file descriptor %d", fd);
    }

    if (munmap(mmap_area, mmap_count) != 0) {
        printf("Could not munmap() area %p+%d. %s", mmap_area, mmap_count, strerror(errno));
    }
    return 0;
}
