#define _XOPEN_SOURCE_EXTENDED
#include <alloca.h>
#include <locale.h>
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

#ifdef XLIB
#include "output/graphical.h"
#include "output/graphical_x.c"
#include "output/graphical_x.h"
#endif

#ifdef SDL
#include "output/graphical.h"
#include "output/graphical_sdl.c"
#include "output/graphical_sdl.h"
#endif

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

dictionary *ini;
char *inputMethod, *outputMethod, *modeString, *color, *bcolor, *style, *raw_target, *data_format;
// *bar_delim, *frame_delim ;
char *gradient_color_1;
char *gradient_color_2;
double monstercat, integral, gravity, ignore, smh, sens;
int fixedbars, framerate, bw, bs, set_win_props, autosens, overshoot;
unsigned int lowcf, highcf;
double smoothDef[64] = {0.8, 0.8, 1, 1, 0.8, 0.8, 1, 0.8, 0.8, 1, 1, 0.8,
					1, 1, 0.8, 0.6, 0.6, 0.7, 0.8, 0.8, 0.8, 0.8, 0.8,
					0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
					0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
					0.7, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6};
double *smooth = smoothDef;
int smcount = 64;
int customEQ = 0;
struct audio_data audio;
int im = 0;
int om = 1;
int mode = 1;
int col = 6;
int bgcol = -1;
int bars = 25;
int autobars = 1;
int stereo = -1;
int M = 2048;
int is_bin = 1;
char bar_delim = ';';
char frame_delim = '\n';
int ascii_range = 1000;
int bit_format = 16;
int gradient = 0;
dictionary* ini;
char supportedInput[255] = "'fifo'";
int sourceIsAuto = 1;



// whether we should reload the config or not
int should_reload = 0;

// general: cleanup
void cleanup(void)
{
	if(om != 5 && om != 6)
	{
		#ifdef NCURSES
		cleanup_terminal_ncurses();
		#endif
		cleanup_terminal_noncurses();
	}
	else if (om == 5)
	{
		#ifdef XLIB
		cleanup_graphical_x();
		#endif
	}
	else if (om == 6)
	{
		#ifdef SDL
		cleanup_graphical_sdl();
		#endif
	}
}

// general: handle signals
void sig_handler(int sig_no)
{
	if (sig_no == SIGUSR1) {
		should_reload = 1;
		return;
	}

	cleanup();
	if (sig_no == SIGINT) {
		printf("CTRL-C pressed -- goodbye\n");
	}
	signal(sig_no, SIG_DFL);
	raise(sig_no);
}

