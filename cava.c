#define _XOPEN_SOURCE_EXTENDED
#include <locale.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#else
#include <stdlib.h>
#endif

#include <stdio.h>
#include <stddef.h>
#include <stdbool.h>
#include <termios.h>
#include <math.h>
#include <fcntl.h>

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
#include <ctype.h>

#ifdef NCURSES
#include "output/terminal_ncurses.h"
#include "output/terminal_ncurses.c"
#include "output/terminal_bcircle.h"
#include "output/terminal_bcircle.c"
#endif

#include "output/terminal_noncurses.h"
#include "output/terminal_noncurses.c"

#include "output/raw.h"
#include "output/raw.c"


#include "input/fifo.h"
#include "input/fifo.c"

#ifdef ALSA
#include <alsa/asoundlib.h>
#include "input/alsa.h"
#include "input/alsa.c"
#endif

#ifdef PULSE
#include "input/pulse.h"
#include "input/pulse.c"
#endif

#ifdef SNDIO
#include "input/sndio.c"
#endif

#include <iniparser.h>

#include "config.h"
#include "config.c"

#ifdef __GNUC__
// curses.h or other sources may already define
#undef  GCC_UNUSED
#define GCC_UNUSED __attribute__((unused))
#else
#define GCC_UNUSED /* nothing */
#endif

struct termios oldtio, newtio;
int rc;
int M = 2048;
int output_mode;



// whether we should reload the config or not
int should_reload = 0;
// whether we should only reload colors or not
int reload_colors = 0;


// general: cleanup
void cleanup(void)
{
	if (output_mode == 1 || output_mode == 2 ) {
	#ifdef NCURSES
	    cleanup_terminal_ncurses();
	#else
		;
	#endif
    }
    else if (output_mode ==3 ) {
	    cleanup_terminal_noncurses();
    }
}

// general: handle signals
void sig_handler(int sig_no)
{
	if (sig_no == SIGUSR1) {
		should_reload = 1;
		return;
	}

	if (sig_no == SIGUSR2) {
		reload_colors = 1;
		return;
	}

	cleanup();
	if (sig_no == SIGINT) {
		printf("CTRL-C pressed -- goodbye\n");
	}
	signal(sig_no, SIG_DFL);
	raise(sig_no);
}


#ifdef ALSA
static bool is_loop_device_for_sure(const char * text) {
	const char * const LOOPBACK_DEVICE_PREFIX = "hw:Loopback,";
	return strncmp(text, LOOPBACK_DEVICE_PREFIX, strlen(LOOPBACK_DEVICE_PREFIX)) == 0;
}

static bool directory_exists(const char * path) {
	DIR * const dir = opendir(path);
	bool exists;// = dir != NULL;
    if (dir == NULL) exists = false;
    else exists = true;
	closedir(dir);
	return exists;
}

#endif

int * separate_freq_bands(fftw_complex out[M / 2 + 1], int bars, int lcf[200],
			 int hcf[200], float k[200], int channel, double sens, double ignore) {
	int o,i;
	float peak[201];
	static int fl[200];
	static int fr[200];
	int y[M / 2 + 1];
	float temp;


	// process: separate frequency bands
	for (o = 0; o < bars; o++) {

		peak[o] = 0;

		// process: get peaks
		for (i = lcf[o]; i <= hcf[o]; i++) {

			//getting r of compex
			y[i] = hypot(out[i][0], out[i][1]);
			peak[o] += y[i]; //adding upp band
		}


		peak[o] = peak[o] / (hcf[o]-lcf[o]+1); //getting average
		temp = peak[o] * k[o] * sens; //multiplying with k and adjusting to sens settings
		if (temp <= ignore) temp = 0;
		if (channel == 1) fl[o] = temp;
		else fr[o] = temp;

	}

	if (channel == 1) return fl;
 	else return fr;
}


