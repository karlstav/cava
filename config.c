#include "config.h"

#include "util.h"

#include <ctype.h>
#ifndef _WIN32
#include <iniparser.h>
#endif
#include <math.h>

#ifdef SNDIO
#include <sndio.h>
#endif

#include <stdarg.h>
#include <stdbool.h>
#include <stdlib.h>

#include <sys/stat.h>

#define NUMBER_OF_SHADERS 6

#define NUMBER_OF_THEMES 2

#ifdef _WIN32
#include "Shlwapi.h"
#include "Windows.h"
#define TEXTFILE 256
#define IDR_CONFIG_FILE 101
#define IDR_BAR_SPECTRUM_SHADER 102
#define IDR_NORTHERN_LIGHTS_SHADER 103
#define IDR_PASS_THROUGH_SHADER 104
#define IDR_SPECTROGRAM_SHADER 105
#define IDR_WINAMP_LINE_STYLE_SPECTRUM_SHADER 106
#define IDR_EYE_OF_PHI_SHADER 107

#define IDR_SOLARIZED_DARK_THEME 501
#define IDR_TRICOLOR_THEME 502

#define PATH_MAX 260
#define PACKAGE "cava"
#define _CRT_SECURE_NO_WARNINGS 1

static void LoadFileInResource(int name, int type, DWORD *size, const char **data) {
    HMODULE handle = GetModuleHandle(NULL);
    HRSRC rc = FindResource(handle, MAKEINTRESOURCE(name), MAKEINTRESOURCE(type));
    HGLOBAL rcData = LoadResource(handle, rc);
    if (size) {
        *size = SizeofResource(handle, rc);
    }
    if (data) {
        *data = (const char *)(LockResource(rcData));
    }
}

int default_shader_data[NUMBER_OF_SHADERS] = {
    IDR_NORTHERN_LIGHTS_SHADER, IDR_PASS_THROUGH_SHADER,
    IDR_BAR_SPECTRUM_SHADER,    IDR_WINAMP_LINE_STYLE_SPECTRUM_SHADER,
    IDR_SPECTROGRAM_SHADER,     IDR_EYE_OF_PHI_SHADER};

int default_theme_data[NUMBER_OF_THEMES] = {IDR_SOLARIZED_DARK_THEME, IDR_TRICOLOR_THEME};
#else
#define INCBIN_SILENCE_BITCODE_WARNING
#include "third_party/incbin.h"

INCTXT(ConfigFile, "example_files/config");

// add your custom shaders to be installed here
INCTXT(bar_spectrum, "output/shaders/bar_spectrum.frag");
INCTXT(northern_lightsfrag, "output/shaders/northern_lights.frag");
INCTXT(winamp_line_style_spectrum, "output/shaders/winamp_line_style_spectrum.frag");
INCTXT(spectrogram, "output/shaders/spectrogram.frag");
INCTXT(eye_of_phi, "output/shaders/eye_of_phi.frag");

INCTXT(pass_throughvert, "output/shaders/pass_through.vert");

INCTXT(solarized_dark, "output/themes/solarized_dark");
INCTXT(tricolor, "output/themes/tricolor");

// INCTXT will create a char g<name>Data
const char *default_shader_data[NUMBER_OF_SHADERS] = {
    gnorthern_lightsfragData,        gpass_throughvertData, gbar_spectrumData,
    gwinamp_line_style_spectrumData, gspectrogramData,      geye_of_phiData};

const char *default_theme_data[NUMBER_OF_THEMES] = {gsolarized_darkData, gtricolorData};
#endif // _WIN32

// name of the installed shader file, technically does not have to be the same as in the source
const char *default_shader_name[NUMBER_OF_SHADERS] = {
    "northern_lights.frag", "pass_through.vert",
    "bar_spectrum.frag",    "winamp_line_style_spectrum.frag",
    "spectrogram.frag",     "eye_of_phi.frag"};
const char *default_theme_name[NUMBER_OF_THEMES] = {"solarized_dark", "tricolor"};

double smoothDef[5] = {1, 1, 1, 1, 1};

enum input_method default_methods[] = {
    INPUT_FIFO,  INPUT_PORTAUDIO, INPUT_ALSA,    INPUT_SNDIO, INPUT_JACK,
    INPUT_PULSE, INPUT_PIPEWIRE,  INPUT_WINSCAP, INPUT_OSS,
};

