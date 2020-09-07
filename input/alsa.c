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

static int get_certain_frame(signed char *buffer, int buffer_index, int adjustment) {
    // using the 10 upper bits this would give me a vert res of 1024, enough...
    int temp = buffer[buffer_index + adjustment - 1] << 2;
    int lo = buffer[buffer_index + adjustment - 2] >> 6;
    if (lo < 0)
        lo = abs(lo) + 1;
    if (temp >= 0)
        temp += lo;
    else
        temp -= lo;
    return temp;
}
/*
static void fill_audio_outs(struct audio_data* audio, signed char* buffer,
const int size) {
        int radj = audio->format / 4; // adjustments for interleaved
        int ladj = audio->format / 8;
        static int audio_out_buffer_index = 0;
        // sorting out one channel and only biggest octet
        for (int buffer_index = 0; buffer_index < size; buffer_index += ladj * 2) {
                // first channel
                int tempr = get_certain_frame(buffer, buffer_index, radj);
                // second channel
                int templ = get_certain_frame(buffer, buffer_index, ladj);

                // mono: adding channels and storing it in the buffer
                if (audio->channels == 1)
                        audio->audio_out_l[audio_out_buffer_index] = (templ + tempr) / 2;
                else { // stereo storing channels in buffer
                        audio->audio_out_l[audio_out_buffer_index] = templ;
                        audio->audio_out_r[audio_out_buffer_index] = tempr;
                }

                ++audio_out_buffer_index;
                audio_out_buffer_index %= audio->FFTbufferSize;
        }
}
*/

void *input_alsa(void *data) {
    int err;
    struct audio_data *audio = (struct audio_data *)data;
    snd_pcm_t *handle;
    snd_pcm_uframes_t buffer_size;
    snd_pcm_uframes_t period_size;
    snd_pcm_uframes_t frames = audio->FFTtreblebufferSize;

    initialize_audio_parameters(&handle, audio, &frames);
    snd_pcm_get_params(handle, &buffer_size, &period_size);

    int radj = audio->format / 4; // adjustments for interleaved
    int ladj = audio->format / 8;
    int16_t buf[period_size];
    int32_t buffer32[period_size];
    frames = period_size / ((audio->format / 8) * CHANNELS_COUNT);
    // printf("period size: %lu\n", period_size);
    // exit(0);

    // frames * bits/8 * channels
    // const int size = frames * (audio->format / 8) * CHANNELS_COUNT;
    signed char *buffer = malloc(period_size);

    while (!audio->terminate) {
        switch (audio->format) {
        case 16:
            err = snd_pcm_readi(handle, buf, frames);
            break;
        case 32:
            err = snd_pcm_readi(handle, buffer32, frames);
            for (uint16_t i = 0; i < frames * 2; i++) {
                buf[i] = buffer32[i] / pow(2, 16);
            }
            break;
        default:
            err = snd_pcm_readi(handle, buffer, frames);
            // sorting out one channel and only biggest octet
            for (uint16_t i = 0; i < period_size * 2; i += ladj * 2) {
                // first channel
                buf[i] = get_certain_frame(buffer, i, ladj);
                // second channel
                buf[i + 1] = get_certain_frame(buffer, i, radj);
            }
            // fill_audio_outs(audio, buffer, period_size);
            break;
        }

        if (err == -EPIPE) {
            /* EPIPE means overrun */
            debug("overrun occurred\n");
            snd_pcm_prepare(handle);
        } else if (err < 0) {
            debug("error from read: %s\n", snd_strerror(err));
        } else if (err != (int)frames) {
            debug("short read, read %d %d frames\n", err, (int)frames);
        }

        pthread_mutex_lock(&lock);
        write_to_fftw_input_buffers(frames, buf, data);
        pthread_mutex_unlock(&lock);
    }

    free(buffer);
    snd_pcm_close(handle);
    return NULL;
}
