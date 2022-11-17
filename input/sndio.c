#include "input/sndio.h"
#include "input/common.h"

#include <sndio.h>

void *input_sndio(void *data) {
    struct audio_data *audio = (struct audio_data *)data;
    struct sio_par par;
    struct sio_hdl *hdl;
    unsigned char buf[audio->input_buffer_size * audio->format / 8];

    sio_initpar(&par);
    par.sig = 1;
    par.bits = audio->format;
    par.le = 1;
    par.rate = audio->rate;
    ;
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

    while (audio->terminate != 1) {
        if (sio_read(hdl, buf, sizeof(buf)) == 0) {
            fprintf(stderr, __FILE__ ": sio_read() failed: %s\n", strerror(errno));
            exit(EXIT_FAILURE);
        }

        write_to_cava_input_buffers(audio->input_buffer_size, buf, audio);
    }

    sio_stop(hdl);
    sio_close(hdl);

    return 0;
}
