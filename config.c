#include "config.h"
#include "util.h"

#include <ctype.h>
#include <iniparser.h>
#include <math.h>

#ifdef SNDIO
#include <sndio.h>
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <sys/stat.h>

double smoothDef[5] = {1, 1, 1, 1, 1};

enum input_method default_methods[] = {
    INPUT_FIFO,
    INPUT_PORTAUDIO,
    INPUT_ALSA,
    INPUT_PULSE,
};

char *outputMethod, *channels;

const char *input_method_names[] = {
    "fifo", "portaudio", "alsa", "pulse", "sndio", "shmem",
};

const bool has_input_method[] = {
    true, /** Always have at least FIFO input. */
    HAS_PORTAUDIO, HAS_ALSA, HAS_PULSE, HAS_SNDIO, HAS_SHMEM,
};

enum input_method input_method_by_name(const char *str) {
    for (int i = 0; i < INPUT_MAX; i++) {
        if (!strcmp(str, input_method_names[i])) {
            return (enum input_method)i;
        }
    }

    return INPUT_MAX;
}

void write_errorf(void *err, const char *fmt, ...) {
    struct error_s *error = (struct error_s *)err;
    va_list args;
    va_start(args, fmt);
    error->length +=
        vsnprintf((char *)error->message + error->length, MAX_ERROR_LEN - error->length, fmt, args);
    va_end(args);
}

