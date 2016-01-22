// input: ALSA
void* input_alsa(void* data)
{
	struct audio_data *audio = (struct audio_data *)data;
	signed char *buffer;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	unsigned int val;
	snd_pcm_uframes_t frames;
	val = 44100;
	int i, n, o, size, dir, err, lo;
	int tempr, templ;
	int radj, ladj;

	// alsa: open device to capture audio
	if ((err = snd_pcm_open(&handle, audio-> source, SND_PCM_STREAM_CAPTURE, 0) < 0)) {
		fprintf(stderr,
		        "error opening stream: %s\n",
		        snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	#ifdef DEBUG
		else printf("open stream successful\n");
	#endif

	snd_pcm_hw_params_alloca(&params); //assembling params
	snd_pcm_hw_params_any (handle, params); //setting defaults or something
	snd_pcm_hw_params_set_access(handle, params,
	                             SND_PCM_ACCESS_RW_INTERLEAVED); //interleaved mode right left right left
	snd_pcm_hw_params_set_format(handle, params,
	                             SND_PCM_FORMAT_S16_LE); //trying to set 16bit
	snd_pcm_hw_params_set_channels(handle, params, 2); //assuming stereo
	val = 44100;
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir); //trying 44100 rate
	frames = 256;
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames,
	                                       &dir); //number of frames pr read

	err = snd_pcm_hw_params(handle, params); //attempting to set params
	if (err < 0) {
		fprintf(stderr,
		        "unable to set hw parameters: %s\n",
		        snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	snd_pcm_hw_params_get_format(params,
	                             (snd_pcm_format_t * )&val); //getting actual format
	//converting result to number of bits
	if (val < 6)audio->format = 16;
	else if (val > 5 && val < 10)audio->format = 24;
	else if (val > 9)audio->format = 32;

	snd_pcm_hw_params_get_rate(params, &audio->rate, &dir); //getting rate

	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	snd_pcm_hw_params_get_period_time(params,  &val, &dir);

	size = frames * (audio->format / 8) * 2; // frames * bits/8 * 2 channels
	buffer = malloc(size);
	radj = audio->format / 4; //adjustments for interleaved
	ladj = audio->format / 8;
	o = 0;
	while (1) {


		err = snd_pcm_readi(handle, buffer, frames);
		if (err == -EPIPE) {
			/* EPIPE means overrun */
			#ifdef DEBUG
				fprintf(stderr, "overrun occurred\n");
			#endif
			snd_pcm_prepare(handle);
		} else if (err < 0) {
			#ifdef DEBUG
				fprintf(stderr, "error from read: %s\n", snd_strerror(err));
			#endif
		} else if (err != (int)frames) {
			#ifdef DEBUG
				fprintf(stderr, "short read, read %d %d frames\n", err, (int)frames);
			#endif
		}

		//sorting out one channel and only biggest octet
		n = 0; //frame counter


		for (i = 0; i < size ; i = i + (ladj) * 2) {

			//first channel
			tempr = ((buffer[i + (radj) - 1 ] <<
			          2)); //using the 10 upper bits this would give me a vert res of 1024, enough...

			lo = ((buffer[i + (radj) - 2] >> 6));
			if (lo < 0)lo = abs(lo) + 1;
			if (tempr >= 0)tempr = tempr + lo;
			if (tempr < 0)tempr = tempr - lo;

			//other channel
			templ = (buffer[i + (ladj) - 1] << 2);
			lo = (buffer[i + (ladj) - 2] >> 6);
			if (lo < 0)lo = abs(lo) + 1;
			if (templ >= 0)templ = templ + lo;
			else templ = templ - lo;

			//mono: adding channels and storing it in the buffer
			if (audio->channels == 1) audio->audio_out_l[o] = (tempr + templ) / 2;


			//stereo storing channels in buffer
			if (audio->channels == 2) {
				audio->audio_out_l[o] = templ;
				audio->audio_out_r[o] = tempr;
			}


			o++;
			if (o == 2048 - 1)o = 0;

			n++;
		}

		if (audio->terminate == 1) {
			free(buffer);
			snd_pcm_close(handle);
			break;
		}
	}

	return 0;
}