void load_config(char configPath[255])
{

	FILE *fp;
	
	//config: creating path to default config file
	if (configPath[0] == '\0') {
		char *configFile = "config";
		char *configHome = getenv("XDG_CONFIG_HOME");
		if (configHome != NULL) {
			sprintf(configPath,"%s/%s/", configHome, PACKAGE);
		} else {
			configHome = getenv("HOME");
			if (configHome != NULL) {
				sprintf(configPath,"%s/%s/%s/", configHome, ".config", PACKAGE);
			} else {
				printf("No HOME found (ERR_HOMELESS), exiting...");
				exit(EXIT_FAILURE);
			}
		}
	
		// config: create directory
		mkdir(configPath, 0777);

		// config: adding default filename file
		strcat(configPath, configFile);
		
		fp = fopen(configPath, "ab+");
		if (fp) {
			fclose(fp);
		} else {
			printf("Unable to access config '%s', exiting...\n", configPath);
			exit(EXIT_FAILURE);
		}


	} else { //opening specified file

		fp = fopen(configPath, "rb+");	
		if (fp) {
			fclose(fp);
		} else {
			printf("Unable to open file '%s', exiting...\n", configPath);
			exit(EXIT_FAILURE);
		}
	}

	// config: parse ini
	ini = iniparser_load(configPath);

	//setting fifo to defaualt if no other input modes supported
	inputMethod = (char *)iniparser_getstring(ini, "input:method", "fifo"); 

	//setting alsa to defaualt if supported
	#ifdef ALSA
		inputMethod = (char *)iniparser_getstring(ini, "input:method", "alsa"); 
	#endif

	//setting pulse to defaualt if supported
	#ifdef PULSE
		inputMethod = (char *)iniparser_getstring(ini, "input:method", "pulse");
	#endif


	#ifdef NCURSES
		outputMethod = (char *)iniparser_getstring(ini, "output:method", "ncurses");
	#endif
	#ifndef NCURSES
		outputMethod = (char *)iniparser_getstring(ini, "output:method", "noncurses");
	#endif
	modeString = (char *)iniparser_getstring(ini, "general:mode", "normal");

	monstercat = 1.5 * iniparser_getdouble(ini, "smoothing:monstercat", 1);
	integral = iniparser_getdouble(ini, "smoothing:integral", 0.7);
	gravity = iniparser_getdouble(ini, "smoothing:gravity", 1);
	ignore = iniparser_getdouble(ini, "smoothing:ignore", 0);

	color = (char *)iniparser_getstring(ini, "color:foreground", "default");
	bcolor = (char *)iniparser_getstring(ini, "color:background", "default");

   	gradient = iniparser_getint(ini, "color:gradient", 0);
    if (gradient) {
        gradient_color_1 = (char *)iniparser_getstring(ini, "color:gradient_color_1", "#0099ff");
        gradient_color_2 = (char *)iniparser_getstring(ini, "color:gradient_color_2", "#ff3399");
    }

	fixedbars = iniparser_getint(ini, "general:bars", 0);
	bw = iniparser_getint(ini, "general:bar_width", 2);
	bs = iniparser_getint(ini, "general:bar_spacing", 1);
	framerate = iniparser_getint(ini, "general:framerate", 60);
	sens = iniparser_getint(ini, "general:sensitivity", 100);
	autosens = iniparser_getint(ini, "general:autosens", 1);
	overshoot = iniparser_getint(ini, "general:overshoot", 20);
	lowcf = iniparser_getint(ini, "general:lower_cutoff_freq", 50);
	highcf = iniparser_getint(ini, "general:higher_cutoff_freq", 10000);

    // config: window
	w = iniparser_getint(ini, "window:width", 640);
	h = iniparser_getint(ini, "window:height", 480);
	windowAlignment = (char *)iniparser_getstring(ini, "window:alignment", "none");
	windowX = iniparser_getint(ini, "window:x_padding", 0);
	windowY = iniparser_getint(ini, "window:y_padding", 0);
	fs = iniparser_getboolean(ini, "window:fullscreen", FALSE);
	transparentFlag = iniparser_getboolean(ini, "window:transparency", FALSE);
	borderFlag = iniparser_getboolean(ini, "window:border", TRUE);
	keepInBottom = iniparser_getboolean(ini, "window:keep_below", FALSE);

    // config: output
	style =  (char *)iniparser_getstring(ini, "output:style", "stereo");
	raw_target = (char *)iniparser_getstring(ini, "output:raw_target", "/dev/stdout");
	data_format = (char *)iniparser_getstring(ini, "output:data_format", "binary");
	bar_delim = (char)iniparser_getint(ini, "output:bar_delimiter", 59);
	frame_delim = (char)iniparser_getint(ini, "output:frame_delimiter", 10);
	ascii_range = iniparser_getint(ini, "output:ascii_max_range", 1000);
	bit_format = iniparser_getint(ini, "output:bit_format", 16);

	// read & validate: eq

	smcount = iniparser_getsecnkeys(ini, "eq");
	if (smcount > 0) {
        customEQ = 1;
		smooth = malloc(smcount*sizeof(*smooth));
		#ifndef LEGACYINIPARSER
		const char *keys[smcount];
		iniparser_getseckeys(ini, "eq", keys);
		#endif
		#ifdef LEGACYINIPARSER
		char **keys = iniparser_getseckeys(ini, "eq");
		#endif
		for (int sk = 0; sk < smcount; sk++) {
			smooth[sk] = iniparser_getdouble(ini, keys[sk], 1);
		}
	} else {
        customEQ = 0;
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
	if (strcmp(inputMethod, "pulse") == 0) {
		im = 3;
		audio.source = (char *)iniparser_getstring(ini, "input:source", "auto");
	}
}

int validate_color(char *checkColor, int om)
{
	int validColor = 0;
	if (checkColor[0] == '#' && strlen(checkColor) == 7) {
		// If the output mode is not ncurses, tell the user to use a named colour instead of hex colours.
		if (om != 1 && om != 2 && om != 5 && om != 6) {
			fprintf(stderr, "Only 'ncurses', 'sdl' and 'x' output method supports HTML colors. Please change the colours or the output method.\n");
			exit(EXIT_FAILURE);
		}
		// 0 to 9 and a to f
		for (int i = 1; checkColor[i]; ++i) {
			if (!isdigit(checkColor[i])) {
				if (tolower(checkColor[i]) >= 'a' && tolower(checkColor[i]) <= 'f') {
					validColor = 1;
				} else {
					validColor = 0;
					break;
				}
			} else {
				validColor = 1;
			}
		}
	} else {
		if ((strcmp(checkColor, "black") == 0) || \
			(strcmp(checkColor, "red") == 0) || \
			(strcmp(checkColor, "green") == 0) || \
			(strcmp(checkColor, "yellow") == 0) || \
			(strcmp(checkColor, "blue") == 0) || \
			(strcmp(checkColor, "magenta") == 0) || \
			(strcmp(checkColor, "cyan") == 0) || \
			(strcmp(checkColor, "white") == 0) || \
			(strcmp(checkColor, "default") == 0)) validColor = 1;
	}
	return validColor;
}

void validate_config()
{


	// validate: input method
    im = 0;
	if (strcmp(inputMethod, "alsa") == 0) {
		im = 1;
		#ifndef ALSA
		        fprintf(stderr,
                                "cava was built without alsa support, install alsa dev files and run make clean && ./configure && make again\n");
                        exit(EXIT_FAILURE);
                #endif
	}
	if (strcmp(inputMethod, "fifo") == 0) {
		im = 2;
	}
	if (strcmp(inputMethod, "pulse") == 0) {
		im = 3;
		#ifndef PULSE
		        fprintf(stderr,
                                "cava was built without pulseaudio support, install pulseaudio dev files and run make clean && ./configure && make again\n");
                        exit(EXIT_FAILURE);
                #endif

	}
	if (im == 0) {
		fprintf(stderr,
			"input method '%s' is not supported, supported methods are: %s\n",
						inputMethod, supportedInput);
		exit(EXIT_FAILURE);
	}

	// validate: output method
    om = 0;
	if (strcmp(outputMethod, "ncurses") == 0) {
		om = 1;
		#ifndef NCURSES
			fprintf(stderr,
				"cava was built without ncurses support, install ncursesw dev files and run make clean && ./configure && make again\n");
			exit(EXIT_FAILURE);
		#endif
	}
	if (strcmp(outputMethod, "circle") == 0) {
		 om = 2;
		#ifndef NCURSES
			fprintf(stderr,
				"cava was built without ncurses support, install ncursesw dev files and run make clean && ./configure && make again\n");
			exit(EXIT_FAILURE);
		#endif
	}
	if (strcmp(outputMethod, "noncurses") == 0) {
		om = 3;
		bgcol = 0;
	}
	if (strcmp(outputMethod, "raw") == 0) {//raw:
		om = 4;
		autosens = 0;
		
		//checking data format
		if (strcmp(data_format, "binary") == 0) {
			is_bin = 1;
			//checking bit format:
			if (bit_format != 8 && bit_format != 16 ) {
			fprintf(stderr,
				"bit format  %d is not supported, supported data formats are: '8' and '16'\n",
							bit_format );
			exit(EXIT_FAILURE);
		
			}
		} else if (strcmp(data_format, "ascii") == 0) {
			is_bin = 0;
			if (ascii_range < 1 ) {
			fprintf(stderr,
				"ascii max value must be a positive integer\n");
			exit(EXIT_FAILURE);
			}
		} else {
		fprintf(stderr,
			"data format %s is not supported, supported data formats are: 'binary' and 'ascii'\n",
						data_format);
		exit(EXIT_FAILURE);
		
		}
	}
	if(strcmp(outputMethod, "x") == 0)
	{
		om = 5;
		#ifndef XLIB
			fprintf(stderr,
				"cava was built without Xlib support, install Xlib dev files and run make clean && ./configure && make again\n");
			exit(EXIT_FAILURE);
		#endif
	}
	if(strcmp(outputMethod, "sdl") == 0)
	{
		om = 6;
		#ifndef SDL
			fprintf(stderr,
				"cava was build without SDL2 support, install SDL2 dev files and run make clean && ./configure && make again\n");
			exit(EXIT_FAILURE);
		#endif
	}
	if (om == 0) {
		fprintf(stderr,
			"output method %s is not supported, supported methods are: 'noncurses'",
						outputMethod);
		#ifdef XLIB
			fprintf(stderr, ", 'x'");
		#endif
		#ifdef SDL
			fprintf(stderr, ", 'sdl'");
		#endif
		#ifdef NCURSES
			fprintf(stderr, ", 'ncurses'");
		#endif
		// Just a quick question, should you add 'circle' and 'raw' here?
		fprintf(stderr, "\n");

		exit(EXIT_FAILURE);	
	}

	// validate: output style
	if (strcmp(style, "mono") == 0) stereo = 0;
	if (strcmp(style, "stereo") == 0) stereo = 1;
	if (stereo == -1) {
		fprintf(stderr,
			"output style %s is not supported, supported styles are: 'mono' and 'stereo'\n",
						style);
		exit(EXIT_FAILURE);
	}




	// validate: bars
	if (fixedbars > 0) autobars = 0;
	if (fixedbars > 200) fixedbars = 200;
	if (bw > 200) bw = 200;
	if (bw < 1) bw = 1;

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
	if (!validate_color(color, om)) {
		fprintf(stderr, "The value for 'foreground' is invalid. It can be either one of the 7 named colors or a HTML color of the form '#xxxxxx'.\n");
		exit(EXIT_FAILURE);
	}

	// validate: background color
	if (!validate_color(bcolor, om)) {
		fprintf(stderr, "The value for 'background' is invalid. It can be either one of the 7 named colors or a HTML color of the form '#xxxxxx'.\n");
		exit(EXIT_FAILURE);
	}

	// In case color is not html format set bgcol and col to predefinedint values
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
	
	//setting sens
	sens = sens / 100;

	// validate: window settings
	if(om == 5 || om == 6)
	{
		// validate: alignment
		if(strcmp(windowAlignment, "top_left"))
		if(strcmp(windowAlignment, "top_right"))
		if(strcmp(windowAlignment, "bottom_left"))
		if(strcmp(windowAlignment, "bottom_right"))
		if(strcmp(windowAlignment, "left"))
		if(strcmp(windowAlignment, "right"))
		if(strcmp(windowAlignment, "top"))
		if(strcmp(windowAlignment, "bottom"))
		if(strcmp(windowAlignment, "center"))
		if(strcmp(windowAlignment, "none"))
			fprintf(stderr, "The value for alignment is invalid, '%s'!", windowAlignment);

		// Get bar settings
		bw = iniparser_getint(ini, "window:bar_width", 20);
		bs = iniparser_getint(ini, "window:bar_spacing", 4);
		
		// Set window properties
		set_win_props = iniparser_getint(ini, "window:set_win_props", 0);
	}
}

#ifdef ALSA
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

#endif

int * separate_freq_bands(fftw_complex out[M / 2 + 1][2], int bars, int lcf[200], int hcf[200], float k[200], int channel) {
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

			y[i] =  pow(pow(*out[i][0], 2) + pow(*out[i][1], 2), 0.5); //getting r of compex
			peak[o] += y[i]; //adding upp band
			
		}
	
		
		peak[o] = peak[o] / (hcf[o]-lcf[o]+1); //getting average
		temp = peak[o] * k[o] * sens; //multiplying with k and adjusting to sens settings
		if (temp <= ignore)temp = 0;
		if (channel == 1) fl[o] = temp;
		else fr[o] = temp;

	}

	if (channel == 1) return fl;
 	else return fr;
} 


