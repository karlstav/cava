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

#ifdef PORTAUDIO
#include "input/portaudio.c"
#include "input/portaudio.h"
#endif

#ifdef PULSE
#include "input/pulse.h"
#include "input/pulse.c"
#endif

#ifdef SNDIO
#include "input/sndio.c"
#endif

#ifdef SHMEM
#include "input/shmem.c"
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

//struct termios oldtio, newtio;
//int M = 8 * 1024;



//used by sig handler 
//needs to know output mode in orer to clean up terminal
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

int * separate_freq_bands(int FFTbassbufferSize, fftw_complex out_bass[FFTbassbufferSize / 2 + 1],
		int FFTmidbufferSize, fftw_complex out_mid[FFTmidbufferSize / 2 + 1],
		int FFTtreblebufferSize, fftw_complex out_treble[FFTtreblebufferSize / 2 + 1],
		int bass_cut_off_bar, int treble_cut_off_bar,
			int bars, int lcf[200],  int hcf[200], double k[200], int channel, 
			double sens, double ignore) {
	int o, i;
	double peak[201];
	static int fl[200];
	static int fr[200];
	double y[FFTbassbufferSize / 2 + 1];
	double temp;

	// process: separate frequency bands
	for (o = 0; o < bars; o++) {

		peak[o] = 0;
		i = 0;

		// process: get peaks
		for (i = lcf[o]; i <= hcf[o]; i++) {
			if (o <= bass_cut_off_bar) {
				y[i] = hypot(out_bass[i][0], out_bass[i][1]);
			} else if (o > bass_cut_off_bar && o <= treble_cut_off_bar) {
				y[i] = hypot(out_mid[i][0], out_mid[i][1]);
			} else if (o > treble_cut_off_bar) {
				y[i] = hypot(out_treble[i][0], out_treble[i][1]);
			}
			
			peak[o] += y[i]; //adding upp band
		}

		peak[o] = peak[o] / (hcf[o]-lcf[o] + 1); //getting average
		temp = peak[o] * sens * k[o]; //multiplying with k and sens
		//printf("%d peak o: %f * sens: %f * k: %f = f: %f\n", o, peak[o], sens, k[o], temp);
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
	int i, n, o, height, h, w, c, rest, inAtty, fp, fptest, rc;
	bool silence;
	//int cont = 1;
	int fall[200];
	//float temp;
	float fpeak[200];
	double k[200];
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
	int bars = 25;
	char supportedInput[255] = "'fifo'";
	int sourceIsAuto = 1;
	double smh;

	double *in_bass_r, *in_bass_l;
	fftw_complex *out_bass_l, *out_bass_r;
	double *in_mid_r, *in_mid_l;
	fftw_complex *out_mid_l, *out_mid_r;
	double *in_treble_r, *in_treble_l;
	fftw_complex *out_treble_l, *out_treble_r;

	struct audio_data audio;
	struct config_params p;


	//int maxvalue = 0;
	
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
    #ifdef SHMEM
        strcat(supportedInput,", 'shmem'");
    #endif



	// general: main loop
	while (1) {

	#ifdef DEBUG
		printf("loading config\n");
	#endif
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

		//in macos vitual terminals are called ttys(xyz) and there are no ttys
		if (strncmp(ttyname(0), "/dev/ttys", 9) == 0)  inAtty = 0;
		if (inAtty) {
			system("setfont cava.psf  >/dev/null 2>&1");
			system("setterm -blank 0");
		}
	}


	//input: init
	int bass_cut_off = 150;
	int treble_cut_off = 1500;

	audio.source = malloc(1 +  strlen(p.audio_source));
	strcpy(audio.source, p.audio_source);

	audio.format = -1;
	audio.rate = 0;
	audio.FFTbassbufferSize = 4096;
	audio.FFTmidbufferSize = 1024;
	audio.FFTtreblebufferSize = 512;
	audio.terminate = 0;
	if (p.stereo) audio.channels = 2;
	if (!p.stereo) audio.channels = 1;
	audio.average = false;
	audio.left = false;
	audio.right = false;
	if (strcmp(p.mono_option, "average") == 0) audio.average = true;
	if (strcmp(p.mono_option, "left") == 0) audio.left = true;
	if (strcmp(p.mono_option, "right") == 0) audio.right = true;
	audio.bass_index = 0;
	audio.mid_index = 0;
	audio.treble_index = 0;


	#ifdef DEBUG
		printf("starting audio thread\n");
	#endif
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

	#ifdef SHMEM
	if (p.im == 5) {
		thr_id = pthread_create(&p_thread, NULL, input_shmem, (void*)&audio);
		//audio.rate = 44100;
	}
	#endif

	#ifdef PORTAUDIO
	if (p.im == 6) {
		thr_id = pthread_create(&p_thread, NULL, input_portaudio, (void*)&audio);
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

	//BASS
	//audio.FFTbassbufferSize =  audio.rate / 20; // audio.FFTbassbufferSize;

	in_bass_r = malloc(2 * (audio.FFTbassbufferSize / 2 + 1) * sizeof(double));
	in_bass_l = malloc(2 * (audio.FFTbassbufferSize / 2 + 1) * sizeof(double));

	out_bass_l = malloc(2 * (audio.FFTbassbufferSize / 2 + 1) * sizeof(fftw_complex));
	out_bass_r = malloc(2 * (audio.FFTbassbufferSize / 2 + 1) * sizeof(fftw_complex));

	fftw_plan p_bass_l =  fftw_plan_dft_r2c_1d(audio.FFTbassbufferSize, in_bass_l, out_bass_l, FFTW_MEASURE);
	fftw_plan p_bass_r =  fftw_plan_dft_r2c_1d(audio.FFTbassbufferSize, in_bass_r, out_bass_r, FFTW_MEASURE);

	//MID
	//audio.FFTmidbufferSize =  audio.rate / bass_cut_off; // audio.FFTbassbufferSize;
	in_mid_r = malloc(2 * (audio.FFTmidbufferSize / 2 + 1) * sizeof(double));
	in_mid_l = malloc(2 * (audio.FFTmidbufferSize / 2 + 1) * sizeof(double));

	out_mid_l = malloc(2 * (audio.FFTmidbufferSize / 2 + 1) * sizeof(fftw_complex));
	out_mid_r = malloc(2 * (audio.FFTmidbufferSize / 2 + 1) * sizeof(fftw_complex));

	fftw_plan p_mid_l =  fftw_plan_dft_r2c_1d(audio.FFTmidbufferSize, in_mid_l, out_mid_l, FFTW_MEASURE);
	fftw_plan p_mid_r =  fftw_plan_dft_r2c_1d(audio.FFTmidbufferSize, in_mid_r, out_mid_r, FFTW_MEASURE);

	//TRIEBLE
	//audio.FFTtreblebufferSize =  audio.rate / treble_cut_off; // audio.FFTbassbufferSize;
	in_treble_r = malloc(2 * (audio.FFTtreblebufferSize / 2 + 1) * sizeof(double));
	in_treble_l = malloc(2 * (audio.FFTtreblebufferSize / 2 + 1) * sizeof(double));

	out_treble_l = malloc(2 * (audio.FFTtreblebufferSize / 2 + 1) * sizeof(fftw_complex));
	out_treble_r = malloc(2 * (audio.FFTtreblebufferSize / 2 + 1) * sizeof(fftw_complex));

	fftw_plan p_treble_l =  fftw_plan_dft_r2c_1d(audio.FFTtreblebufferSize, in_treble_l, out_treble_l, FFTW_MEASURE);
	fftw_plan p_treble_r =  fftw_plan_dft_r2c_1d(audio.FFTtreblebufferSize, in_treble_r, out_treble_r, FFTW_MEASURE);

	#ifdef DEBUG
		printf("got buffer size: %d, %d, %d", audio.FFTbassbufferSize, audio.FFTmidbufferSize, audio.FFTtreblebufferSize);
		printf("zeroing buffers\n");
	#endif
	for (i = 0; i < (audio.FFTbassbufferSize / 2 + 1); i++) {
		if (i < audio.FFTbassbufferSize) {
			audio.audio_out_bass_l[i] = 0;
			audio.audio_out_bass_r[i] = 0;
		}
		in_bass_l[i] = 0;
		in_bass_r[i] = 0;
		for (n = 0; n < 2; n++) {
			out_bass_l[i][n] = 0;
			out_bass_r[i][n] = 0;
		}
	}

	for (i = 0; i < (audio.FFTmidbufferSize / 2 + 1); i++) {
		if (i < audio.FFTmidbufferSize) {
			audio.audio_out_mid_l[i] = 0;
			audio.audio_out_mid_r[i] = 0;
		}
		in_mid_l[i] = 0;
		in_mid_r[i] = 0;
		for (n = 0; n < 2; n++) {
			out_mid_l[i][n] = 0;
			out_mid_r[i][n] = 0;
		}
	}

	for (i = 0; i < (audio.FFTtreblebufferSize / 2 + 1); i++) {
		if (i < audio.FFTtreblebufferSize) {
			audio.audio_out_treble_l[i] = 0;
			audio.audio_out_treble_r[i] = 0;
		}
		in_treble_l[i] = 0;
		in_treble_r[i] = 0;
		for (n = 0; n < 2; n++) {
			out_treble_l[i][n] = 0;
			out_treble_r[i][n] = 0;
		}
	}



	bool reloadConf = false;

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
		int bass_cut_off_bar = -1;
		int treble_cut_off_bar = -1;
		bool first_bar = false;
		int first_treble_bar = 0;
		for (n = 0; n < bars + 1; n++) {
			double pot = freqconst * (-1); 
			pot +=  ((float)n + 1) / ((float)bars + 1) * freqconst;
			fc[n] = p.highcf * pow(10, pot);
			fre[n] = fc[n] / (audio.rate / 2);
			//remember nyquist!, pr my calculations this should be rate/2
			//and  nyquist freq in M/2 but testing shows it is not...
			//or maybe the nq freq is in M/4

			k[n] = pow(fc[n],1);
			k[n] *= (float)height / pow(2,28);
			k[n] *=	p.smooth[(int)floor(((double)n) * smh)];
			k[n] /= log2(audio.FFTbassbufferSize);
			//lfc stores the lower cut frequency foo each bar in the fft out buffer
			if (fc[n] < bass_cut_off ) {
				lcf[n] = fre[n] * (audio.FFTbassbufferSize /2) + 1;
				bass_cut_off_bar++;
				treble_cut_off_bar++;
				k[n] *= log2(audio.FFTbassbufferSize);
			} else if (fc[n] > bass_cut_off && fc[n] < treble_cut_off) {
				lcf[n] = fre[n] * (audio.FFTmidbufferSize /2) + 1;
				treble_cut_off_bar++;
				if ((treble_cut_off_bar - bass_cut_off_bar) == 1) {
					first_bar = true;
					hcf[n - 1] = fre[n] * (audio.FFTbassbufferSize /2);
					if (hcf[n - 1] < lcf[n - 1]) hcf[n - 1] = lcf[n - 1];
				} else {
					first_bar = false;
				}

				k[n] *= log2(audio.FFTmidbufferSize);
			} else {
				lcf[n] = fre[n] * (audio.FFTtreblebufferSize /2) + 1;
				first_treble_bar++;
				if (first_treble_bar == 1) {
					first_bar = true;
					hcf[n - 1] = fre[n] * (audio.FFTmidbufferSize /2);
					if (hcf[n - 1] < lcf[n - 1]) hcf[n - 1] = lcf[n - 1];
				} else {
					first_bar = false;
				}

				k[n] *= log2(audio.FFTtreblebufferSize);
			}

			if (n != 0 && !first_bar) {
				hcf[n - 1] = lcf[n] - 1;

				//pushing the spectrum up if the expe function gets "clumped"
				if (lcf[n] <= lcf[n - 1])lcf[n] = lcf[n - 1] + 1;
				hcf[n - 1] = lcf[n] - 1;
			}

#ifdef DEBUG
			if (n != 0) {
				mvprintw(n,0,"%d: %f -> %f (%d -> %d) bass: %d, treble:%d \n", n,
						fc[n - 1], fc[n], lcf[n - 1],
						hcf[n - 1], bass_cut_off_bar, treble_cut_off_bar);
			}
#endif
		}

		// process: weigh signal to frequencies height and EQ
		for (n = 0; n < bars; n++) {
			}

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
			silence = true;
			for (i = 0; i < (2 * (audio.FFTbassbufferSize / 2 + 1)); i++) {
				if (i < audio.FFTbassbufferSize) {
					in_bass_l[i] = audio.audio_out_bass_l[i];
					if (p.stereo) in_bass_r[i] = audio.audio_out_bass_r[i];
					if (in_bass_l[i] || in_bass_r[i]) silence = false;
				} else {
					in_bass_l[i] = 0;
					if (p.stereo) in_bass_r[i] = 0;
				}
			}

			for (i = 0; i < (2 * (audio.FFTmidbufferSize / 2 + 1)); i++) {
				if (i < audio.FFTmidbufferSize) {
					in_mid_l[i] = audio.audio_out_mid_l[i];
					if (p.stereo) in_mid_r[i] = audio.audio_out_mid_r[i];
				} else {
					in_mid_l[i] = 0;
					if (p.stereo) in_mid_r[i] = 0;
				}
			}

			for (i = 0; i < (2 * (audio.FFTtreblebufferSize / 2 + 1)); i++) {
				if (i < audio.FFTtreblebufferSize) {
					in_treble_l[i] = audio.audio_out_treble_l[i];
					if (p.stereo) in_treble_r[i] = audio.audio_out_treble_r[i];
				} else {
					in_treble_l[i] = 0;
					if (p.stereo) in_treble_r[i] = 0;
				}
			}
			if (silence) sleep++;
			else sleep = 0;

			// process: if input was present for the last 5 seconds apply FFT to it
			if (sleep < p.framerate * 5) {

				// process: execute FFT and sort frequency bands
				if (p.stereo) {
					fftw_execute(p_bass_l);
					fftw_execute(p_bass_r);
					fftw_execute(p_mid_l);
					fftw_execute(p_mid_r);
					fftw_execute(p_treble_l);
					fftw_execute(p_treble_r);

					fl = separate_freq_bands(audio.FFTbassbufferSize, out_bass_l,
							audio.FFTmidbufferSize, out_mid_l,
							audio.FFTtreblebufferSize, out_treble_l,
							bass_cut_off_bar, treble_cut_off_bar,
							bars/2,	lcf, hcf, k, 1, p.sens, p.ignore);

					fr = separate_freq_bands(audio.FFTbassbufferSize, out_bass_r,
							audio.FFTmidbufferSize, out_mid_r,
							audio.FFTtreblebufferSize, out_treble_r,
							bass_cut_off_bar, treble_cut_off_bar,
							bars/2,	lcf, hcf, k, 1, p.sens, p.ignore);

				} else {
					fftw_execute(p_bass_l);
					fftw_execute(p_mid_l);
					fftw_execute(p_treble_l);
					fl = separate_freq_bands(audio.FFTbassbufferSize, out_bass_l,
							audio.FFTmidbufferSize, out_mid_l, 
							audio.FFTtreblebufferSize, out_treble_l, 
							bass_cut_off_bar, treble_cut_off_bar, 
							bars, lcf, hcf, k, 1, p.sens, p.ignore);
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

			// process [filter]

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


			// processing signal
		
			bool senselow = true;

			for (o = 0; o < bars; o++) {
				//mirroring stereo channels
				if (p.stereo) {
					if (o < bars / 2) {
						f[o] = fl[bars / 2 - o - 1];
					} else {
						f[o] = fr[o - bars / 2];
					}

				} else {
					f[o] = fl[o];
				}

				// process [smoothing]: falloff
				if (g > 0) {
					if (f[o] < flast[o]) {
						f[o] = fpeak[o] - (g * fall[o] * fall[o]);
						fall[o]++;
					} else  {
						fpeak[o] = f[o];
						fall[o] = 0;
					}

					flast[o] = f[o];
				}

				// process [smoothing]: integral
				if (p.integral > 0) {
					f[o] = fmem[o] * p.integral + f[o];
					fmem[o] = f[o];

					int diff = (height + 1) - f[o];
					if (diff < 0) diff = 0;
					double div = 1 / (diff + 1);
					//f[o] = f[o] - pow(div, 10) * (height + 1);
					fmem[o] = fmem[o] * (1 - div / 20);

					#ifdef DEBUG
						mvprintw(o,0,"%d: f:%f->%f (%d->%d), k-value:\
						%15e, peak:%d \n", o, fc[o], fc[o + 1], 
						lcf[o], hcf[o], k[o], f[o]);
						//if(f[o] > maxvalue) maxvalue = f[o];
					#endif
				}

				// zero values causes divided by zero segfault (if not raw)
				if (f[o] < 1) {
					f[o] = 1;
					if (p.om == 4) f[o] = 0;
				}

				//autmatic sens adjustment
				if (p.autosens) {
					if (f[o] > height && senselow) {
						p.sens = p.sens * 0.98;
						senselow = false;
					}
				}	
			}

			if (p.autosens && !silence && senselow) p.sens = p.sens * 1.001;


			#ifdef DEBUG
				//printf("%d\n",maxvalue); //checking maxvalue 10000
			#endif

			
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

	free(in_bass_r);
	free(in_bass_l);
	fftw_free(out_bass_r);
	fftw_free(out_bass_l);
	fftw_destroy_plan(p_bass_l);
	fftw_destroy_plan(p_bass_r);

	free(in_mid_r);
	free(in_mid_l);
	fftw_free(out_mid_r);
	fftw_free(out_mid_l);
	fftw_destroy_plan(p_mid_l);
	fftw_destroy_plan(p_mid_r);

	free(in_treble_r);
	free(in_treble_l);
	fftw_free(out_treble_r);
	fftw_free(out_treble_l);
	fftw_destroy_plan(p_treble_l);
	fftw_destroy_plan(p_treble_r);

    	cleanup();

	//fclose(fp);
	}
}
