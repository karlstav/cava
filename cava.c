#include <string.h>
#include <time.h>
#include <locale.h>
#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <signal.h>

#include <termios.h>
#include <unistd.h>
#include <getopt.h>
#include <pthread.h>
#include <alloca.h>
#include <sys/types.h>
#include <sys/ioctl.h>

#include <alsa/asoundlib.h>
#include <fftw3.h>

#ifdef __GNUC__
// curses.h or other sources may already define
#undef  GCC_UNUSED
#define GCC_UNUSED __attribute__((unused))
#else
#define GCC_UNUSED /* nothing */
#endif

static const int M = 2048;
static int shared[2048];
static int format = -1;
static unsigned int rate = 0;

static bool scientificMode = false;

static struct termios oldtio, newtio;
static int rc;

void cleanup(void);
void sig_handler(int sig_no);
void* input_alsa(void* data);
void* input_fifo(void* data);

// general: cleanup
void cleanup()
{
	printf("\033[0m\n");
	system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz  >/dev/null 2>&1");
	system("setterm -cursor on");
	system("setterm -blank 10");
	system("clear");
	rc = tcsetattr(0, TCSAFLUSH, &oldtio);
}

// general: handle signals
void sig_handler(int sig_no)
{
	cleanup();
	if (sig_no == SIGINT) {
		printf("CTRL-C pressed -- goodbye\n");
	}
	signal(sig_no, SIG_DFL);
	raise(sig_no);
}

// input: ALSA
void* input_alsa(void* data)
{
	signed char *buffer;
	snd_pcm_t *handle;
	snd_pcm_hw_params_t *params;
	unsigned int val;
	snd_pcm_uframes_t frames;
	char *device = ((char*)data);
	val = 44100;
	int i, o, size, dir, err, lo;
	int tempr, templ;
	int radj, ladj;

	// alsa: open device to capture audio
	if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_CAPTURE, 0) < 0)) {
		cleanup();
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
	snd_pcm_hw_params_set_rate_near(handle, params, &val, &dir); //trying 44100 rate
	frames = 256;
	snd_pcm_hw_params_set_period_size_near(handle, params, &frames,
	                                       &dir); //number of frames pr read

	err = snd_pcm_hw_params(handle, params); //attempting to set params
	if (err < 0) {
		cleanup();
		fprintf(stderr,
		        "unable to set hw parameters: %s\n",
		        snd_strerror(err));
		exit(EXIT_FAILURE);
	}

	snd_pcm_hw_params_get_format(params,
	                             (snd_pcm_format_t * )&val); //getting actual format
	//converting result to number of bits
	if (val < 6)format = 16;
	else if (val > 5 && val < 10)format = 24;
	else if (val > 9)format = 32;

	snd_pcm_hw_params_get_rate( params, &rate, &dir); //getting rate

	snd_pcm_hw_params_get_period_size(params, &frames, &dir);
	snd_pcm_hw_params_get_period_time(params,  &val, &dir);

	size = frames * (format / 8) * 2; // frames * bits/8 * 2 channels
	buffer = malloc(size);
	radj = format / 4; //adjustments for interleaved
	ladj = format / 8;
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

			//adding channels and storing it in the buffer
			shared[o] = (tempr + templ) / 2;
			o++;
			if (o == M - 1)o = 0;
		}
	}
}

//input: FIFO
void* input_fifo(void* data)
{
	int fd;
	int n = 0;
	signed char buf[1024];
	int tempr, templ, lo;
	int q, i;
	int t = 0;
	int size = 1024;
	char *path = ((char*)data);
	int bytes = 0;
	int flags;
	struct timespec req = { .tv_sec = 0, .tv_nsec = 10000000 };




	fd = open(path, O_RDONLY);
	flags = fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flags | O_NONBLOCK);

	while (1) {

		bytes = read(fd, buf, sizeof(buf));

		if (bytes == -1) { //if no bytes read sleep 10ms and zero shared buffer
			nanosleep (&req, NULL);
			t++;
			if (t > 10) {
				for (i = 0; i < M; i++)shared[i] = 0;
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

				shared[n] = (tempr + templ) / 2;

				n++;
				if (n == M - 1)n = 0;
			}
		}
	}
}