char *outputMethod, *orientation, *channels, *xaxisScale, *monoOption, *fragmentShader,
    *vertexShader, *blendDirection;

const char *input_method_names[] = {
    "fifo", "portaudio", "pipewire", "alsa", "pulse", "sndio", "oss", "jack", "shmem", "winscap",
};

const bool has_input_method[] = {
    HAS_FIFO, /** Always have at least FIFO and shmem input. */
    HAS_PORTAUDIO, HAS_PIPEWIRE, HAS_ALSA,  HAS_PULSE,   HAS_SNDIO,
    HAS_OSS,       HAS_JACK,     HAS_SHMEM, HAS_WINSCAP,
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
        if (p->gradient_count < 2) {
            write_errorf(error, "\nAt least two colors must be given as gradient!\n");
            return false;
        }
        if (p->gradient_count > 8) {
            write_errorf(error, "\nMaximum 8 colors can be specified as gradient!\n");
            return false;
        }

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

    if (p->horizontal_gradient) {
        if (p->horizontal_gradient_count < 2) {
            write_errorf(error, "\nAt least two colors must be given as gradient!\n");
            return false;
        }
        if (p->horizontal_gradient_count > 8) {
            write_errorf(error, "\nMaximum 8 colors can be specified as gradient!\n");
            return false;
        }

        for (int i = 0; i < p->horizontal_gradient_count; i++) {
            if (!validate_color(p->horizontal_gradient_colors[i], p, error)) {
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
        write_errorf(error, "cava was built without opengl support, install opengl dev files "
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

    p->blendDirection = ORIENT_BOTTOM;
    if (strcmp(blendDirection, "down") == 0) {
        p->blendDirection = ORIENT_TOP;
    }
    if (strcmp(blendDirection, "left") == 0) {
        p->blendDirection = ORIENT_LEFT;
    }
    if (strcmp(blendDirection, "right") == 0) {
        p->blendDirection = ORIENT_RIGHT;
    }

    p->orientation = ORIENT_BOTTOM;
    if (strcmp(orientation, "top") == 0) {
        p->orientation = ORIENT_TOP;
    }
    if (strcmp(orientation, "left") == 0) {
        p->orientation = ORIENT_LEFT;
    }
    if (strcmp(orientation, "right") == 0) {
        p->orientation = ORIENT_RIGHT;
    }
    if (strcmp(orientation, "horizontal") == 0) {
        if (p->output != OUTPUT_NONCURSES) {
            write_errorf(error, "only noncurses output supports horizontal orientation\n");
            return false;
        }
        p->orientation = ORIENT_SPLIT_H;
    }
    if ((p->orientation == ORIENT_LEFT || p->orientation == ORIENT_RIGHT) &&
        !(p->output == OUTPUT_SDL || p->output == OUTPUT_NCURSES)) {
        write_errorf(error, "only ncurses and sdl supports left/right orientation\n");
        return false;
    }
    if ((p->orientation == ORIENT_TOP) &&
        !(p->output == OUTPUT_SDL || p->output == OUTPUT_NCURSES ||
          p->output == OUTPUT_NONCURSES)) {
        write_errorf(error, "only noncurses, ncurses and sdl supports top orientation\n");
        return false;
    }

    if ((p->orientation != ORIENT_BOTTOM && p->output == OUTPUT_SDL &&
         (p->gradient != 0 || p->horizontal_gradient != 0))) {
        write_errorf(error,
                     "gradient in sdl is not supported with top, left or right orientation\n");
        return false;
    }

    if (strcmp(fragmentShader, "eye_of_phi.frag") == 0) {
        p->fixedbars = 8;
        p->stereo = 0;
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
    }
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
    if (strcmp(channels, "stereo") == 0)
        p->stereo = 1;
    if (p->stereo == -1) {
        write_errorf(
            error,
            "output channels %s is not supported, supported channels are: 'mono' and 'stereo'\n",
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
    if (p->sens < 1) {
        write_errorf(error, "Sensitivity needs to be at least 1%%\n");
        return false;
    }
    p->sens = p->sens / 100;

    // validate: channels
    if (p->channels <= 1)
        p->channels = 1;
    else
        p->channels = 2;

    if (p->max_height > 1.0) {
        write_errorf(error, "max_height is defined in percent and can't be more than 100\n");
        return false;
    } else if (p->max_height < 0.0) {
        write_errorf(error, "max_height can't be negative\n");
        return false;
    }

    return validate_colors(p, error);
}

bool load_config(char configPath[PATH_MAX], struct config_params *p, bool colorsOnly,
                 struct error_s *error, int reload) {
    FILE *fp;
    char *cava_config_home = malloc(PATH_MAX / 2);

    // config: creating path to default config file
    char *configHome = getenv("XDG_CONFIG_HOME");
    if (configHome != NULL) {
        sprintf(cava_config_home, "%s/%s/", configHome, PACKAGE);
#ifndef _WIN32
        mkdir(cava_config_home, 0777);
#else
        CreateDirectoryA(cava_config_home, NULL);
#endif
    } else {
#ifndef _WIN32
        configHome = getenv("HOME");
#else
        configHome = getenv("USERPROFILE");
#endif

        if (configHome != NULL) {
            sprintf(cava_config_home, "%s/%s/", configHome, ".config");
#ifndef _WIN32
            mkdir(cava_config_home, 0777);
#else
            CreateDirectoryA(cava_config_home, NULL);
#endif

            sprintf(cava_config_home, "%s/%s/%s/", configHome, ".config", PACKAGE);
#ifndef _WIN32
            mkdir(cava_config_home, 0777);
#else
            CreateDirectoryA(cava_config_home, NULL);
#endif
        } else {
            write_errorf(error, "No HOME found (ERR_HOMELESS), exiting...");
            return false;
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
                printf("config file is empty, creating default config file\n");
#ifdef _WIN32
                DWORD gConfigFileSize = 0;
                const char *gConfigFileData = NULL;
                LoadFileInResource(IDR_CONFIG_FILE, TEXTFILE, &gConfigFileSize, &gConfigFileData);
                gConfigFileSize += 1;
#endif
                fwrite(gConfigFileData, gConfigFileSize - 1, sizeof(char), fp);
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
#ifdef _WIN32
        // GetPrivateProfileString does not accept relative paths
        if (PathIsRelativeA(configPath) == TRUE) {
            char newPath[PATH_MAX];
            GetCurrentDirectoryA(PATH_MAX - 1 - strlen(configPath), newPath);
            strcat(newPath, "\\");
            strcat(newPath, configPath);
            memset(configPath, 0, sizeof(configPath));
            strcpy(configPath, newPath);
        }
#endif
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
#ifndef _WIN32
    mkdir(shaderPath, 0777);
#else
    CreateDirectoryA(shaderPath, NULL);
#endif

    for (int i = 0; i < NUMBER_OF_SHADERS; i++) {
        char *shaderFile = malloc(sizeof(char) * PATH_MAX);
        sprintf(shaderFile, "%s/%s", shaderPath, default_shader_name[i]);

        fp = fopen(shaderFile, "ab+");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            if (ftell(fp) == 0) {
                printf("shader file is empty, creating default shader file\n");
#ifndef _WIN32
                fwrite(default_shader_data[i], strlen(default_shader_data[i]), sizeof(char), fp);
#else
                DWORD shaderDataSize = 0;
                const char *shaderData = NULL;
                LoadFileInResource(default_shader_data[i], TEXTFILE, &shaderDataSize, &shaderData);
                fwrite(shaderData, shaderDataSize, sizeof(char), fp);
#endif
            }
            fclose(fp);
            free(shaderFile);
        }
    }
    free(shaderPath);

    // create default theme files if they do not exist
    char *themePath = malloc(sizeof(char) * PATH_MAX);
    sprintf(themePath, "%s/themes", cava_config_home);
#ifndef _WIN32
    mkdir(themePath, 0777);
#else
    CreateDirectoryA(themePath, NULL);
#endif
    for (int i = 0; i < NUMBER_OF_THEMES; i++) {
        char *themeFile = malloc(sizeof(char) * PATH_MAX);
        sprintf(themeFile, "%s/%s", themePath, default_theme_name[i]);

        fp = fopen(themeFile, "ab+");
        if (fp) {
            fseek(fp, 0, SEEK_END);
            if (ftell(fp) == 0) {
                printf("theme file is empty, creating default theme file\n");
#ifndef _WIN32
                fwrite(default_theme_data[i], strlen(default_theme_data[i]), sizeof(char), fp);
#else
                DWORD themeDataSize = 0;
                const char *themeData = NULL;
                LoadFileInResource(default_theme_data[i], TEXTFILE, &themeDataSize, &themeData);
                fwrite(themeData, themeDataSize, sizeof(char), fp);
#endif
            }
            fclose(fp);
            free(themeFile);
        }
    }
    free(themePath);

    free(p->vertex_shader);
    free(p->fragment_shader);
    p->vertex_shader = malloc(sizeof(char) * PATH_MAX);
    p->fragment_shader = malloc(sizeof(char) * PATH_MAX);

    char *themeFile = malloc(sizeof(char) * PATH_MAX);

#ifndef _WIN32
    // config: parse ini
    dictionary *ini;
    ini = iniparser_load(configPath);

    free(p->color);
    free(p->bcolor);

    p->theme = strdup(iniparser_getstring(ini, "color:theme", "none"));

    if (strcmp(p->theme, "none") != 0) {
        sprintf(themeFile, "%s/themes/%s", cava_config_home, p->theme);
        fp = fopen(themeFile, "rb");
        if (fp) {
            fclose(fp);
        } else {
            write_errorf(error, "Unable to open theme file '%s', exiting...\n", themeFile);
            return false;
        }
        iniparser_freedict(ini);
        ini = iniparser_load(themeFile);
    }

    p->color = strdup(iniparser_getstring(ini, "color:foreground", "default"));
    p->bcolor = strdup(iniparser_getstring(ini, "color:background", "default"));

    p->gradient = iniparser_getint(ini, "color:gradient", 0);

    if (reload) {
        for (int i = 0; i < 8; ++i)
            free(p->gradient_colors[i]);
    }
    free(p->gradient_colors);
    p->gradient_colors = (char **)malloc(sizeof(char *) * 8 * 9);

    p->gradient_colors[0] = strdup(iniparser_getstring(ini, "color:gradient_color_1", "not_set"));
    p->gradient_colors[1] = strdup(iniparser_getstring(ini, "color:gradient_color_2", "not_set"));
    p->gradient_colors[2] = strdup(iniparser_getstring(ini, "color:gradient_color_3", "not_set"));
    p->gradient_colors[3] = strdup(iniparser_getstring(ini, "color:gradient_color_4", "not_set"));
    p->gradient_colors[4] = strdup(iniparser_getstring(ini, "color:gradient_color_5", "not_set"));
    p->gradient_colors[5] = strdup(iniparser_getstring(ini, "color:gradient_color_6", "not_set"));
    p->gradient_colors[6] = strdup(iniparser_getstring(ini, "color:gradient_color_7", "not_set"));
    p->gradient_colors[7] = strdup(iniparser_getstring(ini, "color:gradient_color_8", "not_set"));

    p->horizontal_gradient = iniparser_getint(ini, "color:horizontal_gradient", 0);

    if (reload) {
        for (int i = 0; i < 8; ++i)
            free(p->horizontal_gradient_colors[i]);
    }
    free(p->horizontal_gradient_colors);
    p->horizontal_gradient_colors = (char **)malloc(sizeof(char *) * 8 * 9);

    p->horizontal_gradient_colors[0] =
        strdup(iniparser_getstring(ini, "color:horizontal_gradient_color_1", "not_set"));
    p->horizontal_gradient_colors[1] =
        strdup(iniparser_getstring(ini, "color:horizontal_gradient_color_2", "not_set"));
    p->horizontal_gradient_colors[2] =
        strdup(iniparser_getstring(ini, "color:horizontal_gradient_color_3", "not_set"));
    p->horizontal_gradient_colors[3] =
        strdup(iniparser_getstring(ini, "color:horizontal_gradient_color_4", "not_set"));
    p->horizontal_gradient_colors[4] =
        strdup(iniparser_getstring(ini, "color:horizontal_gradient_color_5", "not_set"));
    p->horizontal_gradient_colors[5] =
        strdup(iniparser_getstring(ini, "color:horizontal_gradient_color_6", "not_set"));
    p->horizontal_gradient_colors[6] =
        strdup(iniparser_getstring(ini, "color:horizontal_gradient_color_7", "not_set"));
    p->horizontal_gradient_colors[7] =
        strdup(iniparser_getstring(ini, "color:horizontal_gradient_color_8", "not_set"));

#else
    outputMethod = malloc(sizeof(char) * 32);
    p->color = malloc(sizeof(char) * 14);
    p->bcolor = malloc(sizeof(char) * 14);
    p->audio_source = malloc(sizeof(char) * 129);
    p->theme = malloc(sizeof(char) * 64);

    xaxisScale = malloc(sizeof(char) * 32);
    channels = malloc(sizeof(char) * 32);
    monoOption = malloc(sizeof(char) * 32);
    p->raw_target = malloc(sizeof(char) * 129);
    p->data_format = malloc(sizeof(char) * 32);
    orientation = malloc(sizeof(char) * 32);
    blendDirection = malloc(sizeof(char) * 32);
    vertexShader = malloc(sizeof(char) * PATH_MAX / 2);
    fragmentShader = malloc(sizeof(char) * PATH_MAX / 2);

    p->gradient_colors = (char **)malloc(sizeof(char *) * 8 * 9);
    for (int i = 0; i < 8; ++i) {
        p->gradient_colors[i] = (char *)malloc(sizeof(char *) * 9);
    }
    p->horizontal_gradient_colors = (char **)malloc(sizeof(char *) * 8 * 9);
    for (int i = 0; i < 8; ++i) {
        p->horizontal_gradient_colors[i] = (char *)malloc(sizeof(char *) * 9);
    }

    GetPrivateProfileString("color", "theme", "none", p->theme, 64, configPath);

    char *configFileBak = configPath;

    if (strcmp(p->theme, "none") != 0) {
        sprintf(themeFile, "%s/themes/%s", cava_config_home, p->theme);
        fp = fopen(themeFile, "rb");
        if (fp) {
            fclose(fp);
            configPath = themeFile;
        } else {
            write_errorf(error, "Unable to open theme file '%s', exiting...\n", themeFile);
            return false;
        }
    }

    GetPrivateProfileString("color", "foreground", "default", p->color, 9, configPath);
    GetPrivateProfileString("color", "background", "default", p->bcolor, 9, configPath);
    p->gradient = GetPrivateProfileInt("color", "gradient", 0, configPath);

    GetPrivateProfileString("color", "gradient_color_1", "not_set", p->gradient_colors[0], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_2", "not_set", p->gradient_colors[1], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_3", "not_set", p->gradient_colors[2], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_4", "not_set", p->gradient_colors[3], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_5", "not_set", p->gradient_colors[4], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_6", "not_set", p->gradient_colors[5], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_7", "not_set", p->gradient_colors[6], 9,
                            configPath);
    GetPrivateProfileString("color", "gradient_color_8", "not_set", p->gradient_colors[7], 9,
                            configPath);

    p->horizontal_gradient = GetPrivateProfileInt("color", "horizontal_gradient", 0, configPath);

    GetPrivateProfileString("color", "horizontal_gradient_color_1", "not_set",
                            p->horizontal_gradient_colors[0], 9, configPath);
    GetPrivateProfileString("color", "horizontal_gradient_color_2", "not_set",
                            p->horizontal_gradient_colors[1], 9, configPath);
    GetPrivateProfileString("color", "horizontal_gradient_color_3", "not_set",
                            p->horizontal_gradient_colors[2], 9, configPath);
    GetPrivateProfileString("color", "horizontal_gradient_color_4", "not_set",
                            p->horizontal_gradient_colors[3], 9, configPath);
    GetPrivateProfileString("color", "horizontal_gradient_color_5", "not_set",
                            p->horizontal_gradient_colors[4], 9, configPath);
    GetPrivateProfileString("color", "horizontal_gradient_color_6", "not_set",
                            p->horizontal_gradient_colors[5], 9, configPath);
    GetPrivateProfileString("color", "horizontal_gradient_color_7", "not_set",
                            p->horizontal_gradient_colors[6], 9, configPath);
    GetPrivateProfileString("color", "horizontal_gradient_color_8", "not_set",
                            p->horizontal_gradient_colors[7], 9, configPath);

    if (strcmp(p->theme, "none") != 0) {
        configPath = configFileBak;
    }
    free(p->theme);
#endif
    p->gradient_count = 0;
    for (int i = 0; i < 7; ++i) {
        if (strcmp(p->gradient_colors[i], "not_set") != 0)
            p->gradient_count++;
        else
            break;
    }
    p->horizontal_gradient_count = 0;
    for (int i = 0; i < 7; ++i) {
        if (strcmp(p->horizontal_gradient_colors[i], "not_set") != 0)
            p->horizontal_gradient_count++;
        else
            break;
    }

    free(themeFile);
#ifndef _WIN32
    if (strcmp(p->theme, "none") != 0) {
        iniparser_freedict(ini);
        ini = iniparser_load(configPath);
    }
    free(p->theme);
#endif

    if (colorsOnly) {
        return validate_colors(p, error);
    }

#ifndef _WIN32

    free(orientation);
    free(xaxisScale);
    free(outputMethod);

    outputMethod = strdup(iniparser_getstring(ini, "output:method", "noncurses"));
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
    int max_height = iniparser_getint(ini, "general:max_height", 100);
    p->max_height = max_height / 100.0;
    p->center_align = iniparser_getint(ini, "general:center_align", 1);
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
    free(blendDirection);

    channels = strdup(iniparser_getstring(ini, "output:channels", "stereo"));
    monoOption = strdup(iniparser_getstring(ini, "output:mono_option", "average"));
    p->reverse = iniparser_getint(ini, "output:reverse", 0);
    p->raw_target = strdup(iniparser_getstring(ini, "output:raw_target", "/dev/stdout"));
    p->data_format = strdup(iniparser_getstring(ini, "output:data_format", "binary"));
    p->bar_delim = (char)iniparser_getint(ini, "output:bar_delimiter", 59);
    p->frame_delim = (char)iniparser_getint(ini, "output:frame_delimiter", 10);
    p->ascii_range = iniparser_getint(ini, "output:ascii_max_range", 1000);
    p->bit_format = iniparser_getint(ini, "output:bit_format", 16);
    blendDirection = strdup(iniparser_getstring(ini, "color:blend_direction", "up"));

    p->sdl_width = iniparser_getint(ini, "output:sdl_width", 1024);
    p->sdl_height = iniparser_getint(ini, "output:sdl_height", 512);
    p->sdl_x = iniparser_getint(ini, "output:sdl_x", -1);
    p->sdl_y = iniparser_getint(ini, "output:sdl_y", -1);
    p->sdl_full_screen = iniparser_getint(ini, "output:sdl_full_screen", 0);

    if (strcmp(outputMethod, "sdl") == 0 || strcmp(outputMethod, "sdl_glsl") == 0) {
        free(p->color);
        free(p->bcolor);
        p->color = strdup(iniparser_getstring(ini, "color:foreground", "#33cccc"));
        p->bcolor = strdup(iniparser_getstring(ini, "color:background", "#111111"));
        p->bar_width = iniparser_getint(ini, "general:bar_width", 20);
        p->bar_spacing = iniparser_getint(ini, "general:bar_spacing", 5);
    }

    p->continuous_rendering = iniparser_getint(ini, "output:continuous_rendering", 0);

    p->disable_blanking = iniparser_getint(ini, "output:disable_blanking", 0);

    p->show_idle_bar_heads = iniparser_getint(ini, "output:show_idle_bar_heads", 1);

    p->waveform = iniparser_getint(ini, "output:waveform", 0);

    p->sync_updates = iniparser_getint(ini, "output:synchronized_sync", 0);

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

    enum input_method default_input = INPUT_FIFO;
    for (size_t i = 0; i < ARRAY_SIZE(default_methods); i++) {
        enum input_method method = default_methods[i];
        if (has_input_method[method]) {
            default_input = default_methods[i];
        }
    }
    char *input_method_name =
        strdup(iniparser_getstring(ini, "input:method", input_method_names[default_input]));
    p->input = input_method_by_name(input_method_name);

    if (p->input == input_method_by_name("pipewire"))
        p->samplerate = iniparser_getint(ini, "input:sample_rate", 48000);
    else
        p->samplerate = iniparser_getint(ini, "input:sample_rate", 44100);
    p->samplebits = iniparser_getint(ini, "input:sample_bits", 16);
    p->channels = iniparser_getint(ini, "input:channels", 2);
    p->autoconnect = iniparser_getint(ini, "input:autoconnect", 2);
    p->active = iniparser_getint(ini, "input:active", 0);
    p->remix = iniparser_getint(ini, "input:remix", 1);
    p->virtual = iniparser_getint(ini, "input:virtual", 1);

    switch (p->input) {
#ifdef ALSA
    case INPUT_ALSA:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "hw:Loopback,1"));
        break;
#endif
    case INPUT_FIFO:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "/tmp/mpd.fifo"));
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
#ifdef OSS
    case INPUT_OSS:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "/dev/dsp"));
        break;
#endif
#ifdef JACK
    case INPUT_JACK:
        p->audio_source = strdup(iniparser_getstring(ini, "input:source", "default"));
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

    p->noise_reduction = GetPrivateProfileInt("smoothing", "noise_reduction", 70, configPath);
    GetPrivateProfileString("output", "xaxis", "none", xaxisScale, 16, configPath);
    GetPrivateProfileString("output", "orientation", "bottom", orientation, 16, configPath);
    GetPrivateProfileString("color", "blend_orientation", "up", orientation, 16, configPath);

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
    p->max_height = GetPrivateProfileInt("general", "max_height", 100, configPath) / 100.0;
    p->center_align = GetPrivateProfileInt("general", "center_align", 1, configPath);

    GetPrivateProfileString("output", "channels", "stereo", channels, 16, configPath);
    GetPrivateProfileString("output", "mono_option", "average", monoOption, 16, configPath);
    p->reverse = GetPrivateProfileInt("output", "reverse", 0, configPath);
    GetPrivateProfileString("output", "raw_target", "/dev/stdout", p->raw_target, 64, configPath);
    GetPrivateProfileString("output", "data_format", "binary", p->data_format, 16, configPath);
    p->bar_delim = (char)GetPrivateProfileInt("output", "bar_delimiter", 59, configPath);
    p->frame_delim = (char)GetPrivateProfileInt("output", "frame_delimiter", 10, configPath);
    p->ascii_range = GetPrivateProfileInt("output", "ascii_max_range", 1000, configPath);
    p->bit_format = GetPrivateProfileInt("output", "bit_format", 16, configPath);

    p->sdl_width = GetPrivateProfileInt("output", "sdl_width", 1000, configPath);
    p->sdl_height = GetPrivateProfileInt("output", "sdl_height", 500, configPath);
    p->sdl_x = GetPrivateProfileInt("output", "sdl_x", -1, configPath);
    p->sdl_y = GetPrivateProfileInt("output", "sdl_y", -1, configPath);

    p->sync_updates = GetPrivateProfileInt("output", "synchronized_sync", 0, configPath);
    p->show_idle_bar_heads = GetPrivateProfileInt("output", "show_idle_bar_heads", 1, configPath);
    p->waveform = GetPrivateProfileInt("output", "waveform", 0, configPath);

    // read eq values
    p->userEQ_keys = 0;
    p->userEQ = (double *)malloc(sizeof(double));
    if (p->userEQ == NULL) {
        write_errorf(error, "Memory allocation failed\n");
        return false;
    }
    while (1) {
        char eqResult[10];
        char keyNum[3];

        // pass key counter as string to GetPrivateProfileString
        sprintf(keyNum, "%d", p->userEQ_keys + 1);
        GetPrivateProfileString("eq", keyNum, "NOT FOUND", eqResult, sizeof(eqResult), configPath);
        if (!strcmp(eqResult, "NOT FOUND")) {
            break;
        } else {
            double *oldPtr = p->userEQ;
            p->userEQ = (double *)realloc(p->userEQ, sizeof(double) * (p->userEQ_keys + 1));
            if (p->userEQ == NULL) {
                write_errorf(error, "Memory reallocation failed\n");
                free(oldPtr);
                return false;
            }

            char *endptr;
            p->userEQ[p->userEQ_keys] = strtod(eqResult, &endptr);
            if (endptr == eqResult) {
                write_errorf(error, "Invalid string to double conversion, %d : \"%s\" \n",
                             p->userEQ_keys + 1, eqResult);
                free(p->userEQ);
                return false;
            }
            p->userEQ_keys++;
        }
    }
    if (p->userEQ_keys > 0) {
        p->userEQ_enabled = 1;
    }

    p->input = GetPrivateProfileInt("input", "method", INPUT_WINSCAP, configPath);
    if (p->input != INPUT_WINSCAP) {
        write_errorf(error, "on windows changing input method is not supported, "
                            "simply leave the input method setting commented out\n");
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
    free(cava_config_home);

    return result;
}
