#include "common.h"

#ifndef _WIN32
#include <dirent.h>
#endif

#ifdef _WIN32
#include "winscap.h"
#else
#include "alsa.h"
#include "fifo.h"
#include "jack.h"
#include "oss.h"
#include "pipewire.h"
#include "portaudio.h"
#include "pulse.h"
#include "shmem.h"
#include "sndio.h"
#endif // _WIN32

int write_to_cava_input_buffers(int16_t samples, unsigned char *buf, void *data) {
    if (samples == 0)
        return 0;
    struct audio_data *audio = (struct audio_data *)data;
    pthread_mutex_lock(&audio->lock);
    int bytes_per_sample = audio->format / 8;
    if (audio->samples_counter + samples > audio->cava_buffer_size) {
        // buffer overflow, discard what ever is in the buffer and start over
        for (uint16_t n = 0; n < audio->cava_buffer_size; n++) {
            audio->cava_in[n] = 0;
        }
        audio->samples_counter = 0;
    }
    int n = 0;
    for (uint16_t i = 0; i < samples; i++) {
        switch (bytes_per_sample) {
        case 1:;
            int8_t *buf8 = (int8_t *)&buf[n];
            audio->cava_in[i + audio->samples_counter] = *buf8 * UCHAR_MAX;
            break;
        case 3:
        case 4:;
            if (audio->IEEE_FLOAT) {
                float *ieee_float = (float *)&buf[n];
                audio->cava_in[i + audio->samples_counter] = *ieee_float * USHRT_MAX;
            } else {
                int32_t *buf32 = (int32_t *)&buf[n];
                audio->cava_in[i + audio->samples_counter] = (double)*buf32 / USHRT_MAX;
            }
            break;
        default:;
            int16_t *buf16 = (int16_t *)&buf[n];
            audio->cava_in[i + audio->samples_counter] = *buf16;
            break;
        }
        n += bytes_per_sample;
    }
    audio->samples_counter += samples;
    pthread_mutex_unlock(&audio->lock);
    return 0;
}

void reset_output_buffers(struct audio_data *data) {
    struct audio_data *audio = (struct audio_data *)data;
    pthread_mutex_lock(&audio->lock);
    for (uint16_t n = 0; n < audio->cava_buffer_size; n++) {
        audio->cava_in[n] = 0;
    }
    audio->samples_counter = audio->cava_buffer_size;
    pthread_mutex_unlock(&audio->lock);
}

void signal_threadparams(struct audio_data *audio) {
    pthread_mutex_lock(&audio->lock);
    audio->threadparams = 0;
    pthread_mutex_unlock(&audio->lock);
}

void signal_terminate(struct audio_data *audio) {
    pthread_mutex_lock(&audio->lock);
    audio->terminate = 1;
    pthread_mutex_unlock(&audio->lock);
}

#ifdef ALSA
static bool is_loop_device_for_sure(const char *text) {
    const char *const LOOPBACK_DEVICE_PREFIX = "hw:Loopback,";
    return strncmp(text, LOOPBACK_DEVICE_PREFIX, strlen(LOOPBACK_DEVICE_PREFIX)) == 0;
}

static bool directory_exists(const char *path) {
    DIR *const dir = opendir(path);
    if (dir == NULL)
        return false;

    closedir(dir);
    return true;
}
#endif

ptr get_input(struct audio_data *audio, struct config_params *prm) {
    ptr ret;
    audio->source = malloc(1 + strlen(prm->audio_source));
    strcpy(audio->source, prm->audio_source);

    audio->cava_in = (double *)malloc(audio->cava_buffer_size * sizeof(double));
    memset(audio->cava_in, 0, sizeof(int) * audio->cava_buffer_size);

    audio->threadparams = 0; // most input threads don't adjust the parameters
    audio->terminate = 0;

    switch (prm->input) {
#ifndef _WIN32

#ifdef ALSA
    case INPUT_ALSA:
        if (is_loop_device_for_sure(audio->source)) {
            if (directory_exists("/sys/")) {
                if (!directory_exists("/sys/module/snd_aloop/")) {
                    cleanup(prm->output);
                    fprintf(stderr,
                            "Linux kernel module \"snd_aloop\" does not seem to  be loaded.\n"
                            "Maybe run \"sudo modprobe snd_aloop\".\n");
                    exit(EXIT_FAILURE);
                }
            }
        }
        ret = &input_alsa;
        break;
#endif
    case INPUT_FIFO:
        audio->rate = prm->samplerate;
        audio->format = prm->samplebits;
        ret = &input_fifo;
        break;

#ifdef PULSE
    case INPUT_PULSE:
        audio->format = 16;
        audio->rate = 44100;
        if (strcmp(audio->source, "auto") == 0) {
            getPulseDefaultSink((void *)audio);
        }
        ret = &input_pulse;
        break;
#endif

#ifdef SNDIO
    case INPUT_SNDIO:
        audio->format = 16;
        audio->rate = 44100;
        ret = &input_sndio;
        break;
#endif

    case INPUT_SHMEM:
        audio->format = 16;
        ret = &input_shmem;
        break;

#ifdef PORTAUDIO
    case INPUT_PORTAUDIO:
        audio->format = 16;
        audio->rate = 44100;
        audio->threadparams = 1;
        ret = &input_portaudio;
        break;
#endif

#ifdef PIPEWIRE
    case INPUT_PIPEWIRE:
        audio->format = prm->samplebits;
        audio->rate = prm->samplerate;
        audio->channels = prm->channels;
        audio->active = prm->active;
        audio->remix = prm->remix;
        audio->virtual_node = prm->virtual_node;
        ret = &input_pipewire;
        break;
#endif

#ifdef OSS
    case INPUT_OSS:
        audio->format = prm->samplebits;
        audio->rate = prm->samplerate;
        audio->channels = prm->channels;
        audio->threadparams = 1; // OSS can adjust parameters
        ret = &input_oss;
        break;
#endif

#ifdef JACK
    case INPUT_JACK:
        audio->channels = prm->channels;
        audio->autoconnect = prm->autoconnect;
        audio->threadparams = 1; // JACK server provides parameters
        ret = &input_jack;
        break;
#endif

#endif

#ifdef _WIN32
    case INPUT_WINSCAP:
        ret = &input_winscap;
        break;
#endif

    default:
        exit(EXIT_FAILURE); // Can't happen.``
    }

    return ret;
}
