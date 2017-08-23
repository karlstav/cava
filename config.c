
double smoothDef[64] = {0.8, 0.8, 1, 1, 0.8, 0.8, 1, 0.8, 0.8, 1, 1, 0.8,
					1, 1, 0.8, 0.6, 0.6, 0.7, 0.8, 0.8, 0.8, 0.8, 0.8,
					0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
					0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8, 0.8,
					0.7, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6, 0.6};


char *inputMethod, *outputMethod, *channels, *data_format;


struct config_params {

char *color, *bcolor, *raw_target, *audio_source, *gradient_color_1, *gradient_color_2;
char bar_delim, frame_delim ;
double monstercat, integral, gravity, ignore, sens; 
unsigned int lowcf, highcf;
double *smooth;
int smcount, customEQ, im, om, col, bgcol, autobars, stereo, is_bin, ascii_range,
 bit_format, gradient, fixedbars, framerate, bw, bs, autosens, overshoot, waves;

};




int validate_color(char *checkColor, int om)
{
int validColor = 0;
if (checkColor[0] == '#' && strlen(checkColor) == 7) {
	// If the output mode is not ncurses, tell the user to use a named colour instead of hex colours.
	if (om != 1 && om != 2) {
		fprintf(stderr, "Only 'ncurses' output method supports HTML colors. Please change the colours or the output method.\n");
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

void validate_config(char supportedInput[255], void* params)
{

struct config_params *p = (struct config_params *)params;

// validate: input method
p->im = 0;
if (strcmp(inputMethod, "alsa") == 0) {
	p->im = 1;
	#ifndef ALSA
	        fprintf(stderr,
                        "cava was built without alsa support, install alsa dev files and run make clean && ./configure && make again\n");
                exit(EXIT_FAILURE);
        #endif
}
if (strcmp(inputMethod, "fifo") == 0) {
	p->im = 2;
}
if (strcmp(inputMethod, "pulse") == 0) {
	p->im = 3;
	#ifndef PULSE
	        fprintf(stderr,
                        "cava was built without pulseaudio support, install pulseaudio dev files and run make clean && ./configure && make again\n");
                exit(EXIT_FAILURE);
        #endif

}
if (p->im == 0) {
	fprintf(stderr,
		"input method '%s' is not supported, supported methods are: %s\n",
					inputMethod, supportedInput);
	exit(EXIT_FAILURE);
}

// validate: output method
p->om = 0;
if (strcmp(outputMethod, "ncurses") == 0) {
	p->om = 1;
	#ifndef NCURSES
		fprintf(stderr,
			"cava was built without ncurses support, install ncursesw dev files and run make clean && ./configure && make again\n");
		exit(EXIT_FAILURE);
	#endif
}
if (strcmp(outputMethod, "circle") == 0) {
	 p->om = 2;
	#ifndef NCURSES
		fprintf(stderr,
			"cava was built without ncurses support, install ncursesw dev files and run make clean && ./configure && make again\n");
		exit(EXIT_FAILURE);
	#endif
}
if (strcmp(outputMethod, "noncurses") == 0) {
	p->om = 3;
	p->bgcol = 0;
}
if (strcmp(outputMethod, "raw") == 0) {//raw:
	p->om = 4;
	p->autosens = 0;
	
	//checking data format
	p->is_bin = -1;
	if (strcmp(data_format, "binary") == 0) {
		p->is_bin = 1;
		//checking bit format:
		if (p->bit_format != 8 && p->bit_format != 16 ) {
		fprintf(stderr,
			"bit format  %d is not supported, supported data formats are: '8' and '16'\n",
						p->bit_format );
		exit(EXIT_FAILURE);
	
		}
	} else if (strcmp(data_format, "ascii") == 0) {
		p->is_bin = 0;
		if (p->ascii_range < 1 ) {
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
if (p->om == 0) {
	#ifndef NCURSES
	fprintf(stderr,
		"output method %s is not supported, supported methods are: 'noncurses'\n",
					outputMethod);
	exit(EXIT_FAILURE);
	#endif

	#ifdef NCURSES
        fprintf(stderr,
                "output method %s is not supported, supported methods are: 'ncurses' and 'noncurses'\n",
                                        outputMethod);
        exit(EXIT_FAILURE);
	#endif	
}

// validate: output channels
p->stereo = -1;
if (strcmp(channels, "mono") == 0) p->stereo = 0;
if (strcmp(channels, "stereo") == 0) p->stereo = 1;
if (p->stereo == -1) {
	fprintf(stderr,
		"output channels %s is not supported, supported channelss are: 'mono' and 'stereo'\n",
					channels);
	exit(EXIT_FAILURE);
}




// validate: bars
p->autobars = 1;
if (p->fixedbars > 0) p->autobars = 0;
if (p->fixedbars > 200) p->fixedbars = 200;
if (p->bw > 200) p->bw = 200;
if (p->bw < 1) p->bw = 1;

// validate: framerate
if (p->framerate < 0) {
	fprintf(stderr,
		"framerate can't be negative!\n");
	exit(EXIT_FAILURE);
}

// validate: color
if (!validate_color(p->color, p->om)) {
	fprintf(stderr, "The value for 'foreground' is invalid. It can be either one of the 7 named colors or a HTML color of the form '#xxxxxx'.\n");
	exit(EXIT_FAILURE);
}

// validate: background color
if (!validate_color(p->bcolor, p->om)) {
	fprintf(stderr, "The value for 'background' is invalid. It can be either one of the 7 named colors or a HTML color of the form '#xxxxxx'.\n");
	exit(EXIT_FAILURE);
}

// In case color is not html format set bgcol and col to predefinedint values
p->col = 6;
if (strcmp(p->color, "black") == 0) p->col = 0;
if (strcmp(p->color, "red") == 0) p->col = 1;
if (strcmp(p->color, "green") == 0) p->col = 2;
if (strcmp(p->color, "yellow") == 0) p->col = 3;
if (strcmp(p->color, "blue") == 0) p->col = 4;
if (strcmp(p->color, "magenta") == 0) p->col = 5;
if (strcmp(p->color, "cyan") == 0) p->col = 6;
if (strcmp(p->color, "white") == 0) p->col = 7;
// default if invalid

// validate: background color
p->bgcol = -1;
if (strcmp(p->bcolor, "black") == 0) p->bgcol = 0;
if (strcmp(p->bcolor, "red") == 0) p->bgcol = 1;
if (strcmp(p->bcolor, "green") == 0) p->bgcol = 2;
if (strcmp(p->bcolor, "yellow") == 0) p->bgcol = 3;
if (strcmp(p->bcolor, "blue") == 0) p->bgcol = 4;
if (strcmp(p->bcolor, "magenta") == 0) p->bgcol = 5;
if (strcmp(p->bcolor, "cyan") == 0) p->bgcol = 6;
if (strcmp(p->bcolor, "white") == 0) p->bgcol = 7;
// default if invalid


// validate: gravity
p->gravity = p->gravity / 100;
if (p->gravity < 0) {
	p->gravity = 0;
} 


// validate: integral
p->integral = p->integral / 100;
if (p->integral < 0) {
	p->integral = 0;
} else if (p->integral > 1) {
	p->integral = 1;
}

// validate: cutoff
if (p->lowcf == 0 ) p->lowcf++;
if (p->lowcf > p->highcf) {
	fprintf(stderr,
		"lower cutoff frequency can't be higher than higher cutoff frequency\n");
	exit(EXIT_FAILURE);
}

//setting sens
p->sens = p->sens / 100;


}

void load_config(char configPath[255], char supportedInput[255], void* params)
{

struct config_params *p = (struct config_params *)params;
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
dictionary* ini;
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

p->monstercat = 1.5 * iniparser_getdouble(ini, "smoothing:monstercat", 1);
p->waves = iniparser_getint(ini, "smoothing:waves", 0);
p->integral = iniparser_getdouble(ini, "smoothing:integral", 90);
p->gravity = iniparser_getdouble(ini, "smoothing:gravity", 100);
p->ignore = iniparser_getdouble(ini, "smoothing:ignore", 0);

p->color = (char *)iniparser_getstring(ini, "color:foreground", "default");
p->bcolor = (char *)iniparser_getstring(ini, "color:background", "default");

p->gradient = iniparser_getint(ini, "color:gradient", 0);
if (p->gradient) {
	p->gradient_color_1 = (char *)iniparser_getstring(ini, "color:gradient_color_1", "#0099ff");
	p->gradient_color_2 = (char *)iniparser_getstring(ini, "color:gradient_color_2", "#ff3399");
}

p->fixedbars = iniparser_getint(ini, "general:bars", 0);
p->bw = iniparser_getint(ini, "general:bar_width", 2);
p->bs = iniparser_getint(ini, "general:bar_spacing", 1);
p->framerate = iniparser_getint(ini, "general:framerate", 60);
p->sens = iniparser_getint(ini, "general:sensitivity", 100);
p->autosens = iniparser_getint(ini, "general:autosens", 1);
p->overshoot = iniparser_getint(ini, "general:overshoot", 20);
p->lowcf = iniparser_getint(ini, "general:lower_cutoff_freq", 50);
p->highcf = iniparser_getint(ini, "general:higher_cutoff_freq", 10000);

// config: output
channels =  (char *)iniparser_getstring(ini, "output:channels", "stereo");
p->raw_target = (char *)iniparser_getstring(ini, "output:raw_target", "/dev/stdout");
data_format = (char *)iniparser_getstring(ini, "output:data_format", "binary");
p->bar_delim = (char)iniparser_getint(ini, "output:bar_delimiter", 59);
p->frame_delim = (char)iniparser_getint(ini, "output:frame_delimiter", 10);
p->ascii_range = iniparser_getint(ini, "output:ascii_max_range", 1000);
p->bit_format = iniparser_getint(ini, "output:bit_format", 16);

// read & validate: eq
p->smooth = smoothDef;
p->smcount = iniparser_getsecnkeys(ini, "eq");
if (p->smcount > 0) {
	p->customEQ = 1;
	p->smooth = malloc(p->smcount*sizeof(p->smooth));
	#ifndef LEGACYINIPARSER
	const char *keys[p->smcount];
	iniparser_getseckeys(ini, "eq", keys);
	#endif
	#ifdef LEGACYINIPARSER
	char **keys = iniparser_getseckeys(ini, "eq");
	#endif
	for (int sk = 0; sk < p->smcount; sk++) {
		p->smooth[sk] = iniparser_getdouble(ini, keys[sk], 1);
	}
} else {
	p->customEQ = 0;
	p->smcount = 64; //back to the default one
}

// config: input
p->im = 0;
if (strcmp(inputMethod, "alsa") == 0) {
	p->im = 1;
	p->audio_source = (char *)iniparser_getstring(ini, "input:source", "hw:Loopback,1");
}
if (strcmp(inputMethod, "fifo") == 0) {
	p->im = 2;
	p->audio_source = (char *)iniparser_getstring(ini, "input:source", "/tmp/mpd.fifo");
}
if (strcmp(inputMethod, "pulse") == 0) {
	p->im = 3;
	p->audio_source = (char *)iniparser_getstring(ini, "input:source", "auto");
}

validate_config(supportedInput, params);
//iniparser_freedict(ini);

}
