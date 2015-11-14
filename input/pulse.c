#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#define BUFSIZE 1024

void* input_pulse(void* data)
{

        struct audio_data *audio = (struct audio_data *)data;
        
        int i, n;

	int16_t buf[BUFSIZE / 2];

	/* The sample type to use */
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate =  44100,
		.channels = 2
		};
	static const pa_buffer_attr pb = {
	.maxlength = BUFSIZE * 2,
	.fragsize = BUFSIZE
	};

	pa_simple *s = NULL;	
	int error;

	/* Create the recording stream   */
	if (!(s = pa_simple_new(NULL, "cava", PA_STREAM_RECORD, audio-> source, "audio for cava", &ss, NULL, &pb, &error))) {
		fprintf(stderr, __FILE__": Could not open pulseaudio source: %s, %s. To find a list of your pulseaudio sources run 'pacmd list-sources'\n",audio->source, pa_strerror(error));
		exit(EXIT_FAILURE);
	}

	n = 0;
               
	while (1) {
        	/* Record some data ... */
        	if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
            		fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
        	exit(EXIT_FAILURE);
		}

		 //sorting out channels

	        for (i = 0; i < BUFSIZE / 2; i += 2) {

                                if (audio->channels == 1) audio->audio_out_l[n] = (buf[i] + buf[i + 1]) / 2;

                                //stereo storing channels in buffer
                                if (audio->channels == 2) {
                                        audio->audio_out_l[n] = buf[i];
                                        audio->audio_out_r[n] = buf[i + 1];
                                        }

                                n++;
                                if (n == 2048 - 1)n = 0;
                        }
        }

}
