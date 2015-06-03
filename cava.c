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
#include <time.h>
#include <getopt.h>
#include <pthread.h>
#include "output/terminal_ncurses.h"
#include "output/terminal_ncurses.c"
#include "input/alsa.h"
#include "input/alsa.c"
#include "input/fifo.h"
#include "input/fifo.c"


#ifdef __GNUC__
// curses.h or other sources may already define
#undef  GCC_UNUSED
#define GCC_UNUSED __attribute__((unused))
#else
#define GCC_UNUSED /* nothing */
#endif




bool scientificMode = false;

struct termios oldtio, newtio;
int rc;

// general: cleanup
void cleanup()
{
	cleanup_terminal_ncurses();
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



// general: entry point
int main(int argc, char **argv)
{
	int M = 2048;
	pthread_t  p_thread;
	int        thr_id GCC_UNUSED;
	char *inputMethod = "alsa";
	char *outputMethod = "terminal";
	int im = 1;
	int om = 1;
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
	int bands = 25;
	int sleep = 0;
	int i, n, o, bw, height, h, w, c, rest, virt, fixedbands;
	int autoband = 1;
	float temp;
	double in[2 * (M / 2 + 1)];
	fftw_complex out[M / 2 + 1][2];
	fftw_plan p;
	char *color;
	int col = 6;
	int bgcol = -1;
	int sens = 100;
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
	float sm = 1.25; //min val from smooth[]
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
	struct audio_data audio;

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
	while ((c = getopt (argc, argv, "p:i:b:d:s:f:c:C:hSv")) != -1)
		switch (c) {
			case 'p': // argument: fifo path
				audio.source = optarg;
				break;
			case 'i': // argument: input method
				im = 0;
				inputMethod = optarg;
				if (strcmp(inputMethod, "alsa") == 0) im = 1;
				if (strcmp(inputMethod, "fifo") == 0) im = 2;
				if (im == 0) {	
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
				audio.source  = optarg;
				break;
			case 's': // argument: sensitivity
				sens = atoi(optarg);
				break;
			case 'f': // argument: framerate
				framerate = atoi(optarg);
				if (framerate < 0) {	
					fprintf(stderr,
						"framerate can't be negative!\n");
					exit(EXIT_FAILURE);
				}
				break;
			case 'c': // argument: foreground color
				col = -2;
				color = optarg;
				if (strcmp(color, "black") == 0) col = 0;
				if (strcmp(color, "red") == 0) col = 1;
				if (strcmp(color, "green") == 0) col = 2;
				if (strcmp(color, "yellow") == 0) col = 3;
				if (strcmp(color, "blue") == 0) col = 4;
				if (strcmp(color, "magenta") == 0) col = 5;
				if (strcmp(color, "cyan") == 0) col = 6;
				if (strcmp(color, "white") == 0) col = 7;
				if (col == -2) {	
					fprintf(stderr, "color %s not supported\n", color);
					exit(EXIT_FAILURE);
				}
				break;
			case 'C': // argument: background color
				bgcol = -2;
				color = optarg;
				if (strcmp(color, "black") == 0) bgcol = 0;
				if (strcmp(color, "red") == 0) bgcol = 1;
				if (strcmp(color, "green") == 0) bgcol = 2;
				if (strcmp(color, "yellow") == 0) bgcol = 3;
				if (strcmp(color, "blue") == 0) bgcol = 4;
				if (strcmp(color, "magenta") == 0) bgcol = 5;
				if (strcmp(color, "cyan") == 0) bgcol = 6;
				if (strcmp(color, "white") == 0) bgcol = 7;
				if (bgcol == -2) {	
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

		n = 0;



	// input: wait for the input to be ready
	if (im == 1) {
		audio.source = "hw:1,1";
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
		printf("got format: %d and rate %d\n", format, rate);	
	#endif

	}

	if (im == 2) {
		audio.source =	"/tmp/mpd.fifo";
		thr_id = pthread_create(&p_thread, NULL, input_fifo,
		                        (void*)&audio); //starting fifomusic listener
		audio.rate = 44100;
	}

	p =  fftw_plan_dft_r2c_1d(M, in, *out, FFTW_MEASURE); //planning to rock



	if (framerate <= 1) {
		req.tv_sec = 1  / (float)framerate;
	} else { 
		req.tv_sec = 0;
		req.tv_nsec = (1 / (float)framerate) * 1000000000; //sleeping for set us
	}

	virt = system("setfont cava.psf  >/dev/null 2>&1");
	if (virt == 0) system("setterm -blank 0");


	//output: start ncurses mode
	init_terminal_ncurses(col, bgcol);



	while  (1) {//jumbing back to this loop means that you resized the screen
		for (i = 0; i < 200; i++) {
			flast[i] = 0;
			flastd[i] = 0;
			fall[i] = 0;
			fpeak[i] = 0;
			fmem[i] = 0;
		}

	
	
		//getting orignial numbers of bands incase of resize
		if (autoband == 1)  {
			bands = 25;
		} else bands = fixedbands;
		

		// output: get terminal's geometry
		get_terminal_dim_ncurses(&w, &h);		


		if (bands > w / 2 - 1)bands = w / 2 -
			                1; //handle for user setting to many bars

		if (bands < 1) bands = 1; // must have at least 1 bar;

		height = h - 1;

		bw = (w - bands - 1) / bands;

		if (bw < 1) bw = 1; //bars must have width

		// process [smoothing]: calculate gravity
		g = ((float)height / 400) * pow((60 / (float)framerate), 2.5);

		//if no bands are selected it tries to padd the default 20 if there is extra room
		if (autoband == 1) bands = bands + ((w - (bw * bands + bands - 1)) /
			                                    (bw + 1));


		//checks if there is stil extra room, will use this to center
		rest = (w - bands * bw - bands + 1) / 2;
		if (rest < 0)rest = 0;

		#ifdef DEBUG
			printw("hoyde: %d bredde: %d bands:%d bandbredde: %d rest: %d\n",
			       w,
			       h, bands, bw, rest);
		#endif

		// process: calculate cutoff frequencies
		for (n = 0; n < bands + 1; n++) {
			fc[n] = 10000 * pow(10, -2.37 + ((((float)n + 1) / ((float)bands + 1)) *
			                                 2.37)); //decided to cut it at 10k, little interesting to hear above
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
							printw("%d: %f -> %f (%d -> %d) \n", n, fc[n - 1], fc[n], lcf[n - 1],
							       hcf[n - 1]);
						}
			#endif
		}

		// process: weigh signal to frequencies
		for (n = 0; n < bands;
			n++)k[n] = pow(fc[n],0.62) * ((float)height/(M*3500))  * 8;
					                         

	
		// general: main loop
		while  (1) {

			// general: keyboard controls
			
			ch = getch();
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
			case 's':
				scientificMode = !scientificMode;
				break;
			case 'q':
				cleanup();
				return EXIT_SUCCESS;
			}
		


			#ifdef DEBUG
				system("clear");
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
					peak[o] = 0;

					// process: get peaks
					for (i = lcf[o]; i <= hcf[o]; i++) {
						y[i] = pow(pow(*out[i][0], 2) + pow(*out[i][1], 2), 0.5); //getting r of compex
						peak[o] += y[i]; //adding upp band
					}
					peak[o] = peak[o] / (hcf[o]-lcf[o]+1); //getting average
					temp = peak[o] * k[o] * ((float)sens / 100); //multiplying with k and adjusting to sens settings
					if (temp > height * 8)temp = height * 8; //just in case
					f[o] = temp;

					
				}

			} else { //**if in sleep mode wait and continue**//
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
			if (!scientificMode)
			{

				// process [smoothing]: falloff
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

				// process [smoothing]: monstercat-style "average"
				int z, m_y;
				float m_o = 64 / bands;
				for (z = 0; z < bands; z++) {
					f[z] = f[z] * sm / smooth[(int)floor(z * m_o)];
					if (f[z] < 0.125)f[z] = 0.125;
					for (m_y = z - 1; m_y >= 0; m_y--) {
						f[m_y] = max(f[z] / pow(2, z - m_y), f[m_y]);
					}
					for (m_y = z + 1; m_y < bands; m_y++) {
						f[m_y] = max(f[z] / pow(2, m_y - z), f[m_y]);
					}
				}
				// process [smoothing]: integral
				for (o = 0; o < bands; o++) {
					fmem[o] = fmem[o] * 0.55 + f[o];
					f[o] = fmem[o];

					if (f[o] < 1)f[o] = 1;

					#ifdef DEBUG
						mvprintw(o,0,"%d: f:%f->%f (%d->%d)peak:%f adjpeak: %f \n", o, fc[o], fc[o + 1],
						       lcf[o], hcf[o], peak[o], f[o]);
					#endif
				}
			}

			// output: draw processed input
			#ifndef DEBUG
				switch (om) {
					case 1:

						rc = draw_terminal_ncurses(virt, h, w, bands, bw, rest, f, flastd);
						break;
				}

				if (rc == -1) break; //terminal has been resized breaking to recalibrating values
			

				nanosleep (&req, NULL);
			#endif
		}
	}
}