int * monstercat_filter (int * f, int bars) {

	int z;


	// process [smoothing]: monstercat-style "average"

	int m_y, de;
	if (mode == 3) {
		for (z = 0; z < bars; z++) { // waves
			f[z] = f[z] / 1.25;
			if (f[z] < 0.125)f[z] = 0.125;
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
			if (f[z] < 0.125)f[z] = 0.125;
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
	int modes = 3; // amount of smoothing modes
	float fc[200];
	float fre[200];
	int f[200], lcf[200], hcf[200];
	int *fl, *fr;
	int fmem[200];
	int flast[200];
	int flastd[200];
	int sleep = 0;
	int i, n, o, height, c, rest, inAtty, silence, fp, fptest;
	float temp;
	//int cont = 1;
	int fall[200];
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
as of 0.4.0 all options are specified in config file, see in '/home/username/.config/cava/' \n";

	char ch = '\0';
	double inr[2 * (M / 2 + 1)];
    double inl[2 * (M / 2 + 1)];
	
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

	//fft: planning to rock
	fftw_complex outl[M / 2 + 1][2];
	fftw_plan pl =  fftw_plan_dft_r2c_1d(M, inl, *outl, FFTW_MEASURE); 	

    fftw_complex outr[M / 2 + 1][2];
    fftw_plan pr =  fftw_plan_dft_r2c_1d(M, inr, *outr, FFTW_MEASURE);

	// general: main loop
	while (1) {

	//config: load & validate
	load_config(configPath);
    validate_config();	

	if ((om != 4) && (om != 5)) { 
		// Check if we're running in a Virtual console todo: replace virtual console with terminal emulator
		inAtty = 0;
		if (strncmp(ttyname(0), "/dev/tty", 8) == 0 || strcmp(ttyname(0), "/dev/console") == 0) inAtty = 1;
	
		if (inAtty) {
			system("setfont cava.psf  >/dev/null 2>&1");
			system("setterm -blank 0");
		}
	}


	//input: init
	audio.format = -1;
	audio.rate = 0;
	audio.terminate = 0;
	if (stereo) audio.channels = 2;
	if (!stereo) audio.channels = 1;

	for (i = 0; i < M; i++) {
		audio.audio_out_l[i] = 0;
		audio.audio_out_r[i] = 0;
	}

	#ifdef ALSA
	// input_alsa: wait for the input to be ready
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
		
		n = 0;
		
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
	#endif

	if (im == 2) {
		thr_id = pthread_create(&p_thread, NULL, input_fifo, (void*)&audio); //starting fifomusic listener
		audio.rate = 44100;
	}

	#ifdef PULSE
	if (im == 3) {
		if (strcmp(audio.source, "auto") == 0) {
			getPulseDefaultSink((void*)&audio);
			sourceIsAuto = 1;
			}
		else sourceIsAuto = 0;
		thr_id = pthread_create(&p_thread, NULL, input_pulse, (void*)&audio); //starting pulsemusic listener
		audio.rate = 44100;
	}
	#endif

	if (highcf > audio.rate / 2) {
		cleanup();
		fprintf(stderr,
			"higher cuttoff frequency can't be higher then sample rate / 2"
		);
			exit(EXIT_FAILURE);
	}
	 
	

	bool reloadConf = FALSE;
	bool senseLow = TRUE;

	// open XLIB window and set everything up
	#ifdef XLIB
	if(om == 5) if(init_window_x(color, bcolor, col, bgcol, set_win_props, argv, argc, gradient, gradient_color_1, gradient_color_2)) exit(EXIT_FAILURE);
	#endif

	// setting up sdl
	#ifdef SDL
	if(om == 6) if(init_window_sdl(&col, &bgcol, color, bcolor, gradient, gradient_color_1, gradient_color_2)) exit(EXIT_FAILURE);
	#endif

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
		if (om == 1 || om ==  2) {
			init_terminal_ncurses(color, bcolor, col, bgcol, gradient, gradient_color_1, gradient_color_2,&w, &h);
			//get_terminal_dim_ncurses(&w, &h);
		}
		#endif

		if (om == 3) get_terminal_dim_noncurses(&w, &h);

		// output open file/fifo for raw output
		if ( om == 4) {

			if (strcmp(raw_target,"/dev/stdout") != 0) {

				//checking if file exists
				if( access( raw_target, F_OK ) != -1 ) {
					fptest = open(raw_target, O_RDONLY | O_NONBLOCK, 0644);	//testopening in case it's a fifo
					if (fptest == -1) {
						printf("could not open file %s for writing\n",raw_target);
						exit(1);
					}
				} else {
					printf("creating fifo %s\n",raw_target);
					if (mkfifo(raw_target, 0664) == -1) {
						printf("could not create fifo %s\n",raw_target);
						exit(1);
					}
					fptest = open(raw_target, O_RDONLY | O_NONBLOCK, 0644); //fifo needs to be open for reading in order to write to it
				}
		    }

			fp = open(raw_target, O_WRONLY | O_NONBLOCK | O_CREAT, 0644);
			if (fp == -1) {
				printf("could not open file %s for writing\n",raw_target);
				exit(1);
			}
			printf("open file %s for writing raw ouput\n",raw_target);		
	
            //height and with must be hardcoded for raw output.
			h = 112;
			w = 200;	
		}

		// draw X11 background
		#ifdef XLIB
		if(om == 5) apply_window_settings_x();
		#endif
		#ifdef SDL
		if(om == 6) apply_window_settings_sdl(bgcol);
		#endif

 		//handle for user setting too many bars
		if (fixedbars) {
			autobars = 0;
			if (fixedbars * bw + fixedbars * bs - bs > w) autobars = 1;
		}

		//getting orignial numbers of barss incase of resize
		if (autobars == 1)  {
			bars = (w + bs) / (bw + bs);
			//if (bs != 0) bars = (w - bars * bs + bs) / bw;
		} else bars = fixedbars;


		if (bars < 1) bars = 1; // must have at least 1 bars
        if (bars > 200) bars = 200; // cant have more than 200 bars

		if (stereo) { //stereo must have even numbers of bars
			if (bars%2 != 0) bars--;
		}

		

		height = h - 1;

		// process [smoothing]: calculate gravity
		g = gravity * ((float)height / 270) * pow((60 / (float)framerate), 2.5);


		//checks if there is stil extra room, will use this to center
		rest = (w - bars * bw - bars * bs + bs) / 2;
		if (rest < 0)rest = 0;

		#ifdef DEBUG
			printw("height: %d width: %d bars:%d bar width: %d rest: %d\n",
						 w,
						 h, bars, bw, rest);
		#endif

		//output: start noncurses mode
		if (om == 3) init_terminal_noncurses(col, bgcol, w, h, bw);

		

		if (stereo) bars = bars / 2; // in stereo onle half number of bars per channel

		if ((smcount > 0) && (bars > 0)) {
			smh = (double)(((double)smcount)/((double)bars));
		}


		double freqconst = log10((float)lowcf / (float)highcf) /  ((float)1 / ((float)bars + (float)1) - 1);
	
		//freqconst = -2;

		// process: calculate cutoff frequencies
		for (n = 0; n < bars + 1; n++) {
			fc[n] = highcf * pow(10, freqconst * (-1) + ((((float)n + 1) / ((float)bars + 1)) * freqconst)); //decided to cut it at 10k, little interesting to hear above
			fre[n] = fc[n] / (audio.rate / 2); //remember nyquist!, pr my calculations this should be rate/2 and  nyquist freq in M/2 but testing shows it is not... or maybe the nq freq is in M/4
			lcf[n] = fre[n] * (M /4); //lfc stores the lower cut frequency foo each bar in the fft out buffer
			if (n != 0) {
				hcf[n - 1] = lcf[n] - 1;
				if (lcf[n] <= lcf[n - 1])lcf[n] = lcf[n - 1] + 1; //pushing the spectrum up if the expe function gets "clumped"
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
		for (n = 0; n < bars;
			n++)k[n] = pow(fc[n],0.85) * ((float)height/(M*4000)) * smooth[(int)floor(((double)n) * smh)];
	
		if (stereo) bars = bars * 2; 	

	   	bool resizeTerminal = FALSE;

		while  (!resizeTerminal) {

			// general: keyboard controls
			if(om != 5 || om != 6){
				#ifdef NCURSES
				if (om == 1 || om == 2) ch = getch();
				#endif

				switch (ch) {
					case 's':
						if(bs > 0) bs--;
						resizeTerminal = TRUE;
						break;
					case 'a':
						bs++;
						resizeTerminal = TRUE;
						break;
					case 65:    // key up
						sens = sens * 1.05;
						break;
					case 66:    // key down
						sens = sens * 0.95;
						break;
					case 68:    // key right
						bw++;
						resizeTerminal = TRUE;
						break;
					case 67:    // key left
						if (bw > 1) bw--;
						resizeTerminal = TRUE;
						break;
					case 'm':
						if (mode == modes) {
							mode = 1;
						} else {
							mode++;
						}
						break;
					case 'r': //reload config
						should_reload = 1;
						break;
					case 'c': //change forground color
						if (col < 7) col++;
						else col = 0;
						resizeTerminal = TRUE;
						break;
					case 'b': //change backround color
						if (bgcol < 7) bgcol++;
						else bgcol = 0;
						resizeTerminal = TRUE;
						break;

					case 'q':
						cleanup();
						return EXIT_SUCCESS;
				}
			}
			#ifdef XLIB
			if(om == 5)
			{
				switch(get_window_input_x(&should_reload, &bs, &sens, &bw, &modes, &mode, &w, &h, color, bcolor, gradient))
				{
					case -1:
						cleanup();
						return EXIT_SUCCESS;
					case 1:
						cleanup();
						break;
					case 2:
						resizeTerminal = TRUE;
						break;
				}
			}
			#endif
			#ifdef SDL
			if(om == 6) 
			{
				switch(get_window_input_sdl(&bs, &bw, &sens, &mode, modes, &col, &bgcol, &w, &h, gradient))
				{
					case -1:
						cleanup();
						exit(EXIT_FAILURE);
						break;
					case 1:
						should_reload = 1;
						cleanup();
						break;
					case 2:
						resizeTerminal = 1;
						break;
				}
			}
			#endif

			if (should_reload) {

				reloadConf = TRUE;
				resizeTerminal = TRUE;
				should_reload = 0;
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
					if (stereo) inr[i] = audio.audio_out_r[i];
					if (inl[i] || inr[i]) silence = 0;
				} else {
					inl[i] = 0;
					if (stereo) inr[i] = 0;
				}
			}

			if (silence == 1)sleep++;
			else sleep = 0;

			// process: if input was present for the last 5 seconds apply FFT to it
			if (sleep < framerate * 5) {

				// process: execute FFT and sort frequency bands
				if (stereo) {
					fftw_execute(pl);
					fftw_execute(pr);

					fl = separate_freq_bands(outl,bars/2,lcf,hcf, k, 1);
					fr = separate_freq_bands(outr,bars/2,lcf,hcf, k, 2);
				} else {
					fftw_execute(pl);
					fl = separate_freq_bands(outl,bars,lcf,hcf, k, 1);
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
			if (mode != 2)
			{
				if (monstercat) {
					if (stereo) {
						fl = monstercat_filter(fl, bars / 2);
						fr = monstercat_filter(fr, bars / 2);	
					} else {
						fl = monstercat_filter(fl, bars);
					}
				
				}


				//preperaing signal for drawing
				for (o = 0; o < bars; o++) {
					if (stereo) {
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
					for (o = 0; o < bars; o++) {
						fmem[o] = fmem[o] * integral + f[o];
						f[o] = fmem[o];

						#ifdef DEBUG
							mvprintw(o,0,"%d: f:%f->%f (%d->%d)peak:%d \n", o, fc[o], fc[o + 1],
										 lcf[o], hcf[o], f[o]);
						#endif
					}
				}

			}

			// zero values causes divided by zero segfault
			for (o = 0; o < bars; o++) {
				if (f[o] < 1) {
					f[o] = 1;
					if (om == 4) f[o] = 0;
				}
				//if(f[o] > maxvalue) maxvalue = f[o]; 
			}

			//printf("%d\n",maxvalue); //checking maxvalue I keep forgetting its about 10000

			//autmatic sens adjustment
			if (autosens && om != 4) {
				for (o = 0; o < bars; o++) {
					// Sens for graphical modes
					if((om == 5) | (om == 6)){
						if (f[o] > height + height * overshoot / 100) {
							senseLow = FALSE;
							sens = sens * 0.99;
							break;
						}
						if (senseLow && !silence) sens = sens * 1.01;
					}
					else
					{
						// Sens for console modes
						if (f[o] > height * 8 + height * 8 * overshoot / 100) {
							senseLow = FALSE;
							sens = sens * 0.99;
							break;
						}
						if (senseLow && !silence) sens = sens * 1.01;
					}
				}
			}

			// output: draw processed input
			#ifndef DEBUG
				switch (om) {
					case 1:
						#ifdef NCURSES
						rc = draw_terminal_ncurses(inAtty, h, w, bars, bw, bs, rest, f, flastd, gradient);
						break;
						#endif
					case 2:
						#ifdef NCURSES
						rc = draw_terminal_bcircle(inAtty, h, w, f);
						break;
						#endif
					case 3:
						rc = draw_terminal_noncurses(inAtty, h, w, bars, bw, bs, rest, f, flastd);
						break;
					case 4:
						rc = print_raw_out(bars, fp, is_bin, bit_format, ascii_range, bar_delim, frame_delim,f);
						break;
					case 5:
					{
						#ifdef XLIB
						// this prevents invalid access
						if(reloadConf)
							break;
						
						draw_graphical_x(h, bars, bw, bs, rest, gradient, f, flastd);
						break;
						#endif
					}
					case 6:
					{
						#ifdef SDL
						if(!resizeTerminal) draw_graphical_sdl(bars, rest, bw, bs, f, flastd, col, bgcol, gradient);
						break;
						#endif
					}
				}

				if (rc == -1) resizeTerminal = TRUE; //terminal has been resized breaking to recalibrating values

				if (framerate <= 1) {
					req.tv_sec = 1  / (float)framerate;
				} else {
					req.tv_sec = 0;
					req.tv_nsec = (1 / (float)framerate) * 1000000000; //sleeping for set us
				}

				nanosleep (&req, NULL);
			#endif
		
			for (o = 0; o < bars; o++) {	
				flastd[o] = f[o];
			} 
		}
	}//reloading config
	req.tv_sec = 0; 
	req.tv_nsec = 100; //waiting some time to make shure audio is ready
	nanosleep (&req, NULL);

	//**telling audio thread to terminate**//
	audio.terminate = 1;
	pthread_join( p_thread, NULL);

	if (customEQ) free(smooth);
	if (sourceIsAuto) free(audio.source);
	iniparser_freedict(ini);


	//fclose(fp);
	}
}
