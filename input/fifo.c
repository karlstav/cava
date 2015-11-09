struct audio_data {

        int audio_out_r[2048];
        int audio_out_l[2048];
        int format;
        unsigned int rate ;
        char *source; //alsa device or fifo path
        int im; //input mode alsa or fifo
        int channels;
};


//input: FIFO
void* input_fifo(void* data)
{
	struct audio_data *audio = (struct audio_data *)data;
	int fd;
	int n = 0;
	signed char buf[1024];
	int tempr, templ, lo;
	int q, i;
	int t = 0;
	int size = 1024;
	int bytes = 0;
	int flags;
	struct timespec req = { .tv_sec = 0, .tv_nsec = 10000000 };




	fd = open(audio->source, O_RDONLY);
	flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	while (1) {

		bytes = read(fd, buf, sizeof(buf));

		if (bytes == -1) { //if no bytes read sleep 10ms and zero shared buffer
			nanosleep (&req, NULL);
			t++;
			if (t > 10) {
				for (i = 0; i < 2048; i++)audio->audio_out_l[i] = 0;
				for (i = 0; i < 2048; i++)audio->audio_out_r[i] = 0;
				t = 0;
			}
		} else { //if bytes read go ahead
			t = 0;
			for (q = 0; q < (size / 4); q++) {

				tempr = ( buf[ 4 * q + 3] << 2);

				lo =  ( buf[4 * q + 2] >> 6);
				if (lo < 0)lo = abs(lo) + 1;
				if (tempr >= 0)tempr = tempr + lo;
				else tempr = tempr - lo;

				templ = ( buf[ 4 * q + 1] << 2);

				lo =  ( buf[ 4 * q] >> 6);
				if (lo < 0)lo = abs(lo) + 1;
				if (templ >= 0)templ = templ + lo;
				else templ = templ - lo;

				if (audio->channels == 1) audio->audio_out_l[n] = (tempr + 
templ) / 
2;


				//stereo storing channels in buffer
				if (audio->channels == 2) {
					audio->audio_out_l[n] = templ;
					audio->audio_out_r[n] = tempr;
					}

				n++;
				if (n == 2048 - 1)n = 0;
			}
		}
	}
	close(fd);
}
