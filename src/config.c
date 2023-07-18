#include "cava/config.h"

#include "cava/util.h"

#include <ctype.h>
#ifndef _MSC_VER
#if __has_include(<iniparser.h>)
#include <iniparser.h>
#elif __has_include(<iniparser4/iniparser.h>)
#include <iniparser4/iniparser.h>
#endif
#endif
#include <math.h>

#ifdef SNDIO
#include <sndio.h>
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <sys/stat.h>

#define NUMBER_OF_SHADERS 3

#ifdef _MSC_VER
#include "Windows.h"
#define PATH_MAX 260
#define PACKAGE "cava"
#define _CRT_SECURE_NO_WARNINGS 1
#else
#define INCBIN_SILENCE_BITCODE_WARNING
#include "third_party/incbin.h"

INCTXT(ConfigFile, "example_files/config");

// add your custom shaders to be installed here
INCTXT(bar_spectrum, "src/output/shaders/bar_spectrum.frag");
INCTXT(northern_lightsfrag, "src/output/shaders/northern_lights.frag");
INCTXT(pass_throughvert, "src/output/shaders/pass_through.vert");

// INCTXT will create a char g<name>Data
const char *default_shader_data[NUMBER_OF_SHADERS] = {gnorthern_lightsfragData,
                                                      gpass_throughvertData, gbar_spectrumData};
#endif // _MSC_VER
// name of the installed shader file, technically does not have to be the same as in the source
const char *default_shader_name[NUMBER_OF_SHADERS] = {"northern_lights.frag", "pass_through.vert",
                                                      "bar_spectrum.frag"};

double smoothDef[5] = {1, 1, 1, 1, 1};

enum input_method default_methods[] = {
    INPUT_FIFO, INPUT_PORTAUDIO, INPUT_ALSA, INPUT_PULSE, INPUT_WINSCAP,
};

char *outputMethod, *orientation, *channels, *xaxisScale, *monoOption, *fragmentShader,
    *vertexShader;

const char *input_method_names[] = {
    "fifo", "portaudio", "pipewire", "alsa", "pulse", "sndio", "shmem", "winscap",
};