int * monstercat_filter (int * f, int bars, int waves, double monstercat) {

	int z;


	// process [smoothing]: monstercat-style "average"

	int m_y, de;
	if (waves > 0) {
		for (z = 0; z < bars; z++) { // waves
			f[z] = f[z] / 1.25;
			//if (f[z] < 1) f[z] = 1;
			for (m_y = z - 1; m_y >= 0; m_y--) {
				de = z - m_y;
				f[m_y] = max(f[z] - pow(de, 2), f[m_y]);
			}
			for (m_y = z + 1; m_y < bars; m_y++) {
				de = m_y - z;
				f[m_y] = max(f[z] - pow(de, 2), f[m_y]);
			}
		}
	} else if (monstercat > 0) {
		for (z = 0; z < bars; z++) {
			//if (f[z] < 1)f[z] = 1;
			for (m_y = z - 1; m_y >= 0; m_y--) {
				de = z - m_y;
				f[m_y] = max(f[z] / pow(monstercat, de), f[m_y]);
			}
			for (m_y = z + 1; m_y < bars; m_y++) {
				de = m_y - z;
				f[m_y] = max(f[z] / pow(monstercat, de), f[m_y]);
			}
		}
	}

	return f;

}


// general: entry point
int main(int argc, char **argv)
{


	// general: define variables
	pthread_t  p_thread;
	int thr_id GCC_UNUSED;
	float fc[200];
	float fre[200];
	int f[200], lcf[200], hcf[200];
	int *fl, *fr;
	int fmem[200];
	int flast[200];
	int flastd[200];
	int sleep = 0;
	int i, n, o, height, h, w, c, rest, inAtty, silence, fp, fptest;
	//int cont = 1;
	int fall[200];
	//float temp;
	float fpeak[200];
	float k[200];
	float g;
	struct timespec req = { .tv_sec = 0, .tv_nsec = 0 };
	char configPath[255];
	char *usage = "\n\
Usage : " PACKAGE " [options]\n\
Visualize audio input in terminal. \n\
\n\
Options:\n\
	-p          path to config file\n\
	-v          print version\n\
\n\
Keys:\n\
        Up        Increase sensitivity\n\
        Down      Decrease sensitivity\n\
        Left      Decrease number of bars\n\
        Right     Increase number of bars\n\
        r         Reload config\n\
        c         Reload colors only\n\
        f         Cycle foreground color\n\
        b         Cycle background color\n\
        q         Quit\n\
\n\
as of 0.4.0 all options are specified in config file, see in '/home/username/.config/cava/' \n";

	char ch = '\0';
	double inr[2 * (M / 2 + 1)];
    double inl[2 * (M / 2 + 1)];
	int bars = 25;
	char supportedInput[255] = "'fifo'";
	int sourceIsAuto = 1;
	double smh;

	//int maxvalue = 0;

	struct audio_data audio;
	struct config_params p;


	// general: console title
	printf("%c]0;%s%c", '\033', PACKAGE, '\007');
	
	configPath[0] = '\0';

	setlocale(LC_ALL, "");

	// general: handle Ctrl+C
	struct sigaction action;
	memset(&action, 0, sizeof(action));
	action.sa_handler = &sig_handler;
	sigaction(SIGINT, &action, NULL);
	sigaction(SIGTERM, &action, NULL);
	sigaction(SIGUSR1, &action, NULL);
	sigaction(SIGUSR2, &action, NULL);

	// general: handle command-line arguments
	while ((c = getopt (argc, argv, "p:vh")) != -1) {
		switch (c) {
			case 'p': // argument: fifo path
				snprintf(configPath, sizeof(configPath), "%s", optarg);
				break;
			case 'h': // argument: print usage
				printf ("%s", usage);
				return 1;
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

    #ifdef ALSA
        strcat(supportedInput,", 'alsa'");
    #endif
    #ifdef PULSE
        strcat(supportedInput,", 'pulse'");
    #endif
    #ifdef SNDIO
        strcat(supportedInput,", 'sndio'");
    #endif

	//fft: planning to rock
	fftw_complex outl[M / 2 + 1];
	fftw_plan pl =  fftw_plan_dft_r2c_1d(M, inl, outl, FFTW_MEASURE);

	fftw_complex outr[M / 2 + 1];
	fftw_plan pr =  fftw_plan_dft_r2c_1d(M, inr, outr, FFTW_MEASURE);

	// general: main loop
	while (1) {

	//config: load
	struct error_s error;
	error.length = 0;
	if (!load_config(configPath, supportedInput, (void *)&p, 0, &error)) {
    	fprintf(stderr, "Error loading config. %s", error.message);
        exit(EXIT_FAILURE);
	}

    output_mode = p.om;

	if (p.om != 4) {
		// Check if we're running in a tty
		inAtty = 0;
		if (strncmp(ttyname(0), "/dev/tty", 8) == 0 || strcmp(ttyname(0),
			 "/dev/console") == 0) inAtty = 1;

		if (inAtty) {
			system("setfont cava.psf  >/dev/null 2>&1");
			system("setterm -blank 0");
		}
	}


	//input: init
	audio.source = malloc(1 +  strlen(p.audio_source));
	strcpy(audio.source, p.audio_source);

	audio.format = -1;
	audio.rate = 0;
	audio.terminate = 0;
	if (p.stereo) audio.channels = 2;
	if (!p.stereo) audio.channels = 1;

	for (i = 0; i < M; i++) {
		audio.audio_out_l[i] = 0;
		audio.audio_out_r[i] = 0;
	}

	#ifdef ALSA
	// input_alsa: wait for the input to be ready
	if (p.im == 1) {
		if (is_loop_device_for_sure(audio.source)) {
			if (directory_exists("/sys/")) {
				if (! directory_exists("/sys/module/snd_aloop/")) {
      				cleanup();
					fprintf(stderr,
					"Linux kernel module \"snd_aloop\" does not seem to  be loaded.\n"
					"Maybe run \"sudo modprobe snd_aloop\".\n");
					exit(EXIT_FAILURE);
				}
			}
		}

		thr_id = pthread_create(&p_thread, NULL, input_alsa,
														(void *)&audio); //starting alsamusic listener

		n = 0;

		while (audio.format == -1 || audio.rate == 0) {
			req.tv_sec = 0;
			req.tv_nsec = 1000000;
			nanosleep (&req, NULL);
			n++;
			if (n > 2000) {
				cleanup();
				fprintf(stderr,
				"could not get rate and/or format, problems with audio thread? quiting...\n");
				exit(EXIT_FAILURE);
			}
		}
	#ifdef DEBUG
		printf("got format: %d and rate %d\n", audio.format, audio.rate);
	#endif

	}
	#endif

	if (p.im == 2) {
		//starting fifomusic listener
		thr_id = pthread_create(&p_thread, NULL, input_fifo, (void*)&audio);
		audio.rate = 44100;
	}

	#ifdef PULSE
	if (p.im == 3) {
		if (strcmp(audio.source, "auto") == 0) {
			getPulseDefaultSink((void*)&audio);
			sourceIsAuto = 1;
			}
		else sourceIsAuto = 0;
		//starting pulsemusic listener
		thr_id = pthread_create(&p_thread, NULL, input_pulse, (void*)&audio);
		audio.rate = 44100;
	}
	#endif

	#ifdef SNDIO
	if (p.im == 4) {
		thr_id = pthread_create(&p_thread, NULL, input_sndio, (void*)&audio);
		audio.rate = 44100;
	}
	#endif

	if (p.highcf > audio.rate / 2) {
		cleanup();
		fprintf(stderr,
			"higher cuttoff frequency can't be higher then sample rate / 2"
		);
			exit(EXIT_FAILURE);
	}



	bool reloadConf = false;
	bool senseLow = true;

	while  (!reloadConf) {//jumbing back to this loop means that you resized the screen
		for (i = 0; i < 200; i++) {
			flast[i] = 0;
			flastd[i] = 0;
			fall[i] = 0;
			fpeak[i] = 0;
			fmem[i] = 0;
			f[i] = 0;
		}

		#ifdef NCURSES
		//output: start ncurses mode
		if (p.om == 1 || p.om ==  2) {
			init_terminal_ncurses(p.color, p.bcolor, p.col,
			p.bgcol, p.gradient, p.gradient_count, p.gradient_colors,&w, &h);
			//get_terminal_dim_ncurses(&w, &h);
		}
		#endif

		if (p.om == 3) get_terminal_dim_noncurses(&w, &h);

		height = (h - 1) * 8;

		// output open file/fifo for raw output
		if (p.om == 4) {

			if (strcmp(p.raw_target,"/dev/stdout") != 0) {

				//checking if file exists
				if( access( p.raw_target, F_OK ) != -1 ) {
					//testopening in case it's a fifo
					fptest = open(p.raw_target, O_RDONLY | O_NONBLOCK, 0644);
	
					if (fptest == -1) {
						printf("could not open file %s for writing\n",
							p.raw_target);
						exit(1);
					}
				} else {
					printf("creating fifo %s\n",p.raw_target);
					if (mkfifo(p.raw_target, 0664) == -1) {
						printf("could not create fifo %s\n",
							p.raw_target);
						exit(1);
					}
					//fifo needs to be open for reading in order to write to it
					fptest = open(p.raw_target, O_RDONLY | O_NONBLOCK, 0644);
				}
		    }

			fp = open(p.raw_target, O_WRONLY | O_NONBLOCK | O_CREAT, 0644);
			if (fp == -1) {
				printf("could not open file %s for writing\n",p.raw_target);
				exit(1);
			}
			printf("open file %s for writing raw ouput\n",p.raw_target);

            //width must be hardcoded for raw output.
			w = 200;

    		if (strcmp(p.data_format, "binary") == 0) {
                height = pow(2, p.bit_format) - 1;
            } else {
                height = p.ascii_range;
            }



		}

 		//handle for user setting too many bars
		if (p.fixedbars) {
			p.autobars = 0;
			if (p.fixedbars * p.bw + p.fixedbars * p.bs - p.bs > w) p.autobars = 1;
		}

		//getting orignial numbers of barss incase of resize
		if (p.autobars == 1)  {
			bars = (w + p.bs) / (p.bw + p.bs);
			//if (p.bs != 0) bars = (w - bars * p.bs + p.bs) / bw;
		} else bars = p.fixedbars;


		if (bars < 1) bars = 1; // must have at least 1 bars
        if (bars > 200) bars = 200; // cant have more than 200 bars

		if (p.stereo) { //stereo must have even numbers of bars
			if (bars%2 != 0) bars--;
		}





		// process [smoothing]: calculate gravity
		g = p.gravity * ((float)height / 2160) * pow((60 / (float)p.framerate), 2.5);


		//checks if there is stil extra room, will use this to center
		rest = (w - bars * p.bw - bars * p.bs + p.bs) / 2;
		if (rest < 0)rest = 0;

		#ifdef DEBUG
			printw("height: %d width: %d bars:%d bar width: %d rest: %d\n",
						 w,
						 h, bars, p.bw, rest);
		#endif

		//output: start noncurses mode
		if (p.om == 3) init_terminal_noncurses(p.col, p.bgcol, w, h, p.bw);



		if (p.stereo) bars = bars / 2; // in stereo onle half number of bars per channel

		if ((p.smcount > 0) && (bars > 0)) {
			smh = (double)(((double)p.smcount)/((double)bars));
		}


		double freqconst = log10((float)p.lowcf / (float)p.highcf) /
			((float)1 / ((float)bars + (float)1) - 1);

		//freqconst = -2;

		// process: calculate cutoff frequencies
		for (n = 0; n < bars + 1; n++) {
			fc[n] = p.highcf * pow(10, freqconst * (-1) + ((((float)n + 1) /
				((float)bars + 1)) * freqconst));
			fre[n] = fc[n] / (audio.rate / 2);
			//remember nyquist!, pr my calculations this should be rate/2
			//and  nyquist freq in M/2 but testing shows it is not...
			//or maybe the nq freq is in M/4

			//lfc stores the lower cut frequency foo each bar in the fft out buffer
			lcf[n] = fre[n] * (M /2);
			if (n != 0) {
				hcf[n - 1] = lcf[n] - 1;
	
				//pushing the spectrum up if the expe function gets "clumped"
				if (lcf[n] <= lcf[n - 1])lcf[n] = lcf[n - 1] + 1;
				hcf[n - 1] = lcf[n] - 1;
			}

			#ifdef DEBUG
			 	if (n != 0) {
					mvprintw(n,0,"%d: %f -> %f (%d -> %d) \n", n,
						fc[n - 1], fc[n], lcf[n - 1],
			 				 hcf[n - 1]);
						}
			#endif
		}

		// process: weigh signal to frequencies
		for (n = 0; n < bars;
			n++)k[n] = pow(fc[n],0.85) * ((float)height/(M*32000)) *
				p.smooth[(int)floor(((double)n) * smh)];

		if (p.stereo) bars = bars * 2;

		bool resizeTerminal = false;

		while  (!resizeTerminal) {

			// general: keyboard controls
			#ifdef NCURSES
			if (p.om == 1 || p.om == 2) ch = getch();
			#endif

			switch (ch) {
				case 65:    // key up
					p.sens = p.sens * 1.05;
					break;
				case 66:    // key down
					p.sens = p.sens * 0.95;
					break;
				case 68:    // key right
					p.bw++;
					resizeTerminal = true;
					break;
				case 67:    // key left
					if (p.bw > 1) p.bw--;
					resizeTerminal = true;
					break;
				case 'r': //reload config
					should_reload = 1;
					break;
				case 'c': //reload colors
					reload_colors = 1;
					break;
				case 'f': //change forground color
					if (p.col < 7) p.col++;
					else p.col = 0;
					resizeTerminal = true;
					break;
				case 'b': //change backround color
					if (p.bgcol < 7) p.bgcol++;
					else p.bgcol = 0;
					resizeTerminal = true;
					break;

				case 'q':
					cleanup();
					return EXIT_SUCCESS;
			}

			if (should_reload) {

				reloadConf = true;
				resizeTerminal = true;
				should_reload = 0;

			}

			if (reload_colors) {
    			struct error_s error;
    			error.length = 0;
				if (!load_config(configPath, supportedInput, (void *)&p, 1, &error)) {
    			    cleanup();
                	fprintf(stderr, "Error loading config. %s", error.message);
                    exit(EXIT_FAILURE);
				}
				resizeTerminal = true;
				reload_colors = 0;
			}

			//if (cont == 0) break;

			#ifdef DEBUG
				//clear();
				refresh();
			#endif

			// process: populate input buffer and check if input is present
			silence = 1;
			for (i = 0; i < (2 * (M / 2 + 1)); i++) {
				if (i < M) {
					inl[i] = audio.audio_out_l[i];
					if (p.stereo) inr[i] = audio.audio_out_r[i];
					if (inl[i] || inr[i]) silence = 0;
				} else {
					inl[i] = 0;
					if (p.stereo) inr[i] = 0;
				}
			}

			if (silence == 1)sleep++;
			else sleep = 0;

			// process: if input was present for the last 5 seconds apply FFT to it
			if (sleep < p.framerate * 5) {

				// process: execute FFT and sort frequency bands
				if (p.stereo) {
					fftw_execute(pl);
					fftw_execute(pr);

					fl = separate_freq_bands(outl,bars/2,lcf,hcf, k, 1,
						p.sens, p.ignore);
					fr = separate_freq_bands(outr,bars/2,lcf,hcf, k, 2,
						p.sens, p.ignore);
				} else {
					fftw_execute(pl);
					fl = separate_freq_bands(outl,bars,lcf,hcf, k, 1,
						p.sens, p.ignore);
				}


			}
			 else { //**if in sleep mode wait and continue**//
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

			if (p.monstercat) {
				if (p.stereo) {
					fl = monstercat_filter(fl, bars / 2, p.waves,
					 	p.monstercat);
					fr = monstercat_filter(fr, bars / 2, p.waves,
						p.monstercat);	
				} else {
					fl = monstercat_filter(fl, bars, p.waves, p.monstercat);
				}
			}


			//preperaing signal for drawing
			for (o = 0; o < bars; o++) {
				if (p.stereo) {
					if (o < bars / 2) {
						f[o] = fl[bars / 2 - o - 1];
					} else {
						f[o] = fr[o - bars / 2];
					}

				} else {
					f[o] = fl[o];
				}
			}


			// process [smoothing]: falloff
			if (g > 0) {
				for (o = 0; o < bars; o++) {
					if (f[o] < flast[o]) {
						f[o] = fpeak[o] - (g * fall[o] * fall[o]);
						fall[o]++;
					} else  {
						fpeak[o] = f[o];
						fall[o] = 0;
					}

					flast[o] = f[o];
				}
			}

			// process [smoothing]: integral
			if (p.integral > 0) {
				for (o = 0; o < bars; o++) {
					f[o] = fmem[o] * p.integral + f[o];
					fmem[o] = f[o];

					int diff = (height + 1) - f[o];
					if (diff < 0) diff = 0;
					double div = 1 / (diff + 1);
					//f[o] = f[o] - pow(div, 10) * (height + 1);
					fmem[o] = fmem[o] * (1 - div / 20);

					#ifdef DEBUG
						mvprintw(o,0,"%d: f:%f->%f (%d->%d)peak:%d \n",
							 o, fc[o], fc[o + 1],
									 lcf[o], hcf[o], f[o]);
					#endif
				}
			}


			// zero values causes divided by zero segfault
			for (o = 0; o < bars; o++) {
				if (f[o] < 1) {
					f[o] = 1;
					if (p.om == 4) f[o] = 0;
				}
				//if(f[o] > maxvalue) maxvalue = f[o];
			}

			//printf("%d\n",maxvalue); //checking maxvalue I keep forgetting its about 10000

			//autmatic sens adjustment
			if (p.autosens) {
				for (o = 0; o < bars; o++) {
					if (f[o] > height ) {
						senseLow = false;
						p.sens = p.sens * 0.985;
						break;
					}
					if (senseLow && !silence) p.sens = p.sens * 1.01;
				if (o == bars - 1) p.sens = p.sens * 1.002;
				}
			}
			
			// output: draw processed input
			#ifndef DEBUG
				switch (p.om) {
					case 1:
						#ifdef NCURSES
						rc = draw_terminal_ncurses(inAtty, h, w, bars,
							p.bw, p.bs, rest, f, flastd, p.gradient);
						break;
						#endif
					case 2:
						#ifdef NCURSES
						rc = draw_terminal_bcircle(inAtty, h, w, f);
						break;
						#endif
					case 3:
						rc = draw_terminal_noncurses(inAtty, h, w, bars,
							 p.bw, p.bs, rest, f, flastd);
						break;
					case 4:
						rc = print_raw_out(bars, fp, p.is_bin,
							p.bit_format, p.ascii_range, p.bar_delim,
							 p.frame_delim,f);
						break;
				}

				//terminal has been resized breaking to recalibrating values
				if (rc == -1) resizeTerminal = true;

				if (p.framerate <= 1) {
					req.tv_sec = 1  / (float)p.framerate;
				} else {
					req.tv_sec = 0;
					req.tv_nsec = (1 / (float)p.framerate) * 1000000000;
				}

				nanosleep (&req, NULL);
			#endif

			for (o = 0; o < bars; o++) {
				flastd[o] = f[o];
			}

            //checking if audio thread has exited unexpectedly
            if (audio.terminate == 1) {
                cleanup();
   				fprintf(stderr,
                "Audio thread exited unexpectedly. %s\n", audio.error_message);
                exit(EXIT_FAILURE);
            }

		}//resize terminal

	}//reloading config
	req.tv_sec = 0;
	req.tv_nsec = 100; //waiting some time to make shure audio is ready
	nanosleep (&req, NULL);

	//**telling audio thread to terminate**//
	audio.terminate = 1;
	pthread_join( p_thread, NULL);

	if (p.customEQ) free(p.smooth);
	if (sourceIsAuto) free(audio.source);

    cleanup();

	//fclose(fp);
	}
}
