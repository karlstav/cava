#include "input/common.h"
#include <math.h>

#include <string.h>

void reset_output_buffers(struct audio_data *data) {
    memset(data->in_bass_r, 0, sizeof(double) * data->FFTbassbufferSize);
    memset(data->in_bass_l, 0, sizeof(double) * data->FFTbassbufferSize);
    memset(data->in_mid_r, 0, sizeof(double) * data->FFTmidbufferSize);
    memset(data->in_mid_l, 0, sizeof(double) * data->FFTmidbufferSize);
    memset(data->in_treble_r, 0, sizeof(double) * data->FFTtreblebufferSize);
    memset(data->in_treble_l, 0, sizeof(double) * data->FFTtreblebufferSize);
    memset(data->in_bass_r_raw, 0, sizeof(double) * data->FFTbassbufferSize);
    memset(data->in_bass_l_raw, 0, sizeof(double) * data->FFTbassbufferSize);
    memset(data->in_mid_r_raw, 0, sizeof(double) * data->FFTmidbufferSize);
    memset(data->in_mid_l_raw, 0, sizeof(double) * data->FFTmidbufferSize);
    memset(data->in_treble_r_raw, 0, sizeof(double) * data->FFTtreblebufferSize);
    memset(data->in_treble_l_raw, 0, sizeof(double) * data->FFTtreblebufferSize);
}

int write_to_fftw_input_buffers(int16_t frames, int16_t buf[frames * 2], void *data) {
    if (frames == 0)
        return 0;
    struct audio_data *audio = (struct audio_data *)data;

    for (uint16_t n = audio->FFTbassbufferSize; n > frames; n = n - frames) {
        for (uint16_t i = 1; i <= frames; i++) {
            audio->in_bass_l_raw[n - i] = audio->in_bass_l_raw[n - i - frames];
            if (audio->channels == 2)
                audio->in_bass_r_raw[n - i] = audio->in_bass_r_raw[n - i - frames];
        }
    }
    for (uint16_t n = audio->FFTmidbufferSize; n > frames; n = n - frames) {
        for (uint16_t i = 1; i <= frames; i++) {
            audio->in_mid_l_raw[n - i] = audio->in_mid_l_raw[n - i - frames];
            if (audio->channels == 2)
                audio->in_mid_r_raw[n - i] = audio->in_mid_r_raw[n - i - frames];
        }
    }
    for (uint16_t n = audio->FFTtreblebufferSize; n > frames; n = n - frames) {
        for (uint16_t i = 1; i <= frames; i++) {
            audio->in_treble_l_raw[n - i] = audio->in_treble_l_raw[n - i - frames];
            if (audio->channels == 2)
                audio->in_treble_r_raw[n - i] = audio->in_treble_r_raw[n - i - frames];
        }
    }
    uint16_t n = frames - 1;
    for (uint16_t i = 0; i < frames; i++) {
        if (audio->channels == 1) {
            if (audio->average) {
                audio->in_bass_l_raw[n] = (buf[i * 2] + buf[i * 2 + 1]) / 2;
            }
            if (audio->left) {
                audio->in_bass_l_raw[n] = buf[i * 2];
            }
            if (audio->right) {
                audio->in_bass_l_raw[n] = buf[i * 2 + 1];
            }
        }
        // stereo storing channels in buffer
        if (audio->channels == 2) {
            audio->in_bass_l_raw[n] = buf[i * 2];
            audio->in_bass_r_raw[n] = buf[i * 2 + 1];

            audio->in_mid_r_raw[n] = audio->in_bass_r_raw[n];
            audio->in_treble_r_raw[n] = audio->in_bass_r_raw[n];
        }

        audio->in_mid_l_raw[n] = audio->in_bass_l_raw[n];
        audio->in_treble_l_raw[n] = audio->in_bass_l_raw[n];
        n--;
    }

    // Hann Window
    for (int i = 0; i < audio->FFTbassbufferSize; i++) {
        audio->in_bass_l[i] = audio->bass_multiplier[i] * audio->in_bass_l_raw[i];
        if (audio->channels == 2)
            audio->in_bass_r[i] = audio->bass_multiplier[i] * audio->in_bass_r_raw[i];
    }
    for (int i = 0; i < audio->FFTmidbufferSize; i++) {
        audio->in_mid_l[i] = audio->mid_multiplier[i] * audio->in_mid_l_raw[i];
        if (audio->channels == 2)
            audio->in_mid_r[i] = audio->mid_multiplier[i] * audio->in_mid_r_raw[i];
    }
    for (int i = 0; i < audio->FFTtreblebufferSize; i++) {
        audio->in_treble_l[i] = audio->treble_multiplier[i] * audio->in_treble_l_raw[i];
        if (audio->channels == 2)
            audio->in_treble_r[i] = audio->treble_multiplier[i] * audio->in_treble_r_raw[i];
    }
    return 0;
}
