#include "input/shmem.h"
#include "input/common.h"

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

typedef unsigned int u32_t;
typedef short s16_t;

// #define BUFSIZE 1024
#define BUFSIZE 2048

int rc;

#define VIS_BUF_SIZE 16384
#define VB_OFFSET 8192 + 4096

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
    // printf("bufs: %u / run: %u / rate: %u\n",mmap_area->buf_size, mmap_area->running,
    // mmap_area->rate);
    audio->rate = mmap_area->rate;

    while (!audio->terminate) {
        write_to_fftw_input_buffers(mmap_area->buffer, BUFSIZE, audio);
        /*
                        for (i = VB_OFFSET; i < BUFSIZE+VB_OFFSET; i += 2) {
                                if (audio->channels == 1) {
                                        audio->audio_out_l[n] = (mmap_area->buffer[i] +
           mmap_area->buffer[i + 1]) / 2; } else if (audio->channels == 2) { audio->audio_out_l[n] =
           mmap_area->buffer[i]; audio->audio_out_r[n] = mmap_area->buffer[i + 1];
                                }
                                n++;
                                if (n == audio->FFTbufferSize - 1) n = 0;
                        }
        */
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
