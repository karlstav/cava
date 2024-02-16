#include <stdbool.h>
#include <stddef.h>

#include <sndio.h>

#include "cava/input/common.h"
#include "cava/input/sndio.h"

void *input_sndio(void *data) {
    static const unsigned int mode = SIO_REC;

    struct audio_data *audio = data;
    struct sio_par par;
    int bytes;
    size_t buf_size;

    struct sio_hdl *hdl = NULL;
    void *buf = NULL;

    bool is_sio_started = false;
    bool success = false;

    if ((hdl = sio_open(audio->source, mode, 0)) == NULL) {
        fprintf(stderr, __FILE__ ": Could not open sndio source '%s'.\n", audio->source);
        goto cleanup;
    }

    // The recommended approach to negotiate device parameters is to try to set them with preferred
    // values and check what sndio returns for actual supported values. If CAVA doesn't support the
    // final values for rate and channels then it will complain later. We test the resulting format
    // explicitly here.
    sio_initpar(&par);
    par.bits = audio->format;
    par.sig = 1;
    par.le = 1;
    par.rchan = audio->channels;
    par.rate = audio->rate;
    par.appbufsz = audio->input_buffer_size * SIO_BPS(audio->format) / audio->channels;

    if (sio_setpar(hdl, &par) == 0) {
        fprintf(stderr, __FILE__ ": sio_setpar() failed.\n");
        goto cleanup;
    }

    if (sio_getpar(hdl, &par) == 0) {
        fprintf(stderr, __FILE__ ": sio_getpar() failed.\n");
        goto cleanup;
    }

    switch (par.bits) {
    case 8:
    case 16:
    case 24:
    case 32:
        audio->format = par.bits;
        break;
    default:
        fprintf(stderr, __FILE__ ": No support for 8, 16, 24 or 32 bits in sndio source '%s'.\n",
                audio->source);
        goto cleanup;
    }

    audio->channels = par.rchan;
    audio->rate = par.rate;

    // Parameters finalized. Signal main thread.
    signal_threadparams(audio);

    // Get the correct number of bytes per sample. Sndio uses 32 bits for 24bit, thankfully SIO_BPS
    // handles this.
    bytes = SIO_BPS(audio->format);
    buf_size = audio->input_buffer_size * bytes;

    if ((buf = malloc(buf_size)) == NULL) {
        fprintf(stderr, __FILE__ ": malloc() failed: %s\n", strerror(errno));
        goto cleanup;
    }

    if (sio_start(hdl) == 0) {
        fprintf(stderr, __FILE__ ": sio_start() failed.\n");
        goto cleanup;
    }

    is_sio_started = true;

    while (audio->terminate != 1) {
        size_t rd;

        if ((rd = sio_read(hdl, buf, buf_size)) == 0) {
            fprintf(stderr, __FILE__ ": sio_read() failed.\n");
            goto cleanup;
        }

        write_to_cava_input_buffers(rd / bytes, buf, audio);
    }

    success = true;

cleanup:
    if (is_sio_started && (sio_stop(hdl) == 0)) {
        fprintf(stderr, __FILE__ ": sio_stop() failed.\n");
        success = false;
    }

    free(buf);

    if (hdl != NULL)
        sio_close(hdl);

    signal_threadparams(audio);
    signal_terminate(audio);

    if (!success)
        exit(EXIT_FAILURE);

    return NULL;
}
