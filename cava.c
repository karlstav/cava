#define _XOPEN_SOURCE_EXTENDED
#include <alloca.h>
#include <locale.h>
#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <termios.h>
#include <math.h>
#include <alsa/asoundlib.h>
#include <sys/ioctl.h>
#include <fftw3.h>
#define max(a,b) \
	 ({ __typeof__ (a) _a = (a); \
			 __typeof__ (b) _b = (b); \
		 _a > _b ? _a : _b; })
#include <unistd.h>
#include <sys/types.h>
#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <getopt.h>
#include <pthread.h>
#include <dirent.h>
#include "output/terminal_noncurses.h"
#include "output/terminal_noncurses.c"
#include "output/terminal_ncurses.h"
#include "output/terminal_ncurses.c"
#include "output/terminal_bcircle.h"
#include "output/terminal_bcircle.c"
#include "input/alsa.h"
#include "input/alsa.c"
#include "input/fifo.h"
#include "input/fifo.c"
#include <iniparser.h>


#ifdef __GNUC__
// curses.h or other sources may already define
#undef  GCC_UNUSED
#define GCC_UNUSED __attribute__((unused))
#else
#define GCC_UNUSED /* nothing */
#endif

struct termios oldtio, newtio;
int rc;

char *inputMethod, *outputMethod, *modeString, *color, *bcolor;
double monstercat, integral, gravity, ignore, smh;
int fixedbands, sens, framerate;
unsigned int lowcf, highcf;
double smoothDef[64] = {0.8, 0.8, 1, 1, 0.8, 0.8, 1, 0.8, 0.8, 1, 1, 0.8,
					1, 1, 0.8, 0.6, 0.6, 0.7, 0.8, 0.8, 0.8, 0.8, 0.8,
					0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
					0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
					0.7, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6};
double *smooth = smoothDef;
int smcount = 64;
struct audio_data audio;
int im = 1;
int om = 1;
int mode = 1;
int col = 6;
int bgcol = -1;
int bands = 25;
int autoband = 1;

