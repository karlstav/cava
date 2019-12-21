#include "input/common.h"

int write_to_fftw_input_buffers(int16_t buf[], int16_t frames, void *data) {
    struct audio_data *audio = (struct audio_data *)data;

    for (uint16_t i = 0; i < frames * 2; i += 2) {
        if (audio->channels == 1) {
            if (audio->average) {
                audio->audio_out_bass_l[audio->bass_index] = (buf[i] + buf[i + 1]) / 2;
            }
            if (audio->left) {
                audio->audio_out_bass_l[audio->bass_index] = buf[i];
            }
            if (audio->right) {
                audio->audio_out_bass_l[audio->bass_index] = buf[i + 1];
            }
        }
        // stereo storing channels in buffer
        if (audio->channels == 2) {
            audio->audio_out_bass_l[audio->bass_index] = buf[i];
            audio->audio_out_bass_r[audio->bass_index] = buf[i + 1];

            audio->audio_out_mid_r[audio->mid_index] = audio->audio_out_bass_r[audio->bass_index];
            audio->audio_out_treble_r[audio->treble_index] =
                audio->audio_out_bass_r[audio->bass_index];
        }

        audio->audio_out_mid_l[audio->mid_index] = audio->audio_out_bass_l[audio->bass_index];
        audio->audio_out_treble_l[audio->treble_index] = audio->audio_out_bass_l[audio->bass_index];

        audio->bass_index++;
        audio->mid_index++;
        audio->treble_index++;
        if (audio->bass_index == audio->FFTbassbufferSize - 1)
            audio->bass_index = 0;
        if (audio->mid_index == audio->FFTmidbufferSize - 1)
            audio->mid_index = 0;
        if (audio->treble_index == audio->FFTtreblebufferSize - 1)
            audio->treble_index = 0;
    }

    return 0;
}
