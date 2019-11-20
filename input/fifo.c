#include <unistd.h>
#define BUFSIZE 1024
#define MAX_FFTBUFERSIZE
int rc;

struct audio_data {

	int FFTbassbufferSize;
	int FFTmidbufferSize;
	int FFTtreblebufferSize;
	int bass_index;
	int mid_index;
	int treble_index;
	int16_t audio_out_bass_r[65536];
	int16_t audio_out_bass_l[65536];
	int16_t audio_out_mid_r[65536];
	int16_t audio_out_mid_l[65536];
	int16_t audio_out_treble_r[65536];
	int16_t audio_out_treble_l[65536];
	int format;
	unsigned int rate ;
	char *source; //alsa device, fifo path or pulse source
	int im; //input mode alsa, fifo or pulse
	int channels;
	bool left, right, average;
	int terminate; // shared variable used to terminate audio thread
	char error_message[1024];
};

int write_to_fftw_input_buffers(int16_t buf[], int16_t frames, void* data) {

	struct audio_data* audio = (struct audio_data*)data;

	for (uint16_t i = 0; i < frames * 2; i += 2) {
		if (audio->channels == 1){
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
		//stereo storing channels in buffer
		if (audio->channels == 2) {
			audio->audio_out_bass_l[audio->bass_index] = buf[i];
			audio->audio_out_bass_r[audio->bass_index] = buf[i + 1];

			audio->audio_out_mid_r[audio->mid_index] = audio->audio_out_bass_r[audio->bass_index];
			audio->audio_out_treble_r[audio->treble_index] = audio->audio_out_bass_r[audio->bass_index];
		}

		audio->audio_out_mid_l[audio->mid_index] = audio->audio_out_bass_l[audio->bass_index];
		audio->audio_out_treble_l[audio->treble_index] = audio->audio_out_bass_l[audio->bass_index];

		audio->bass_index++;
		audio->mid_index++;
		audio->treble_index++;
		if (audio->bass_index == audio->FFTbassbufferSize - 1) audio->bass_index = 0;
		if (audio->mid_index == audio->FFTmidbufferSize - 1) audio->mid_index = 0;
		if (audio->treble_index == audio->FFTtreblebufferSize - 1) audio->treble_index = 0;
	}

	return 0;


}

int open_fifo(const char *path)
{
	int fd = open(path, O_RDONLY);
	int flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	return fd;
}


//input: FIFO
void* input_fifo(void* data)
{
	struct audio_data *audio = (struct audio_data *)data;
	int fd;
	int i;
	int t = 0;
	int bytes = 0;
	int16_t buf[BUFSIZE / 2];
	struct timespec req = { .tv_sec = 0, .tv_nsec = 10000000 };
	uint16_t frames = BUFSIZE / 4;

	fd = open_fifo(audio->source);

	while (1) {

		bytes = read(fd, buf, sizeof(buf));

		if (bytes < 1) { //if no bytes read sleep 10ms and zero shared buffer
			nanosleep (&req, NULL);
			t++;
			if (t > 10) {
				for (i = 0; i < audio->FFTbassbufferSize; i++)audio->audio_out_bass_l[i] = 0;
				for (i = 0; i < audio->FFTbassbufferSize; i++)audio->audio_out_bass_r[i] = 0;
				for (i = 0; i < audio->FFTmidbufferSize; i++)audio->audio_out_mid_l[i] = 0;
				for (i = 0; i < audio->FFTmidbufferSize; i++)audio->audio_out_mid_l[i] = 0;
				for (i = 0; i < audio->FFTtreblebufferSize; i++)audio->audio_out_treble_r[i] = 0;
				for (i = 0; i < audio->FFTtreblebufferSize; i++)audio->audio_out_treble_r[i] = 0;
				close(fd);
				fd = open_fifo(audio->source);
				t = 0;
			}
		} else { //if bytes read go ahead
			t = 0;

			write_to_fftw_input_buffers(buf, frames, audio);
		}

		if (audio->terminate == 1) {
			close(fd);
			break;
		}

	}

	return 0;
}