// general: cleanup
void cleanup(void)
{
	cleanup_terminal_ncurses();
	cleanup_terminal_noncurses();
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

void load_config()
{
	// config: location
	char *configFile = "config";
	char configPath[255];
	configPath[0] = '\0';

	if (configPath[0] == '\0') {
		char *configHome = getenv("XDG_CONFIG_HOME");
		if (configHome != NULL) {
			snprintf(configPath, sizeof(configPath), "%s/%s/", configHome, PACKAGE);
		} else {
			configHome = getenv("HOME");
			if (configHome != NULL) {
				snprintf(configPath, sizeof(configPath), "%s/%s/%s/", configHome, ".config", PACKAGE);
			} else {
				printf("No HOME found (ERR_HOMELESS), exiting...");
				exit(EXIT_FAILURE);
			}
		}
	}
	// config: create directory
	mkdir(configPath, 0777);

	// config: create empty file
	strcat(configPath, configFile);
	FILE *fp = fopen(configPath, "ab+");
	fclose(fp);

	// config: parse ini
	dictionary* ini = iniparser_load(configPath);

	inputMethod = (char *)iniparser_getstring(ini, "input:method", "alsa");
	outputMethod = (char *)iniparser_getstring(ini, "output:method", "ncurses");
	modeString = (char *)iniparser_getstring(ini, "general:mode", "normal");
	monstercat = 1.5 * iniparser_getdouble(ini, "smoothing:monstercat", 1);
	integral = iniparser_getdouble(ini, "smoothing:integral", 0.7);
	gravity = iniparser_getdouble(ini, "smoothing:gravity", 1);
	ignore = iniparser_getdouble(ini, "smoothing:ignore", 0);
	color = (char *)iniparser_getstring(ini, "color:foreground", "default");;
	bcolor = (char *)iniparser_getstring(ini, "color:background", "default");;
	fixedbands = iniparser_getint(ini, "general:bars", 0);
	sens = iniparser_getint(ini, "general:sensitivity", 100);
	framerate = iniparser_getint(ini, "general:framerate", 60);
	lowcf = iniparser_getint(ini, "general:lower_cutoff_freq", 20);
	highcf = iniparser_getint(ini, "general:higher_cutoff_freq", 10000);

	smcount = iniparser_getsecnkeys(ini, "eq");
	if (smcount > 0) {
		smooth = malloc(smcount*sizeof(*smooth));
		const char *keys[smcount];
		iniparser_getseckeys(ini, "eq", keys);
		for (int sk = 0; sk < smcount; sk++) {
			smooth[sk] = iniparser_getdouble(ini, keys[sk], 1);
		}
	} else {
		smcount = 64; //back to the default one
	}

	// config: input
	if (strcmp(inputMethod, "alsa") == 0) {
		im = 1;
		audio.source = (char *)iniparser_getstring(ini, "input:source", "hw:Loopback,1");
	}
	if (strcmp(inputMethod, "fifo") == 0) {
		im = 2;
		audio.source = (char *)iniparser_getstring(ini, "input:source", "/tmp/mpd.fifo");
	}
}

void validate_config()
{
	// validate: input method
	if (strcmp(inputMethod, "alsa") == 0) {
		im = 1;
	}
	if (strcmp(inputMethod, "fifo") == 0) {
		im = 2;
	}
	if (im == 0) {
		fprintf(stderr,
			"input method %s is not supported, supported methods are: 'alsa' and 'fifo'\n",
						inputMethod);
		exit(EXIT_FAILURE);
	}

	// validate: output method
	if (strcmp(outputMethod, "ncurses") == 0) om = 1;
	if (strcmp(outputMethod, "circle") == 0) om = 2;
	if (strcmp(outputMethod, "noncurses") == 0) {
		om = 3;
		bgcol = 0;
	}
	if (om == 0) {
		fprintf(stderr,
			"output method %s is not supported, supported methods are: 'terminal', 'circle'\n",
						outputMethod);
		exit(EXIT_FAILURE);
	}

	// validate: bands
	if (fixedbands > 0) autoband = 0;
	if (fixedbands > 200)fixedbands = 200;

	// validate: mode
	if (strcmp(modeString, "normal") == 0) mode = 1;
	if (strcmp(modeString, "scientific") == 0) mode = 2;
	if (strcmp(modeString, "waves") == 0) mode = 3;
	if (mode == 0) {
		fprintf(stderr,
			"smoothing mode %s is not supported, supported modes are: 'normal', 'scientific', 'waves'\n",
						modeString);
		exit(EXIT_FAILURE);
	}

	// validate: framerate
	if (framerate < 0) {
		fprintf(stderr,
			"framerate can't be negative!\n");
		exit(EXIT_FAILURE);
	}

	// validate: color
	if (strcmp(color, "black") == 0) col = 0;
	if (strcmp(color, "red") == 0) col = 1;
	if (strcmp(color, "green") == 0) col = 2;
	if (strcmp(color, "yellow") == 0) col = 3;
	if (strcmp(color, "blue") == 0) col = 4;
	if (strcmp(color, "magenta") == 0) col = 5;
	if (strcmp(color, "cyan") == 0) col = 6;
	if (strcmp(color, "white") == 0) col = 7;
	// default if invalid

	// validate: background color
	if (strcmp(bcolor, "black") == 0) bgcol = 0;
	if (strcmp(bcolor, "red") == 0) bgcol = 1;
	if (strcmp(bcolor, "green") == 0) bgcol = 2;
	if (strcmp(bcolor, "yellow") == 0) bgcol = 3;
	if (strcmp(bcolor, "blue") == 0) bgcol = 4;
	if (strcmp(bcolor, "magenta") == 0) bgcol = 5;
	if (strcmp(bcolor, "cyan") == 0) bgcol = 6;
	if (strcmp(bcolor, "white") == 0) bgcol = 7;
	// default if invalid

	// validate: gravity
	if (gravity < 0) {
		gravity = 0;
	}

	// validate: integral
	if (integral < 0) {
		integral = 0;
	} else if (integral > 1) {
		integral = 0.99;
	}

	// validate: cutoff
	if (lowcf == 0 ) lowcf++;
	if (lowcf > highcf) {
		fprintf(stderr,
			"lower cutoff frequency can't be higher than higher cutoff frequency\n");
		exit(EXIT_FAILURE);
	}
	// read & validate: eq
}

static bool is_loop_device_for_sure(const char * text) {
	const char * const LOOPBACK_DEVICE_PREFIX = "hw:Loopback,";
	return strncmp(text, LOOPBACK_DEVICE_PREFIX, strlen(LOOPBACK_DEVICE_PREFIX)) == 0;
}

static bool directory_exists(const char * path) {
	DIR * const dir = opendir(path);
	const bool exists = dir != NULL;
	closedir(dir);
	return exists;
}

// general: entry point
int main(int argc, char **argv)
{
	load_config();

	// general: define variables
	int M = 2048;
	pthread_t  p_thread;
	int        thr_id GCC_UNUSED;
	int modes = 3; // amount of smoothing modes
	float fc[200];
	float fr[200];
	int lcf[200], hcf[200];
	int f[200];
	int fmem[200];
	int flast[200];
	int flastd[200];
	float peak[201];
	int y[M / 2 + 1];
	long int lpeak, hpeak;
	int sleep = 0;
	int i, n, o, bw, height, h, w, c, rest, virt;
	float temp;
	double in[2 * (M / 2 + 1)];
	fftw_complex out[M / 2 + 1][2];
	fftw_plan p;
	int cont = 1;
	int fall[200];
	float fpeak[200];
	float k[200];
	float g;
	struct timespec req = { .tv_sec = 0, .tv_nsec = 0 };
	char *usage = "\n\
Usage : " PACKAGE " [options]\n\
Visualize audio input in terminal. \n\
\n\
Options:\n\
	-b 1..(console columns/2-1) or 200  number of bars in the spectrum (default 25 + fills up the console), program will automatically adjust if there are too many frequency bands)\n\
	-i 'input method'     method used for listening to audio, supports: 'alsa' and 'fifo'\n\
	-o 'output method'      method used for outputting processed data, supports: 'ncurses', 'noncurses' and 'circle'\n\
	-d 'alsa device'      name of alsa capture device (default 'hw:Loopback,1')\n\
	-p 'fifo path'        path to fifo (default '/tmp/mpd.fifo')\n\
	-c foreground color     supported colors: red, green, yellow, magenta, cyan, white, blue, black (default: cyan)\n\
	-C background color     supported colors: same as above (default: no change)\n\
	-s sensitivity        sensitivity percentage, 0% - no response, 50% - half, 100% - normal, etc...\n\
	-f framerate        FPS limit, if you are experiencing high CPU usage, try reducing this (default: 60)\n\
	-m mode         set mode (normal, scientific, waves)\n\
	-h          print the usage\n\
	-v          print version\n\
\n";
	char ch;

	audio.format = -1;
	audio.rate = 0;

	setlocale(LC_ALL, "");

	for (i = 0; i < M; i++)audio.audio_out[i] = 0;

	// general: handle Ctrl+C
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = &sig_handler;
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);


	// general: handle command-line arguments
	while ((c = getopt (argc, argv, "p:i:o:m:b:d:s:f:c:C:hv")) != -1) {
		switch (c) {
			case 'i': // argument: input method
				im = 0;
				inputMethod = optarg;
				break;
			case 'p': // argument: fifo path
				audio.source = optarg;
				break;
			case 'o': // argument: output method
				om = 0;
				outputMethod = optarg;
				break;
			case 'm': // argument: smoothing mode
				mode = 0;
				modeString = optarg;
				break;
			case 'b': // argument: bar count
				fixedbands = atoi(optarg);
				break;
			case 'd': // argument: alsa device
				audio.source  = optarg;
				break;
			case 's': // argument: sensitivity
				sens = atoi(optarg);
				break;
			case 'f': // argument: framerate
				framerate = atoi(optarg);
				break;
			case 'c': // argument: foreground color
				col = -2;
				color = optarg;
				break;
			case 'C': // argument: background color
				bgcol = -2;
				bcolor = optarg;
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

		n = 0;
	}

	// config: validate
	validate_config();

	// input: wait for the input to be ready
	if (im == 1) {
		if (is_loop_device_for_sure(audio.source)) {
			if (directory_exists("/sys/")) {
				if (! directory_exists("/sys/module/snd_aloop/")) {
					fprintf(stderr,
							"Linux kernel module \"snd_aloop\" does not seem to be loaded.\n"
							"Maybe run \"sudo modprobe snd_aloop\".\n");
					exit(EXIT_FAILURE);
				}
			}
		}

		thr_id = pthread_create(&p_thread, NULL, input_alsa,
														(void *)&audio); //starting alsamusic listener
		while (audio.format == -1 || audio.rate == 0) {
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
		printf("got format: %d and rate %d\n", audio.format, audio.rate);
	#endif

	}

	if (im == 2) {
		thr_id = pthread_create(&p_thread, NULL, input_fifo,
														(void*)&audio); //starting fifomusic listener
		audio.rate = 44100;
	}


	if (highcf > audio.rate / 2) {
		cleanup();
		fprintf(stderr,
			"higher cuttoff frequency can't be higher then sample rate / 2"
		);
			exit(EXIT_FAILURE);
	}

	p =  fftw_plan_dft_r2c_1d(M, in, *out, FFTW_MEASURE); //planning to rock



	virt = system("setfont cava.psf  >/dev/null 2>&1");
	if (virt == 0) system("setterm -blank 0");


	//output: start ncurses mode
	if (om == 1 || om ==  2) {
	init_terminal_ncurses(col, bgcol);
	}



	while  (1) {//jumbing back to this loop means that you resized the screen
		for (i = 0; i < 200; i++) {
			flast[i] = 0;
			flastd[i] = 0;
			fall[i] = 0;
			fpeak[i] = 0;
			fmem[i] = 0;
			f[i] = 0;
		}


		//getting orignial numbers of bands incase of resize
		if (autoband == 1)  {
			bands = 25;
		} else bands = fixedbands;


		// output: get terminal's geometry
		if (om == 1 || om == 2) get_terminal_dim_ncurses(&w, &h);

		if (om == 3) get_terminal_dim_noncurses(&w, &h);


		if (bands > w / 2 - 1)bands = w / 2 -
											1; //handle for user setting to many bars

		if (bands < 1) bands = 1; // must have at least 1 bar;

		height = h - 1;

		bw = (w - bands - 1) / bands;

		if (bw < 1) bw = 1; //bars must have width

		// process [smoothing]: calculate gravity
		g = gravity * ((float)height / 270) * pow((60 / (float)framerate), 2.5);

		//if no bands are selected it tries to padd the default 20 if there is extra room
		if (autoband == 1) bands = bands + ((w - (bw * bands + bands - 1)) /
																					(bw + 1));


		//checks if there is stil extra room, will use this to center
		rest = (w - bands * bw - bands + 1) / 2;
		if (rest < 0)rest = 0;

		if ((smcount > 0) && (bands > 0)) {
			smh = (double)(((double)smcount)/((double)bands));
		}


		#ifdef DEBUG
			printw("hoyde: %d bredde: %d bands:%d bandbredde: %d rest: %d\n",
						 w,
						 h, bands, bw, rest);
		#endif

		//output: start noncurses mode
		if (om == 3) init_terminal_noncurses(col, bgcol, w, h, bw);

		double freqconst = log10((float)lowcf / (float)highcf) /  ((float)1 / ((float)bands + (float)1) - 1);
	
		//freqconst = -2;

		// process: calculate cutoff frequencies
		for (n = 0; n < bands + 1; n++) {
			fc[n] = highcf * pow(10, freqconst * (-1) + ((((float)n + 1) / ((float)bands + 1)) *
																			 freqconst)); //decided to cut it at 10k, little interesting to hear above
			fr[n] = fc[n] / (audio.rate /
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
							mvprintw(n,0,"%d: %f -> %f (%d -> %d) \n", n, fc[n - 1], fc[n], lcf[n - 1],
					 				 hcf[n - 1]);
						}
			#endif
		}

		// process: weigh signal to frequencies
		for (n = 0; n < bands;
			n++)k[n] = pow(fc[n],0.85) * ((float)height/(M*4000)) * smooth[(int)floor(((double)n) * smh)];

	   	cont = 1;
		// general: main loop
		while  (cont) {
		    //fprintf(stderr, "in main loop\n");
			// general: keyboard controls
			if (om == 1 || om == 2) ch = getch();
			switch (ch) {
				case 65:    // key up
					sens += 10;
					break;
				case 66:    // key down
					sens -= 10;
					break;
				case 67:    // key right
					break;
				case 68:    // key left
					break;
				case 'm':
					if (mode == modes) {
						mode = 1;
					} else {
						mode++;
					}
					break;
				case 'r': //reload config
					load_config();
					validate_config();
					cont = 0;
					break;
				case 'q':
					cleanup();
					return EXIT_SUCCESS;
			}



			#ifdef DEBUG
				//clear();
				refresh();
			#endif

			// process: populate input buffer and check if input is present
			lpeak = 0;
			hpeak = 0;
			for (i = 0; i < (2 * (M / 2 + 1)); i++) {
				if (i < M) {
					in[i] = audio.audio_out[i];
					if (audio.audio_out[i] > hpeak) hpeak = audio.audio_out[i];
					if (audio.audio_out[i] < lpeak) lpeak = audio.audio_out[i];
				} else in[i] = 0;
			}
			peak[bands] = (hpeak + abs(lpeak));
			if (peak[bands] == 0)sleep++;
			else sleep = 0;

			// process: if input was present for the last 5 seconds apply FFT to it
			if (sleep < framerate * 5) {

				// process: send input to external library
				fftw_execute(p);

				// process: separate frequency bands
				for (o = 0; o < bands; o++) {
					flastd[o] = f[o]; //saving last value for drawing
					peak[o] = 0;

					// process: get peaks
					for (i = lcf[o]; i <= hcf[o]; i++) {
						y[i] =  pow(pow(*out[i][0], 2) + pow(*out[i][1], 2), 0.5); //getting r of compex
						peak[o] += y[i]; //adding upp band
					}
					peak[o] = peak[o] / (hcf[o]-lcf[o]+1); //getting average
					temp = peak[o] * k[o] * ((float)sens / 100); //multiplying with k and adjusting to sens settings
					if (temp > height * 8)temp = height * 8; //just in case
					if (temp <= ignore)temp = 0;
					f[o] = temp;

				}

			} else { //**if in sleep mode wait and continue**//
			    //fprintf(stderr, "in sleep mode...\n");
				#ifdef DEBUG
					printw("no sound detected for 3 sec, going to sleep mode\n");
				#endif
				//wait 1 sec, then check sound again.
				req.tv_sec = 1;
				req.tv_nsec = 0;
				nanosleep (&req, NULL);
				continue;
			}

			// process [smoothing]
			if (mode != 2)
			{
				int z;

				// process [smoothing]: monstercat-style "average"

				int m_y, de;
				if (mode == 3) {
					for (z = 0; z < bands; z++) { // waves
						f[z] = f[z] / 1.25;
						if (f[z] < 0.125)f[z] = 0.125;
						for (m_y = z - 1; m_y >= 0; m_y--) {
							de = z - m_y;
							f[m_y] = max(f[z] - pow(de, 2), f[m_y]);
						}
						for (m_y = z + 1; m_y < bands; m_y++) {
							de = m_y - z;
							f[m_y] = max(f[z] - pow(de, 2), f[m_y]);
						}
					}
				} else if (monstercat > 0) {
					for (z = 0; z < bands; z++) {
						if (f[z] < 0.125)f[z] = 0.125;
						for (m_y = z - 1; m_y >= 0; m_y--) {
							de = z - m_y;
							f[m_y] = max(f[z] / pow(monstercat, de), f[m_y]);
						}
						for (m_y = z + 1; m_y < bands; m_y++) {
							de = m_y - z;
							f[m_y] = max(f[z] / pow(monstercat, de), f[m_y]);
						}
					}
				}

				// process [smoothing]: falloff
				if (g > 0) {
					for (o = 0; o < bands; o++) {
						temp = f[o];

						if (temp < flast[o]) {
							f[o] = fpeak[o] - (g * fall[o] * fall[o]);
							fall[o]++;
						} else if (temp >= flast[o]) {
							f[o] = temp;
							fpeak[o] = f[o];
							fall[o] = 0;
						}

						flast[o] = f[o];
					}
				}

				// process [smoothing]: integral
				if (integral > 0) {
					for (o = 0; o < bands; o++) {
						fmem[o] = fmem[o] * integral + f[o];
						f[o] = fmem[o];

						#ifdef DEBUG
							mvprintw(o,0,"%d: f:%f->%f (%d->%d)peak:%f adjpeak: %d \n", o, fc[o], fc[o + 1],
										 lcf[o], hcf[o], peak[o], f[o]);
						#endif
					}
				}

				// zero values causes divided by zero segfault.
				for (o = 0; o < bands; o++) {
					if (f[o] < 1)f[o] = 1;
				}

			}

			// output: draw processed input
			#ifndef DEBUG
				switch (om) {
					case 1:
					    //fprintf(stderr, "drawing\n");
					    rc = draw_terminal_ncurses(virt, h, w, bands, bw, rest, f, flastd);
						break;
					case 2:
						rc = draw_terminal_bcircle(virt, h, w, f);
						break;
					case 3:
						rc = draw_terminal_noncurses(virt, h, w, bands, bw, rest, f, flastd);
						break;
				}

				//fprintf(stderr, "done drawing\n");
				if (rc == -1) break; //terminal has been resized breaking to recalibrating values

				if (framerate <= 1) {
					req.tv_sec = 1  / (float)framerate;
				} else {
					req.tv_sec = 0;
					req.tv_nsec = (1 / (float)framerate) * 1000000000; //sleeping for set us
				}

				nanosleep (&req, NULL);
			#endif
		}
	}
}
