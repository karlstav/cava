

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
				for (i = 0; i < 2048; i++)audio->audio_out[i] = 0;
			t = 0;
			}
		} else { //if bytes read go ahead
			t = 0;
			for (q = 0; q < (size / 4); q++) {

				tempr = ( buf[ 4 * q - 1] << 2);

				lo =  ( buf[4 * q ] >> 6);
				if (lo < 0)lo = abs(lo) + 1;
				if (tempr >= 0)tempr = tempr + lo;
				else tempr = tempr - lo;

				templ = ( buf[ 4 * q - 3] << 2);

				lo =  ( buf[ 4 * q - 2] >> 6);
				if (lo < 0)lo = abs(lo) + 1;
				if (templ >= 0)templ = templ + lo;
				else templ = templ - lo;

				audio->audio_out[n] = (tempr + templ) / 2;

				n++;
				if (n == 2048 - 1)n = 0;
			}
		}
	}
	close(fd);
}