int validate_color(char *checkColor, int om, void *err) {
    struct error_s *error = (struct error_s *)err;
    int validColor = 0;
    if (checkColor[0] == '#' && strlen(checkColor) == 7) {
        // If the output mode is not ncurses, tell the user to use a named colour instead of hex
        // colours.
        if (om != 1 && om != 2) {
            write_errorf(error, "Only 'ncurses' output method supports HTML colors. Please change "
                                "the colours or the output method.\n");
            return 0;
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
        if ((strcmp(checkColor, "black") == 0) || (strcmp(checkColor, "red") == 0) ||
            (strcmp(checkColor, "green") == 0) || (strcmp(checkColor, "yellow") == 0) ||
            (strcmp(checkColor, "blue") == 0) || (strcmp(checkColor, "magenta") == 0) ||
            (strcmp(checkColor, "cyan") == 0) || (strcmp(checkColor, "white") == 0) ||
            (strcmp(checkColor, "default") == 0))
            validColor = 1;
    }
    return validColor;
}

bool validate_colors(void *params, void *err) {
    struct config_params *p = (struct config_params *)params;
    struct error_s *error = (struct error_s *)err;

    // validate: color
    if (!validate_color(p->color, p->om, error)) {
        write_errorf(error, "The value for 'foreground' is invalid. It can be either one of the 7 "
                            "named colors or a HTML color of the form '#xxxxxx'.\n");
        return false;
    }

    // validate: background color
    if (!validate_color(p->bcolor, p->om, error)) {
        write_errorf(error, "The value for 'background' is invalid. It can be either one of the 7 "
                            "named colors or a HTML color of the form '#xxxxxx'.\n");
        return false;
    }

    if (p->gradient) {
        for (int i = 0; i < p->gradient_count; i++) {
            if (!validate_color(p->gradient_colors[i], p->om, error)) {
                write_errorf(
                    error,
                    "Gradient color %d is invalid. It must be HTML color of the form '#xxxxxx'.\n",
                    i + 1);
                return false;
            }
        }
    }

    // In case color is not html format set bgcol and col to predefinedint values
    p->col = 6;
    if (strcmp(p->color, "black") == 0)
        p->col = 0;
    if (strcmp(p->color, "red") == 0)
        p->col = 1;
    if (strcmp(p->color, "green") == 0)
        p->col = 2;
    if (strcmp(p->color, "yellow") == 0)
        p->col = 3;
    if (strcmp(p->color, "blue") == 0)
        p->col = 4;
    if (strcmp(p->color, "magenta") == 0)
        p->col = 5;
    if (strcmp(p->color, "cyan") == 0)
        p->col = 6;
    if (strcmp(p->color, "white") == 0)
        p->col = 7;
    // default if invalid

    // validate: background color
    if (strcmp(p->bcolor, "black") == 0)
        p->bgcol = 0;
    if (strcmp(p->bcolor, "red") == 0)
        p->bgcol = 1;
    if (strcmp(p->bcolor, "green") == 0)
        p->bgcol = 2;
    if (strcmp(p->bcolor, "yellow") == 0)
        p->bgcol = 3;
    if (strcmp(p->bcolor, "blue") == 0)
        p->bgcol = 4;
    if (strcmp(p->bcolor, "magenta") == 0)
        p->bgcol = 5;
    if (strcmp(p->bcolor, "cyan") == 0)
        p->bgcol = 6;
    if (strcmp(p->bcolor, "white") == 0)
        p->bgcol = 7;
    // default if invalid

    return true;
}

bool validate_config(struct config_params *p, struct error_s *error) {
    // validate: output method
    p->om = 0;
    if (strcmp(outputMethod, "ncurses") == 0) {
        p->om = 1;
        p->bgcol = -1;
#ifndef NCURSES
        write_errorf(error, "cava was built without ncurses support, install ncursesw dev files "
                            "and run make clean && ./configure && make again\n");
        return false;
#endif
    }
    if (strcmp(outputMethod, "circle") == 0) {
        p->om = 2;
#ifndef NCURSES
        write_errorf(error, "cava was built without ncurses support, install ncursesw dev files "
                            "and run make clean && ./configure && make again\n");
        return false;
#endif
    }
    if (strcmp(outputMethod, "noncurses") == 0) {
        p->om = 3;
        p->bgcol = 0;
    }
    if (strcmp(outputMethod, "raw") == 0) { // raw:
        p->om = 4;
        p->bs = 0;
        p->bw = 1;

        // checking data format
        p->is_bin = -1;
        if (strcmp(p->data_format, "binary") == 0) {
            p->is_bin = 1;
            // checking bit format:
            if (p->bit_format != 8 && p->bit_format != 16) {
                write_errorf(
                    error,
                    "bit format  %d is not supported, supported data formats are: '8' and '16'\n",
                    p->bit_format);
                return false;
            }
        } else if (strcmp(p->data_format, "ascii") == 0) {
            p->is_bin = 0;
            if (p->ascii_range < 1) {
                write_errorf(error, "ascii max value must be a positive integer\n");
                return false;
            }
        } else {
            write_errorf(error,
                         "data format %s is not supported, supported data formats are: 'binary' "
                         "and 'ascii'\n",
                         p->data_format);
            return false;
        }
    }
    if (p->om == 0) {
#ifndef NCURSES
        write_errorf(error,
                     "output method %s is not supported, supported methods are: 'noncurses'\n",
                     outputMethod);
        return false;
#endif

#ifdef NCURSES
        write_errorf(
            error,
            "output method %s is not supported, supported methods are: 'ncurses' and 'noncurses'\n",
            outputMethod);
        return false;
#endif
    }

    // validate: output channels
    p->stereo = -1;
    if (strcmp(channels, "mono") == 0) {
        p->stereo = 0;
        if (strcmp(p->mono_option, "average") != 0 && strcmp(p->mono_option, "left") != 0 &&
            strcmp(p->mono_option, "right") != 0) {

            write_errorf(error,
                         "mono option %s is not supported, supported options are: 'average', "
                         "'left' or 'right'\n",
                         p->mono_option);
            return false;
        }
    }
    if (strcmp(channels, "stereo") == 0)
        p->stereo = 1;
    if (p->stereo == -1) {
        write_errorf(
            error,
            "output channels %s is not supported, supported channelss are: 'mono' and 'stereo'\n",
            channels);
        return false;
    }

    // validate: bars
    p->autobars = 1;
    if (p->fixedbars > 0)
        p->autobars = 0;
    if (p->fixedbars > 200)
        p->fixedbars = 200;
    if (p->bw > 200)
        p->bw = 200;
    if (p->bw < 1)
        p->bw = 1;

    // validate: framerate
    if (p->framerate < 0) {
        write_errorf(error, "framerate can't be negative!\n");
        return false;
    }

    if (!validate_colors(p, error)) {
        return false;
    }

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
    if (p->lowcf == 0)
        p->lowcf++;
    if (p->lowcf > p->highcf) {
        write_errorf(error,
                     "lower cutoff frequency can't be higher than higher cutoff frequency\n");
        return false;
    }

    // setting sens
    p->sens = p->sens / 100;

    // validate FFT buffer
    if (p->FFTbufferSize >= 8 && p->FFTbufferSize <= 16) {
        p->FFTbufferSize = pow(2, p->FFTbufferSize);
    } else {
        write_errorf(error, "FFT buffer is set in the exponent of 2 and must be between 8 - 16\n");
        return false;
    }

    return true;
}

bool load_colors(struct config_params *p, dictionary *ini, void *err) {
    struct error_s *error = (struct error_s *)err;

    free(p->color);
    free(p->bcolor);

    p->color = strdup(iniparser_getstring(ini, "color:foreground", "default"));
    p->bcolor = strdup(iniparser_getstring(ini, "color:background", "default"));

    p->gradient = iniparser_getint(ini, "color:gradient", 0);
    if (p->gradient) {
        for (int i = 0; i < p->gradient_count; ++i) {
            free(p->gradient_colors[i]);
        }
        p->gradient_count = iniparser_getint(ini, "color:gradient_count", 2);
        if (p->gradient_count < 2) {
            write_errorf(error, "\nAtleast two colors must be given as gradient!\n");
            return false;
        }
        if (p->gradient_count > 8) {
            write_errorf(error, "\nMaximum 8 colors can be specified as gradient!\n");
            return false;
        }
        p->gradient_colors = (char **)malloc(sizeof(char *) * p->gradient_count);
        for (int i = 0; i < p->gradient_count; i++) {
            char ini_config[32];
            sprintf(ini_config, "color:gradient_color_%d", (i + 1));
            p->gradient_colors[i] = strdup(iniparser_getstring(ini, ini_config, NULL));
            if (p->gradient_colors[i] == NULL) {
                write_errorf(error, "\nGradient color not specified : gradient_color_%d\n",
                             (i + 1));
                return false;
            }
        }
        // p->gradient_color_1 = (char *)iniparser_getstring(ini, "color:gradient_color_1",
        // "#0099ff"); p->gradient_color_2 = (char *)iniparser_getstring(ini,
        // "color:gradient_color_2", "#ff3399");
    }
    return true;
}

bool load_config(char configPath[PATH_MAX], struct config_params *p, bool colorsOnly,
                 struct error_s *error) {
    FILE *fp;

    // config: creating path to default config file
    if (configPath[0] == '\0') {
        char *configFile = "config";
        char *configHome = getenv("XDG_CONFIG_HOME");
        if (configHome != NULL) {
            sprintf(configPath, "%s/%s/", configHome, PACKAGE);
        } else {
            configHome = getenv("HOME");
            if (configHome != NULL) {
                sprintf(configPath, "%s/%s/", configHome, ".config");
                mkdir(configPath, 0777);
                sprintf(configPath, "%s/%s/%s/", configHome, ".config", PACKAGE);
            } else {
                write_errorf(error, "No HOME found (ERR_HOMELESS), exiting...");
                return false;
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
            write_errorf(error, "Unable to access config '%s', exiting...\n", configPath);
            return false;
        }

    } else { // opening specified file

        fp = fopen(configPath, "rb+");
        if (fp) {
            fclose(fp);
        } else {
            write_errorf(error, "Unable to open file '%s', exiting...\n", configPath);
            return false;
        }
    }

    // config: parse ini
    dictionary *ini;
    ini = iniparser_load(configPath);

    if (colorsOnly) {
        if (!load_colors(p, ini, error)) {
            return false;
        }
        return validate_colors(p, error);
    }

#ifdef NCURSES
    outputMethod = (char *)iniparser_getstring(ini, "output:method", "ncurses");
#endif
#ifndef NCURSES
    outputMethod = (char *)iniparser_getstring(ini, "output:method", "noncurses");
#endif

    p->monstercat = 1.5 * iniparser_getdouble(ini, "smoothing:monstercat", 0);
    p->waves = iniparser_getint(ini, "smoothing:waves", 0);
    p->integral = iniparser_getdouble(ini, "smoothing:integral", 77);
    p->gravity = iniparser_getdouble(ini, "smoothing:gravity", 100);
    p->ignore = iniparser_getdouble(ini, "smoothing:ignore", 0);

    if (!load_colors(p, ini, error)) {
        return false;
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
    p->FFTbufferSize = iniparser_getint(ini, "general:FFTbufferSize", 12);

    // config: output
    free(channels);
    free(p->mono_option);
    free(p->raw_target);
    free(p->data_format);

    channels = strdup(iniparser_getstring(ini, "output:channels", "stereo"));
    p->mono_option = strdup(iniparser_getstring(ini, "output:mono_option", "average"));
    p->raw_target = strdup(iniparser_getstring(ini, "output:raw_target", "/dev/stdout"));
    p->data_format = strdup(iniparser_getstring(ini, "output:data_format", "binary"));
    p->bar_delim = (char)iniparser_getint(ini, "output:bar_delimiter", 59);
    p->frame_delim = (char)iniparser_getint(ini, "output:frame_delimiter", 10);
    p->ascii_range = iniparser_getint(ini, "output:ascii_max_range", 1000);
    p->bit_format = iniparser_getint(ini, "output:bit_format", 16);

    // read & validate: eq
    p->smooth = smoothDef;
    p->smcount = iniparser_getsecnkeys(ini, "eq");
    if (p->smcount > 0) {
        p->customEQ = 1;
        p->smooth = calloc(p->smcount + 1, sizeof(p->smooth));
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
        p->smcount = sizeof(smoothDef) / sizeof(smoothDef[0]); // back to the default one
    }

    free(p->audio_source);

    char *input_method_name;
    for (size_t i = 0; i < ARRAY_SIZE(default_methods); i++) {
        enum input_method method = default_methods[i];
        if (has_input_method[method]) {
            input_method_name =
                (char *)iniparser_getstring(ini, "input:method", input_method_names[method]);
        }
    }

    p->im = input_method_by_name(input_method_name);
    switch (p->im) {
#ifdef ALSA
    case INPUT_ALSA:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "hw:Loopback,1"));
        break;
#endif
    case INPUT_FIFO:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "/tmp/mpd.fifo"));
        p->fifoSample = iniparser_getint(ini, "input:sample_rate", 44100);
        p->fifoSampleBits = iniparser_getint(ini, "input:sample_bits", 16);
        break;
#ifdef PULSE
    case INPUT_PULSE:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "auto"));
        break;
#endif
#ifdef SNDIO
    case INPUT_SNDIO:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", SIO_DEVANY));
        break;
#endif
#ifdef SHMEM
    case INPUT_SHMEM:
        p->audio_source =
            strdup(iniparser_getstring(ini, "input:source", "/squeezelite-00:00:00:00:00:00"));
        break;
#endif
#ifdef PORTAUDIO
    case INPUT_PORTAUDIO:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "auto"));
        break;
#endif
    case INPUT_MAX: {
        char supported_methods[255] = "";
        for (int i = 0; i < INPUT_MAX; i++) {
            if (has_input_method[i]) {
                strcat(supported_methods, "'");
                strcat(supported_methods, input_method_names[i]);
                strcat(supported_methods, "' ");
            }
        }
        write_errorf(error, "input method '%s' is not supported, supported methods are: %s\n",
                     input_method_name, supported_methods);
        return false;
    }
    default:
        write_errorf(error, "cava was built without '%s' input support\n",
                     input_method_names[p->im]);
        return false;
    }

    bool result = validate_config(p, error);
    iniparser_freedict(ini);
    return result;
}
