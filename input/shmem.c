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
    struct timespec req = {.tv_sec = 0, .tv_nsec = 0};

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
    req.tv_nsec = (1000000 / mmap_area->rate) * BUFSIZE;

    int16_t buffer[BUFSIZE];

    for (int i = 0; i < BUFSIZE; i++)
        buffer[i] = 0;

  //  time_t last_write = time(NULL);
  //  write_to_fftw_input_buffers(buffer, BUFSIZE, audio);

    while (!audio->terminate) {
        // if (mmap_area->updated > last_write) {
        int n = 0;
        for (int i = VB_OFFSET; i < BUFSIZE + VB_OFFSET; i++) {
            buffer[n] = mmap_area->buffer[i];
            n++;
        }
        write_to_fftw_input_buffers(buffer, BUFSIZE, audio);
        //    last_write = time(NULL);
        //}
        nanosleep(&req, NULL);
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