// general: entry point
int main(int argc, char **argv)
{
	pthread_t  p_thread;
	int        thr_id GCC_UNUSED;
	char *inputMethod = "alsa";
	char *outputMethod = "terminal";
	int im = 1;
	int om = 1;
	char *device = "hw:1,1";
	char *path = "/tmp/mpd.fifo";
	float fc[200];
	float fr[200];
	int lcf[200], hcf[200];
	float f[200];
	float fmem[200];
	int flast[200];
	float peak[201];
	int y[M / 2 + 1];
	long int lpeak, hpeak;
	int bands = 25;
	int sleep = 0;
	int i, n, o, bw, width, height, c, rest, virt, fixedbands;
	int autoband = 1;
	float temp;
	struct winsize w;
	double in[2 * (M / 2 + 1)];
	fftw_complex out[M / 2 + 1][2];
	fftw_plan p;
	char *color;
	int col = 36;
	int bgcol = 0;
	int sens = 100;
	#ifndef DEBUG
		int move = 0;
	#endif
	int fall[200];
	float fpeak[200];
	float k[200];
	float g;
	int framerate = 60;
	float smooth[64] = {5, 4.5, 4, 3, 2, 1.5, 1.25, 1.5, 1.5, 1.25, 1.25, 1.5, 
						1.25, 1.25, 1.5, 2, 2, 1.75, 1.5, 1.5, 1.5, 1.5, 1.5, 
						1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 
						1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 1.5, 
						1.75, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
	float sm = 1.25f; //min val from smooth[]
	struct timespec req = { .tv_sec = 0, .tv_nsec = 0 };
	char *usage = "\n\
Usage : " PACKAGE " [options]\n\
Visualize audio input in terminal. \n\
\n\
Options:\n\
	-b 1..(console columns/2-1) or 200	number of bars in the spectrum (default 25 + fills up the console), program will automatically adjust if there are too many frequency bands)\n\
	-i 'input method'			method used for listening to audio, supports: 'alsa' and 'fifo'\n\
	-o 'output method'			method used for outputting processed data, only supports 'terminal'\n\
	-d 'alsa device'			name of alsa capture device (default 'hw:1,1')\n\
	-p 'fifo path'				path to fifo (default '/tmp/mpd.fifo')\n\
	-c foreground color			supported colors: red, green, yellow, magenta, cyan, white, blue, black (default: cyan)\n\
	-C background color			supported colors: same as above (default: no change)\n\
	-s sensitivity				sensitivity percentage, 0% - no response, 50% - half, 100% - normal, etc...\n\
	-f framerate 				FPS limit, if you are experiencing high CPU usage, try reducing this (default: 60)\n\
	-S					\"scientific\" mode (disables most smoothing)\n\
	-h					print the usage\n\
	-v					print version\n\
\n";
	char ch;

	setlocale(LC_ALL, "");

	for (i = 0; i < 200; i++) {
		flast[i] = 0;
		fall[i] = 0;
		fpeak[i] = 0;
		fmem[i] = 0;
	}
	for (i = 0; i < M; i++)shared[i] = 0;

  // general: handle command-line arguments
	while ((c = getopt (argc, argv, "p:i:b:d:s:f:c:C:hSv")) != -1)
		switch (c) {
			case 'p': // argument: fifo path
				path = optarg;
				break;
			case 'i': // argument: input method
				im = 0;
				inputMethod = optarg;
				if (strcmp(inputMethod, "alsa") == 0) im = 1;
				if (strcmp(inputMethod, "fifo") == 0) im = 2;
				if (im == 0) {
					cleanup();
					fprintf(stderr,
						"input method %s is not supported, supported methods are: 'alsa' and 'fifo'\n",
					        inputMethod);
					exit(EXIT_FAILURE);
				}
				break;
			case 'o': // argument: output method
				om = 0;
				outputMethod = optarg;
				if (strcmp(outputMethod, "terminal") == 0) im = 1;
				if (im == 0) {
					cleanup();
					fprintf(stderr,
						"output method %s is not supported, supported methods are: 'terminal'\n",
					        outputMethod);
					exit(EXIT_FAILURE);
				}
				break;
			case 'b': // argument: bar count
				fixedbands = atoi(optarg);
				autoband = 0;
				if (fixedbands > 200)fixedbands = 200;
				break;
			case 'd': // argument: alsa device
				device = optarg;
				break;
			case 's': // argument: sensitivity
				sens = atoi(optarg);
				break;
			case 'f': // argument: framerate
				framerate = atoi(optarg);
				break;
			case 'c': // argument: foreground color
				col = 0;
				color = optarg;
				if (strcmp(color, "black") == 0) col = 30;
				if (strcmp(color, "red") == 0) col = 31;
				if (strcmp(color, "green") == 0) col = 32;
				if (strcmp(color, "yellow") == 0) col = 33;
				if (strcmp(color, "blue") == 0) col = 34;
				if (strcmp(color, "magenta") == 0) col = 35;
				if (strcmp(color, "cyan") == 0) col = 36;
				if (strcmp(color, "white") == 0) col = 37;
				if (col == 0) {
					cleanup();
					fprintf(stderr, "color %s not supported\n", color);
					exit(EXIT_FAILURE);
				}
				break;
			case 'C': // argument: background color
				bgcol = 0;
				color = optarg;
				if (strcmp(color, "black") == 0) bgcol = 40;
				if (strcmp(color, "red") == 0) bgcol = 41;
				if (strcmp(color, "green") == 0) bgcol = 42;
				if (strcmp(color, "yellow") == 0) bgcol = 43;
				if (strcmp(color, "blue") == 0) bgcol = 44;
				if (strcmp(color, "magenta") == 0) bgcol = 45;
				if (strcmp(color, "cyan") == 0) bgcol = 46;
				if (strcmp(color, "white") == 0) bgcol = 47;
				if (bgcol == 0) {
					cleanup();
					fprintf(stderr, "color %s not supported\n", color);
					exit(EXIT_FAILURE);
				}
				break;
			case 'S': // argument: enable "scientific" mode
				scientificMode = true;
				break;
			case 'h': // argument: print usage
				printf ("%s", usage);
				return 0;
			case '?': // argument: print usage
				printf ("%s", usage);
				return 1;
			case 'v': // argument: print version
				printf (PACKAGE " " VERSION "\n");
				return 0;
			default:  // argument: no arguments; exit
				abort ();
		}

	// general: handle Ctrl+C
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = &sig_handler;
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);

	rc = tcgetattr (0, &oldtio);
	memcpy(&newtio, &oldtio, sizeof (newtio));
	newtio.c_lflag &= ~(ICANON | ECHO);
	newtio.c_cc[VMIN] = 0;
	rc = tcsetattr(0, TCSAFLUSH, &newtio);

	n = 0;

	// input: wait for the input to be ready
	if (im == 1) {
		thr_id = pthread_create(&p_thread, NULL, input_alsa,
		                        (void*)device); //starting alsamusic listener
		while (format == -1 || rate == 0) {
			req.tv_sec = 0;
			req.tv_nsec = 1000000;
			nanosleep (&req, NULL);
			n++;
			if (n > 2000) {
			#ifdef DEBUG
				cleanup();
				fprintf(stderr,
					"could not get rate and/or format, problems with audio thread? quiting...\n");
			#endif
				exit(EXIT_FAILURE);
			}
		}
	#ifdef DEBUG
		printf("got format: %d and rate %d\n", format, rate);	
	#endif

	}

	if (im == 2) {
		thr_id = pthread_create(&p_thread, NULL, input_fifo,
		                        (void*)path); //starting fifomusic listener
		rate = 44100;
		format = 16;
	}

	p =  fftw_plan_dft_r2c_1d(M, in, *out, FFTW_MEASURE); //planning to rock

	// output: get terminal's geometry
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
	while  (1) { //jumbing back to this loop means that you resized the screen
		//getting orignial numbers of bands incase of resize
		if (autoband == 1)  {
			bands = 25;
		} else bands = fixedbands;


		if (bands > (int)w.ws_col / 2 - 1)bands = (int)w.ws_col / 2 -
			                1; //handle for user setting too many bars
		height = (int)w.ws_row - 1;
		width = (int)w.ws_col - bands - 1;

		#ifndef DEBUG
			int matrix[width][height];
			for (i = 0; i < width; i++) {
				for (n = 0; n < height; n++) {
					matrix[i][n] = 0;
				}
			}
		#endif

		bw = width / bands;

		// process [smoothing]: calculate gravity
		g = ((float)height / 1000) * powf((60 / (float)framerate), 2.5f);

		//if no bands are selected it tries to padd the default 20 if there is extra room
		if (autoband == 1) bands = bands + (((w.ws_col) - (bw * bands + bands - 1)) /
			                                    (bw + 1));
		width = (int)w.ws_col - bands - 1;
		//checks if there is still extra room, will use this to center
		rest = (((w.ws_col) - (bw * bands + bands - 1)));
		if (rest < 0)rest = 0;

		#ifdef DEBUG
			printf("height: %d width: %d bands:%d bandwidth: %d rest: %d\n",
			       (int)w.ws_row,
			       (int)w.ws_col, bands, bw, rest);
		#endif

		// process: calculate cutoff frequencies
		for (n = 0; n < bands + 1; n++) {
			fc[n] = 10000 * pow(10, -2.37f + ((((float)n + 1) / ((float)bands + 1)) *
			                                 2.37f)); //decided to cut it at 10k, little interesting to hear above
			fr[n] = fc[n] / (rate /
			                 2); //remember nyquist!, pr my calculations this should be rate/2 and  nyquist freq in M/2 but testing shows it is not... or maybe the nq freq is in M/4
			lcf[n] = fr[n] * (M /
			                  4); //lfc stores the lower cut frequency foo each band in the fft out buffer

			if (n != 0) {
				hcf[n - 1] = lcf[n] - 1;
				if (lcf[n] <= lcf[n - 1])lcf[n] = lcf[n - 1] +
					                                  1; //pushing the spectrum up if the expe function gets "clumped"
				hcf[n - 1] = lcf[n] - 1;
			}

			#ifdef DEBUG
						if (n != 0) {
							printf("%d: %f -> %f (%d -> %d) \n", n, fc[n - 1], fc[n], lcf[n - 1],
							       hcf[n - 1]);
						}
			#endif
		}

		// process: weigh signal to frequencies
		for (n = 0; n < bands;
			n++)k[n] = powf(fc[n],0.62f) * (float)height/(M*2000);

		// output: prepare screen
		virt = system("setfont cava.psf  >/dev/null 2>&1");
		#ifndef DEBUG
			system("setterm -cursor off");
			system("setterm -blank 0");

			// output: reset console
			printf("\033[0m\n");
			system("clear");

			printf("\033[%dm", col); //setting color

			printf("\033[1m"); //setting "bright" color mode, looks cooler... I think
			if (bgcol != 0)
				printf("\033[%dm", bgcol);
			{
				for (n = (height); n >= 0; n--) {
					for (i = 0; i < width + bands; i++) {

						printf(" "); //setting backround color

					}
					printf("\n");
				}
				printf("\033[%dA", height); //moving cursor back up
			}
		#endif


		// general: main loop
		while  (1) {

			// general: keyboard controls
			if ((ch = getchar()) != EOF) {
				switch (ch) {
				case 's':
					scientificMode = !scientificMode;
					break;
				case 'q':
					cleanup();
					return EXIT_SUCCESS;
				}
			}

			// output: check if terminal has been resized
			if (virt != 0) {
				ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
				if ( ((int)w.ws_row - 1) != height || ((int)w.ws_col - bands - 1) != width) {
					break;
				}
			}

			#ifdef DEBUG
				system("clear");
			#endif

			// process: populate input buffer and check if input is present
			lpeak = 0;
			hpeak = 0;
			for (i = 0; i < (2 * (M / 2 + 1)); i++) {
				if (i < M) {
					in[i] = shared[i];
					if (shared[i] > hpeak) hpeak = shared[i];
					if (shared[i] < lpeak) lpeak = shared[i];
				} else in[i] = 0;
			}
			peak[bands] = (hpeak + labs(lpeak));
			if (fabs(peak[bands]) == fabs(0.0f))sleep++;
			else sleep = 0;

			// process: if input was present for the last 5 seconds apply FFT to it
			if (sleep < framerate * 5) {

				// process: send input to external library
				fftw_execute(p);

				// process: separate frequency bands
				for (o = 0; o < bands; o++) {
					peak[o] = 0;

					// process: get peaks
					for (i = lcf[o]; i <= hcf[o]; i++) {
						y[i] = pow(pow(*out[i][0], 2) + pow(*out[i][1], 2), 0.5f); //getting r of compex
						peak[o] += y[i]; //adding upp band
					}
					peak[o] = peak[o] / (hcf[o]-lcf[o]+1); //getting average
					temp = peak[o] * k[o] * ((float)sens /
					                         100); //multiplying with k and adjusting to sens settings
					if (temp > height)temp = height; //just in case
					f[o] = temp;
					//**falloff function**//
					if (!scientificMode) {
						if (temp < flast[o]) {
							f[o] = fpeak[o] - (g * fall[o] * fall[o]);
							fall[o]++;
						} else if (temp >= flast[o]) {
							f[o] = temp;
							fpeak[o] = f[o];
							fall[o] = 0;
						}

						// process [smoothing]

						fmem[o] += f[o];
						fmem[o] = fmem[o] * 0.55f;
						f[o] = fmem[o];



						flast[o] = f[o]; //memory for falloff func	
					}
					

					if (f[o] < 0.125f)f[o] = 0.125f;
					#ifdef DEBUG
						printf("%d: f:%f->%f (%d->%d)peak:%f adjpeak: %f \n", o, fc[o], fc[o + 1],
						       lcf[o], hcf[o], peak[o], f[o]);
					#endif
				}

			} else { //**if in sleep mode wait and continue**//
				#ifdef DEBUG
					printf("no sound detected for 3 sec, going to sleep mode\n");
				#endif
				//wait 1 sec, then check sound again.
				req.tv_sec = 1;
				req.tv_nsec = 0;
				nanosleep (&req, NULL);
				continue;
			}

			if (!scientificMode)
			{
				// process [smoothing]: monstercat-style "average"
				int z, m_y;
				float m_o = 64 / bands;
				for (z = 0; z < bands; z++) {
					f[z] = f[z] * sm / smooth[(int)floor(z * m_o)];
					if (f[z] < 0.125f)f[z] = 0.125f;
					for (m_y = z - 1; m_y >= 0; m_y--) {
						f[m_y] = fmax(f[z] / powf(2, z - m_y), f[m_y]);
					}
					for (m_y = z + 1; m_y < bands; m_y++) {
						f[m_y] = fmaxf(f[z] / powf(2, m_y - z), f[m_y]);
					}
				}
			}

			// output: draw processed input
			#ifndef DEBUG
				switch (om) {
					case 1:
						for (n = (height - 1); n >= 0; n--) {
							o = 0;
							move = rest / 2; //center adjustment
							for (i = 0; i < width; i++) {

								// output: check if we're already at the next bar
								if (i != 0 && i % bw == 0) {
									o++;
									if (o < bands)move++;
								}

								// output: draw and move to another one, check whether we're not too far
								if (o < bands) {
									if (f[o] - n < 0.125f) { //blank
										if (matrix[i][n] != 0) { //change?
											if (move != 0)printf("\033[%dC", move);
											move = 0;
											printf(" ");
										} else move++; //no change, moving along
										matrix[i][n] = 0;
									} else if (f[o] - n > 1) { //color
										if (matrix[i][n] != 1) { //change?
											if (move != 0)printf("\033[%dC", move);
											move = 0;
											printf("\u2588");
										} else move++; //no change, moving along
										matrix[i][n] = 1;
									} else { //top color, finding fraction
										if (move != 0)printf("\033[%dC", move);
										move = 0;
										c = ((((f[o] - (float)n) - 0.125f) / 0.875f * 7) + 1);
										if (0 < c && c < 8) {
											if (virt == 0)printf("%d", c);
											else printf("%lc", L'\u2580' + c);
										} else printf(" ");
										matrix[i][n] = 2;
									}
								}

							}

							printf("\n");

						}

						printf("\033[%dA", height);
						break;
				}

				req.tv_sec = 0;
				req.tv_nsec = (1 / (float)framerate) * 1000000000; //sleeping for set us
				nanosleep (&req, NULL);
			#endif
		}
	}
}
