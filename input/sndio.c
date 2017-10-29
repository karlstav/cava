#include <assert.h>
#include <errno.h>
#include <sndio.h>
#include <string.h>

void* input_sndio(void* data)
{
	struct audio_data *audio = (struct audio_data *)data;
	struct sio_par par;
	struct sio_hdl *hdl;
	int16_t buf[256];
	unsigned int i, n, channels;

	assert(audio->channels > 0);
	channels = audio->channels;

	sio_initpar(&par);
	par.sig = 1;
	par.bits = 16;
	par.le = 1;
	par.rate = 44100;
	par.rchan = channels;
	par.appbufsz = sizeof(buf) / channels;

	if ((hdl = sio_open(audio->source, SIO_REC, 0)) == NULL) {
		fprintf(stderr, __FILE__": Could not open sndio source: %s\n", audio->source);
		exit(EXIT_FAILURE);
	}

	if (!sio_setpar(hdl, &par) || !sio_getpar(hdl, &par) || par.sig != 1 || par.le != 1 || par.rate != 44100 || par.rchan != channels) {
		fprintf(stderr, __FILE__": Could not set required audio parameters\n");
		exit(EXIT_FAILURE);
	}

	if (!sio_start(hdl)) {
		fprintf(stderr, __FILE__": sio_start() failed\n");
		exit(EXIT_FAILURE);
	}

	n = 0;
	while (audio->terminate != 1) {
		if (sio_read(hdl, buf, sizeof(buf)) == 0) {
			fprintf(stderr, __FILE__": sio_read() failed: %s\n", strerror(errno));
			exit(EXIT_FAILURE);
		}

		for (i = 0; i < sizeof(buf)/sizeof(buf[0]); i += 2) {
			if (par.rchan == 1) {
				// sndiod has already taken care of averaging the samples
				audio->audio_out_l[n] = buf[i];
			} else if (par.rchan == 2) {
				audio->audio_out_l[n] = buf[i];
				audio->audio_out_r[n] = buf[i + 1];
			}
			n = (n + 1) % 2048;
		}
	}

	sio_stop(hdl);
	sio_close(hdl);

	return 0;
}
