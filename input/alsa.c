// input: ALSA
#include "input/alsa.h"
#include "debug.h"
#include "input/common.h"

#include <alloca.h>
#include <alsa/asoundlib.h>
#include <math.h>

// assuming stereo
#define CHANNELS_COUNT 2
#define SAMPLE_RATE 44100

static void initialize_audio_parameters(snd_pcm_t **handle, struct audio_data *audio,
                                        snd_pcm_uframes_t *frames) {
    // alsa: open device to capture audio
    int err = snd_pcm_open(handle, audio->source, SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0) {
        fprintf(stderr, "error opening stream: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    } else
        debug("open stream successful\n");

    snd_pcm_hw_params_t *params;
    snd_pcm_hw_params_alloca(&params);      // assembling params
    snd_pcm_hw_params_any(*handle, params); // setting defaults or something
    // interleaved mode right left right left
    snd_pcm_hw_params_set_access(*handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
    // trying to set 16bit
    snd_pcm_hw_params_set_format(*handle, params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_channels(*handle, params, CHANNELS_COUNT);
    unsigned int sample_rate = SAMPLE_RATE;
    // trying our rate
    snd_pcm_hw_params_set_rate_near(*handle, params, &sample_rate, NULL);
    // number of frames pr read
    snd_pcm_hw_params_set_period_size_near(*handle, params, frames, NULL);
    err = snd_pcm_hw_params(*handle, params); // attempting to set params
    if (err < 0) {
        fprintf(stderr, "unable to set hw parameters: %s\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    if ((err = snd_pcm_prepare(*handle)) < 0) {
        fprintf(stderr, "cannot prepare audio interface for use (%s)\n", snd_strerror(err));
        exit(EXIT_FAILURE);
    }

    // getting actual format
    snd_pcm_hw_params_get_format(params, (snd_pcm_format_t *)&sample_rate);
    // converting result to number of bits
    if (sample_rate <= 5)
        audio->format = 16;
    else if (sample_rate <= 9)
        audio->format = 24;
    else
        audio->format = 32;
    snd_pcm_hw_params_get_rate(params, &audio->rate, NULL);
    snd_pcm_hw_params_get_period_size(params, frames, NULL);
    // snd_pcm_hw_params_get_period_time(params, &sample_rate, &dir);
}

void *input_alsa(void *data) {
    int err;
    struct audio_data *audio = (struct audio_data *)data;
    snd_pcm_t *handle;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t frames = audio->input_buffer_size / 2;

    initialize_audio_parameters(&handle, audio, &frames);
    snd_pcm_get_params(handle, &buffer_size, &period_size);

    unsigned char *buf = malloc(buffer_size);
    frames = period_size / ((audio->format / 8) * CHANNELS_COUNT);

    while (!audio->terminate) {

        err = snd_pcm_readi(handle, buf, frames);

        if (err == -EPIPE) {
            /* EPIPE means overrun */
            debug("overrun occurred\n");
            snd_pcm_prepare(handle);
        } else if (err < 0) {
            debug("error from read: %s\n", snd_strerror(err));
        } else if (err != (int)frames) {
            debug("short read, read %d %d frames\n", err, (int)frames);
        }

        write_to_cava_input_buffers(frames * CHANNELS_COUNT, buf, data);
    }

    free(buf);
    snd_pcm_close(handle);
    return NULL;
}