const bool has_input_method[] = {
    HAS_FIFO, /** Always have at least FIFO and shmem input. */
    HAS_PORTAUDIO, HAS_PIPEWIRE, HAS_ALSA, HAS_PULSE, HAS_SNDIO, HAS_SHMEM, HAS_WINSCAP,
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
int validate_color(char *checkColor, void *params, void *err) {
    struct config_params *p = (struct config_params *)params;
    struct error_s *error = (struct error_s *)err;
    int validColor = 0;
    if (checkColor[0] == '#' && strlen(checkColor) == 7) {
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
        if (p->output == OUTPUT_SDL) {
            write_errorf(error, "SDL only supports setting color in html format\n");
            return 0;
        }
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
    if (!validate_color(p->color, p, error)) {
        write_errorf(error, "The value for 'foreground' is invalid. It can be either one of the 7 "
                            "named colors or a HTML color of the form '#xxxxxx'.\n");
        return false;
    }

    // validate: background color
    if (!validate_color(p->bcolor, p, error)) {
        write_errorf(error, "The value for 'background' is invalid. It can be either one of the 7 "
                            "named colors or a HTML color of the form '#xxxxxx'.\n");
        return false;
    }

    if (p->gradient) {
        for (int i = 0; i < p->gradient_count; i++) {
            if (!validate_color(p->gradient_colors[i], p, error)) {
                write_errorf(
                    error,
                    "Gradient color %d is invalid. It must be HTML color of the form '#xxxxxx'.\n",
                    i + 1);
                return false;
            }
        }
    }

    // In case color is not html format set bgcol and col to predefinedint values
    p->col = -1;
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
    if (p->color[0] == '#')
        p->col = 8;
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
    if (p->bcolor[0] == '#')
        p->bgcol = 8;
    // default if invalid
    if (p->gradient) {

        if (p->gradient_count < 2) {
            write_errorf(error, "\nAtleast two colors must be given as gradient!\n");
            return false;
        }
        if (p->gradient_count > 8) {
            write_errorf(error, "\nMaximum 8 colors can be specified as gradient!\n");
            return false;
        }
    }

    return true;
}

bool validate_config(struct config_params *p, struct error_s *error) {
    // validate: output method
    p->output = OUTPUT_NOT_SUPORTED;
    if (strcmp(outputMethod, "ncurses") == 0) {
        p->output = OUTPUT_NCURSES;
        p->bgcol = -1;
#ifndef NCURSES
        write_errorf(error, "cava was built without ncurses support, install ncursesw dev files "
                            "and run make clean && ./configure && make again\n");
        return false;
#endif
    }
    if (strcmp(outputMethod, "sdl") == 0) {
        p->output = OUTPUT_SDL;
#ifndef SDL
        write_errorf(error, "cava was built without sdl support, install sdl dev files "
                            "and run make clean && ./configure && make again\n");
        return false;
#endif
    }
    if (strcmp(outputMethod, "sdl_glsl") == 0) {
        p->output = OUTPUT_SDL_GLSL;
#ifndef SDL_GLSL
        write_errorf(error, "cava was built without sdl support, install sdl dev files "
                            "and run make clean && ./configure && make again\n");
        return false;
#endif
    }
    if (strcmp(outputMethod, "noncurses") == 0) {
        p->output = OUTPUT_NONCURSES;
        p->bgcol = 0;
    }
    if (strcmp(outputMethod, "noritake") == 0) {
        p->output = OUTPUT_NORITAKE;
        p->raw_format = FORMAT_NTK3000; // only format supported now
    }
    if (strcmp(outputMethod, "raw") == 0) { // raw:
        p->output = OUTPUT_RAW;
        p->bar_spacing = 0;
        p->bar_width = 1;

        // checking data format
        p->raw_format = -1;
        if (strcmp(p->data_format, "binary") == 0) {
            p->raw_format = FORMAT_BINARY;
            // checking bit format:
            if (p->bit_format != 8 && p->bit_format != 16) {
                write_errorf(
                    error,
                    "bit format  %d is not supported, supported data formats are: '8' and '16'\n",
                    p->bit_format);
                return false;
            }
        } else if (strcmp(p->data_format, "ascii") == 0) {
            p->raw_format = FORMAT_ASCII;
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
    if (p->output == OUTPUT_NOT_SUPORTED) {

        char supportedOutput[1024] = "'noncurses', 'raw', 'noritake'";

#ifdef NCURSES
        strcat(supportedOutput, ", 'ncurses'");
#endif
#ifdef SDL
        strcat(supportedOutput, ", 'sdl'");
#endif
#ifdef SDL_GLSL
        strcat(supportedOutput, ", 'sdl_glsl'");
#endif
        write_errorf(error, "output method %s is not supported, supported methods are: %s\n",
                     outputMethod, supportedOutput);
        return false;
    }

    p->orientation = ORIENT_BOTTOM;
    if (p->output == OUTPUT_SDL || p->output == OUTPUT_NCURSES) {
        if (strcmp(orientation, "top") == 0) {
            p->orientation = ORIENT_TOP;
        }
        if (strcmp(orientation, "left") == 0) {
            p->orientation = ORIENT_LEFT;
        }
        if (strcmp(orientation, "right") == 0) {
            p->orientation = ORIENT_RIGHT;
        }
    }

    p->xaxis = NONE;
    if (strcmp(xaxisScale, "none") == 0) {
        p->xaxis = NONE;
    }
    if (strcmp(xaxisScale, "frequency") == 0) {
        p->xaxis = FREQUENCY;
    }
    if (strcmp(xaxisScale, "note") == 0) {
        p->xaxis = NOTE;
    }

    // validate: output channels
    p->stereo = -1;
    if (strcmp(channels, "mono") == 0) {
        p->stereo = 0;
        if (strcmp(monoOption, "average") == 0) {
            p->mono_opt = AVERAGE;
        } else if (strcmp(monoOption, "left") == 0) {
            p->mono_opt = LEFT;
        } else if (strcmp(monoOption, "right") == 0) {
            p->mono_opt = RIGHT;
        } else {
            write_errorf(error,
                         "mono option %s is not supported, supported options are: 'average', "
                         "'left' or 'right'\n",
                         monoOption);
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
    if (p->bar_width < 1)
        p->bar_width = 1;

    // validate: framerate
    if (p->framerate < 0) {
        write_errorf(error, "framerate can't be negative!\n");
        return false;
    }

    // validate: colors
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

    // validate: noise_reduction
    p->noise_reduction /= 100;
    if (p->noise_reduction < 0) {
        p->noise_reduction = 0;
    } else if (p->noise_reduction > 1) {
        p->noise_reduction = 1;
    }

    // validate: cutoff
    if (p->lower_cut_off == 0)
        p->lower_cut_off++;
    if (p->lower_cut_off > p->upper_cut_off) {
        write_errorf(error,
                     "lower cutoff frequency can't be higher than higher cutoff frequency\n");
        return false;
    }

    // setting sens
    p->sens = p->sens / 100;

    return validate_colors(p, error);
}

bool load_config(char configPath[PATH_MAX], struct config_params *p, bool colorsOnly,
                 struct error_s *error) {
    FILE *fp;
    char cava_config_home[PATH_MAX / 2];

    // config: creating path to default config file
    char *configHome = getenv("XDG_CONFIG_HOME");
    if (configHome != NULL) {
        sprintf(cava_config_home, "%s/%s/", configHome, PACKAGE);
        mkdir(cava_config_home, 0777);
    } else {
#ifndef _MSC_VER
        configHome = getenv("HOME");

#else
        configHome = getenv("userprofile");
#endif
        if (configHome != NULL) {
            sprintf(cava_config_home, "%s/%s/", configHome, ".config");
            mkdir(cava_config_home, 0777);

            sprintf(cava_config_home, "%s/%s/%s/", configHome, ".config", PACKAGE);
            mkdir(cava_config_home, 0777);
        } else {
#ifndef _MSC_VER
            sprintf(cava_config_home, "/tmp/%s/", PACKAGE);
            mkdir(cava_config_home, 0777);
#else
            write_errorf(error, "No HOME found (ERR_HOMELESS), exiting...");
            return false;
#endif
        }
    }
    if (configPath[0] == '\0') {
        // config: adding default filename file
        strcat(configPath, cava_config_home);
        strcat(configPath, "config");

        // open file or create file if it does not exist
        fp = fopen(configPath, "ab+");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            if (ftell(fp) == 0) {
#ifndef _MSC_VER
                printf("config file is empty, creating default config file\n");
                fwrite(gConfigFileData, gConfigFileSize - 1, sizeof(char), fp);
#else
                printf(
                    "WARNING: config file is empty, windows does not support automatic default "
                    "config "
                    "generation.\n An empty file was created at %s, overwrite with the default "
                    "config from the source to get a complete and documented list of options.\n\n",
                    configPath);
#endif
            }
            fclose(fp);
        } else {
            // try to open file read only
            fp = fopen(configPath, "rb");
            if (fp) {
                fclose(fp);
            } else {
                write_errorf(error, "Unable to open or create file '%s', exiting...\n", configPath);
                return false;
            }
        }

    } else { // opening specified file

        fp = fopen(configPath, "rb");
        if (fp) {
            fclose(fp);
        } else {
            write_errorf(error, "Unable to open file '%s', exiting...\n", configPath);
            return false;
        }
    }

    // create default shader files if they do not exist
    char *shaderPath = malloc(sizeof(char) * PATH_MAX);
    sprintf(shaderPath, "%s/shaders", cava_config_home);
    mkdir(shaderPath, 0777);

    for (int i = 0; i < NUMBER_OF_SHADERS; i++) {
        char *shaderFile = malloc(sizeof(char) * PATH_MAX);
        sprintf(shaderFile, "%s/%s", shaderPath, default_shader_name[i]);

        fp = fopen(shaderFile, "ab+");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            if (ftell(fp) == 0) {
#ifndef _MSC_VER
                printf("shader file is empty, creating default shader file\n");
                fwrite(default_shader_data[i], strlen(default_shader_data[i]), sizeof(char), fp);
#else
                printf(
                    "WARNING: shader file is empty, windows does not support automatic default "
                    "shader "
                    "generation.\n In order to use the shader copy the file %s from the source to "
                    "%s\n\n",
                    default_shader_name[i], shaderPath);
#endif
            }
            fclose(fp);
            free(shaderFile);
        }
    }
    free(shaderPath);

    p->gradient_colors = (char **)malloc(sizeof(char *) * 8 * 9);
    for (int i = 0; i < 8; ++i) {
        p->gradient_colors[i] = (char *)malloc(sizeof(char *) * 9);
    }
    p->vertex_shader = malloc(sizeof(char) * PATH_MAX);
    p->fragment_shader = malloc(sizeof(char) * PATH_MAX);

#ifndef _MSC_VER
    // config: parse ini
    dictionary *ini;
    ini = iniparser_load(configPath);

    free(p->color);
    free(p->bcolor);

    p->color = strdup(iniparser_getstring(ini, "color:foreground", "default"));
    p->bcolor = strdup(iniparser_getstring(ini, "color:background", "default"));

    p->gradient = iniparser_getint(ini, "color:gradient", 0);
    p->gradient_count = iniparser_getint(ini, "color:gradient_count", 8);
    for (int i = 0; i < 8; ++i) {
        free(p->gradient_colors[i]);
    }
    free(p->gradient_colors);
    p->gradient_colors = (char **)malloc(sizeof(char *) * p->gradient_count * 9);
    p->gradient_colors[0] = strdup(iniparser_getstring(ini, "color:gradient_color_1", "#59cc33"));
    p->gradient_colors[1] = strdup(iniparser_getstring(ini, "color:gradient_color_2", "#80cc33"));
    p->gradient_colors[2] = strdup(iniparser_getstring(ini, "color:gradient_color_3", "#a6cc33"));
    p->gradient_colors[3] = strdup(iniparser_getstring(ini, "color:gradient_color_4", "#cccc33"));
    p->gradient_colors[4] = strdup(iniparser_getstring(ini, "color:gradient_color_5", "#cca633"));
    p->gradient_colors[5] = strdup(iniparser_getstring(ini, "color:gradient_color_6", "#cc8033"));
    p->gradient_colors[6] = strdup(iniparser_getstring(ini, "color:gradient_color_7", "#cc5933"));
    p->gradient_colors[7] = strdup(iniparser_getstring(ini, "color:gradient_color_8", "#cc3333"));

#else
    outputMethod = malloc(sizeof(char) * 32);
    p->color = malloc(sizeof(char) * 14);
    p->bcolor = malloc(sizeof(char) * 14);
    p->audio_source = malloc(sizeof(char) * 129);

    xaxisScale = malloc(sizeof(char) * 32);
    channels = malloc(sizeof(char) * 32);
    monoOption = malloc(sizeof(char) * 32);
    p->raw_target = malloc(sizeof(char) * 129);
    p->data_format = malloc(sizeof(char) * 32);
    channels = malloc(sizeof(char) * 32);
    orientation = malloc(sizeof(char) * 32);
    vertexShader = malloc(sizeof(char) * PATH_MAX / 2);
    fragmentShader = malloc(sizeof(char) * PATH_MAX / 2);

    GetPrivateProfileString("color", "foreground", "default", p->color, 9, configPath);
    GetPrivateProfileString("color", "background", "default", p->bcolor, 9, configPath);
    p->gradient = GetPrivateProfileInt("color", "gradient", 0, configPath);
    p->gradient_count = GetPrivateProfileInt("color", "gradient_count", 8, configPath);

    GetPrivateProfileString("color", "gradient_color_1", "#59cc33", p->gradient_colors[0], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_2", "#80cc33", p->gradient_colors[1], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_3", "#a6cc33", p->gradient_colors[2], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_4", "#cccc33", p->gradient_colors[3], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_5", "#cca633", p->gradient_colors[4], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_6", "#cc8033", p->gradient_colors[5], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_7", "#cc5933", p->gradient_colors[6], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_8", "#cc3333", p->gradient_colors[7], 9,
                            configPath);

#endif
    if (colorsOnly) {
        return validate_colors(p, error);
    }
#ifndef _MSC_VER

#ifdef NCURSES
    outputMethod = strdup(iniparser_getstring(ini, "output:method", "ncurses"));
#endif
#ifndef NCURSES
    outputMethod = strdup(iniparser_getstring(ini, "output:method", "noncurses"));
#endif

    orientation = strdup(iniparser_getstring(ini, "output:orientation", "bottom"));
    xaxisScale = strdup(iniparser_getstring(ini, "output:xaxis", "none"));
    p->monstercat = iniparser_getdouble(ini, "smoothing:monstercat", 0);
    p->waves = iniparser_getint(ini, "smoothing:waves", 0);
    p->integral = iniparser_getdouble(ini, "smoothing:integral", 77);
    p->gravity = iniparser_getdouble(ini, "smoothing:gravity", 100);
    p->ignore = iniparser_getdouble(ini, "smoothing:ignore", 0);
    p->noise_reduction = iniparser_getdouble(ini, "smoothing:noise_reduction", 77);

    p->fixedbars = iniparser_getint(ini, "general:bars", 0);
    p->bar_width = iniparser_getint(ini, "general:bar_width", 2);
    p->bar_spacing = iniparser_getint(ini, "general:bar_spacing", 1);
    p->bar_height = iniparser_getint(ini, "general:bar_height", 32);
    p->framerate = iniparser_getint(ini, "general:framerate", 60);
    p->sens = iniparser_getint(ini, "general:sensitivity", 100);
    p->autosens = iniparser_getint(ini, "general:autosens", 1);
    p->overshoot = iniparser_getint(ini, "general:overshoot", 20);
    p->lower_cut_off = iniparser_getint(ini, "general:lower_cutoff_freq", 50);
    p->upper_cut_off = iniparser_getint(ini, "general:higher_cutoff_freq", 10000);
    p->sleep_timer = iniparser_getint(ini, "general:sleep_timer", 0);

    // hidden test features

    // draw this many frames and quit, used for testing
    p->draw_and_quit = iniparser_getint(ini, "general:draw_and_quit", 0);
    // expect combined values of bar height in last frame to be 0
    p->zero_test = iniparser_getint(ini, "general:zero_test", 0);
    // or not 0
    p->non_zero_test = iniparser_getint(ini, "general:non_zero_test", 0);

    // config: output
    free(channels);
    free(monoOption);
    free(p->raw_target);
    free(p->data_format);
    free(vertexShader);
    free(fragmentShader);

    channels = strdup(iniparser_getstring(ini, "output:channels", "stereo"));
    monoOption = strdup(iniparser_getstring(ini, "output:mono_option", "average"));
    p->reverse = iniparser_getint(ini, "output:reverse", 0);
    p->raw_target = strdup(iniparser_getstring(ini, "output:raw_target", "/dev/stdout"));
    p->data_format = strdup(iniparser_getstring(ini, "output:data_format", "binary"));
    p->bar_delim = (char)iniparser_getint(ini, "output:bar_delimiter", 59);
    p->frame_delim = (char)iniparser_getint(ini, "output:frame_delimiter", 10);
    p->ascii_range = iniparser_getint(ini, "output:ascii_max_range", 1000);
    p->bit_format = iniparser_getint(ini, "output:bit_format", 16);

    p->sdl_width = iniparser_getint(ini, "output:sdl_width", 1000);
    p->sdl_height = iniparser_getint(ini, "output:sdl_height", 500);
    p->sdl_x = iniparser_getint(ini, "output:sdl_x", -1);
    p->sdl_y = iniparser_getint(ini, "output:sdl_y", -1);

    if (strcmp(outputMethod, "sdl") == 0 || strcmp(outputMethod, "sdl_glsl") == 0) {
        free(p->color);
        free(p->bcolor);
        p->color = strdup(iniparser_getstring(ini, "color:foreground", "#33cccc"));
        p->bcolor = strdup(iniparser_getstring(ini, "color:background", "#111111"));
        p->bar_width = iniparser_getint(ini, "general:bar_width", 20);
        p->bar_spacing = iniparser_getint(ini, "general:bar_spacing", 5);
    }

    p->continuous_rendering = iniparser_getint(ini, "output:continuous_rendering", 0);

    p->sync_updates = iniparser_getint(ini, "output:alacritty_sync", 0);

    vertexShader = strdup(iniparser_getstring(ini, "output:vertex_shader", "pass_through.vert"));
    fragmentShader =
        strdup(iniparser_getstring(ini, "output:fragment_shader", "bar_spectrum.frag"));

    // read & validate: eq
    p->userEQ_keys = iniparser_getsecnkeys(ini, "eq");
    if (p->userEQ_keys > 0) {
        p->userEQ_enabled = 1;
        p->userEQ = (double *)calloc(p->userEQ_keys + 1, sizeof(double));
#ifndef LEGACYINIPARSER
        const char *keys[p->userEQ_keys];
        iniparser_getseckeys(ini, "eq", keys);
#endif
#ifdef LEGACYINIPARSER
        char **keys = iniparser_getseckeys(ini, "eq");
#endif
        for (int sk = 0; sk < p->userEQ_keys; sk++) {
            p->userEQ[sk] = iniparser_getdouble(ini, keys[sk], 1);
        }
    } else {
        p->userEQ_enabled = 0;
    }

    free(p->audio_source);

    int default_input = -1;
    for (size_t i = 0; i < ARRAY_SIZE(default_methods); i++) {
        enum input_method method = default_methods[i];
        if (has_input_method[method]) {
            default_input++;
        }
    }
    char *input_method_name =
        strdup(iniparser_getstring(ini, "input:method", input_method_names[default_input]));
    p->input = input_method_by_name(input_method_name);
    switch (p->input) {
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
#ifdef PIPEWIRE
    case INPUT_PIPEWIRE:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "auto"));
        break;
#endif
#ifdef SNDIO
    case INPUT_SNDIO:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", SIO_DEVANY));
        break;
#endif
    case INPUT_SHMEM:
        p->audio_source =
            strdup(iniparser_getstring(ini, "input:source", "/squeezelite-00:00:00:00:00:00"));
        break;
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
                     input_method_names[p->input]);
        return false;
    }
    free(input_method_name);
    iniparser_freedict(ini);
#else

    GetPrivateProfileString("output", "method", "noncurses", outputMethod, 16, configPath);

    p->waves = GetPrivateProfileInt("smoothing", "waves", 0, configPath);
    p->monstercat = GetPrivateProfileInt("smoothing", "monstercat", 0, configPath);

    p->noise_reduction = GetPrivateProfileInt("smoothing", "noise_reduction", 77, configPath);
    GetPrivateProfileString("output", "xaxis", "none", xaxisScale, 16, configPath);
    GetPrivateProfileString("output", "orientation", "bottom", orientation, 16, configPath);

    p->fixedbars = GetPrivateProfileInt("general", "bars", 0, configPath);

    p->bar_width = GetPrivateProfileInt("general", "bar_width", 2, configPath);
    p->bar_spacing = GetPrivateProfileInt("general", "bar_spacing", 1, configPath);

    p->bar_height = GetPrivateProfileInt("general", "bar_height", 32, configPath);
    p->framerate = GetPrivateProfileInt("general", "framerate", 60, configPath);
    p->sens = GetPrivateProfileInt("general", "sensitivity", 100, configPath);
    p->autosens = GetPrivateProfileInt("general", "autosens", 1, configPath);
    p->overshoot = GetPrivateProfileInt("general", "overshoot", 20, configPath);
    p->lower_cut_off = GetPrivateProfileInt("general", "lower_cutoff_freq", 50, configPath);
    p->upper_cut_off = GetPrivateProfileInt("general", "higher_cutoff_freq", 10000, configPath);
    p->sleep_timer = GetPrivateProfileInt("general", "sleep_timer", 0, configPath);

    GetPrivateProfileString("output", "channels", "stereo", channels, 16, configPath);
    GetPrivateProfileString("output", "mono_option", "average", monoOption, 16, configPath);
    p->reverse = GetPrivateProfileInt("output", "reverse", 0, configPath);
    GetPrivateProfileString("output", "raw_target", "stdout", p->raw_target, 64, configPath);
    GetPrivateProfileString("output", "data_format", "binary", p->data_format, 16, configPath);
    p->bar_delim = (char)GetPrivateProfileInt("output", "bar_delimiter", 50, configPath);
    p->frame_delim = (char)GetPrivateProfileInt("output", "frame_delimiter", 10, configPath);
    p->ascii_range = GetPrivateProfileInt("output", "ascii_max_range", 1000, configPath);
    p->bit_format = GetPrivateProfileInt("output", "bit_format", 16, configPath);

    p->sdl_width = GetPrivateProfileInt("output", "sdl_width", 1000, configPath);
    p->sdl_height = GetPrivateProfileInt("output", "sdl_height", 500, configPath);
    p->sdl_x = GetPrivateProfileInt("output", "sdl_x", -1, configPath);
    p->sdl_y = GetPrivateProfileInt("output", "sdl_y", -1, configPath);

    p->sync_updates = GetPrivateProfileInt("output", "alacritty_sync", 0, configPath);

    p->userEQ_enabled = 0;

    p->input = GetPrivateProfileInt("input", "method", INPUT_WINSCAP, configPath);
    if (p->input != INPUT_WINSCAP) {
        write_errorf(error, "cava was built without '%s' input support\n",
                     input_method_names[p->input]);
        return false;
    }
    GetPrivateProfileString("input", "source", "auto", p->audio_source, 64, configPath);

    if (strcmp(outputMethod, "sdl") == 0 || strcmp(outputMethod, "sdl_glsl") == 0) {
        p->bar_width = GetPrivateProfileInt("general", "bar_width", 20, configPath);
        p->bar_spacing = GetPrivateProfileInt("general", "bar_spacing", 5, configPath);
        GetPrivateProfileString("color", "foreground", "#33cccc", p->color, 9, configPath);
        GetPrivateProfileString("color", "background", "#111111", p->bcolor, 9, configPath);
    }

    p->continuous_rendering = GetPrivateProfileInt("output", "continuous_rendering", 0, configPath);

    GetPrivateProfileString("output", "vertex_shader", "pass_through.vert", vertexShader, 64,
                            configPath);
    GetPrivateProfileString("output", "fragment_shader", "bar_spectrum.frag", fragmentShader, 64,
                            configPath);

#endif
    sprintf(p->vertex_shader, "%s/shaders/%s", cava_config_home, vertexShader);
    sprintf(p->fragment_shader, "%s/shaders/%s", cava_config_home, fragmentShader);

    bool result = validate_config(p, error);

    return result;
}
