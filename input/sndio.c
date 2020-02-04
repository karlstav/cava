#include "input/sndio.h"
#include "input/common.h"

#include <sndio.h>

void *input_sndio(void *data) {
    struct audio_data *audio = (struct audio_data *)data;
    struct sio_par par;
    struct sio_hdl *hdl;
    int16_t buf[256];
    unsigned int channels;

    sio_initpar(&par);
    par.sig = 1;
    par.bits = 16;
    par.le = 1;
    par.rate = 44100;
    par.rchan = 2;
    par.appbufsz = sizeof(buf) / par.rchan;

    if ((hdl = sio_open(audio->source, SIO_REC, 0)) == NULL) {
        fprintf(stderr, __FILE__ ": Could not open sndio source: %s\n", audio->source);
        exit(EXIT_FAILURE);
    }

    if (!sio_setpar(hdl, &par) || !sio_getpar(hdl, &par) || par.sig != 1 || par.le != 1 ||
        par.rate != 44100 || par.rchan != audio->channels) {
        fprintf(stderr, __FILE__ ": Could not set required audio parameters\n");
        exit(EXIT_FAILURE);
    }

    if (!sio_start(hdl)) {
        fprintf(stderr, __FILE__ ": sio_start() failed\n");
        exit(EXIT_FAILURE);
    }

    uint16_t frames = (sizeof(buf) / sizeof(buf[0])) / channels;
    while (audio->terminate != 1) {
        if (sio_read(hdl, buf, sizeof(buf)) == 0) {
            fprintf(stderr, __FILE__ ": sio_read() failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        write_to_fftw_input_buffers(buf, frames, audio);
        /*
                        for (i = 0; i < sizeof(buf)/sizeof(buf[0]); i += 2) {
                                if (par.rchan == 1) {
                                        // sndiod has already taken care of averaging the samples
                                        audio->audio_out_l[n] = buf[i];
                                } else if (par.rchan == 2) {
                                        audio->audio_out_l[n] = buf[i];
                                        audio->audio_out_r[n] = buf[i + 1];
                                }
                                n = (n + 1) % audio->FFTbufferSize;
                        }
        */
    }

    sio_stop(hdl);
    sio_close(hdl);

    return 0;
}
