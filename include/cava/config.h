#pragma once

#include <limits.h>
#include <stdbool.h>

#ifdef _WIN32
#include <windows.h>
#endif

#define MAX_ERROR_LEN 1024

#ifndef PATH_MAX
#ifdef _WIN32
#define PATH_MAX 260
#elif defined(_WIN64)
#define PATH_MAX 260
#endif
#endif

// These are in order of least-favourable to most-favourable choices, in case
// multiple are supported and configured.
enum input_method {
    INPUT_FIFO,
    INPUT_PORTAUDIO,
    INPUT_PIPEWIRE,
    INPUT_ALSA,
    INPUT_PULSE,
    INPUT_SNDIO,
    INPUT_OSS,
    INPUT_JACK,
    INPUT_SHMEM,
    INPUT_WINSCAP,
    INPUT_MAX,
};

enum output_method {
    OUTPUT_NCURSES,
    OUTPUT_NONCURSES,
    OUTPUT_RAW,
    OUTPUT_SDL,
    OUTPUT_SDL_GLSL,
    OUTPUT_NORITAKE,
    OUTPUT_NOT_SUPORTED
};

enum mono_option { LEFT, RIGHT, AVERAGE };
enum data_format { FORMAT_ASCII = 0, FORMAT_BINARY = 1, FORMAT_NTK3000 = 2 };

enum xaxis_scale { NONE, FREQUENCY, NOTE };

enum orientation {
    ORIENT_BOTTOM,
    ORIENT_TOP,
    ORIENT_LEFT,
    ORIENT_RIGHT,
    ORIENT_SPLIT_H,
    ORIENT_SPLIT_V
};

struct config_params {
    char *color, *bcolor, *raw_target, *audio_source, **gradient_colors,
        **horizontal_gradient_colors, *data_format, *vertex_shader, *fragment_shader, *theme;

    char bar_delim, frame_delim;
    double monstercat, integral, gravity, ignore, sens, noise_reduction, max_height;

    unsigned int lower_cut_off, upper_cut_off;
    double *userEQ;
    enum input_method input;
    enum output_method output;
    enum xaxis_scale xaxis;
    enum mono_option mono_opt;
    enum orientation orientation;
    enum orientation blendDirection;
    int userEQ_keys, userEQ_enabled, col, bgcol, autobars, stereo, raw_format, ascii_range,
        bit_format, gradient, gradient_count, horizontal_gradient, horizontal_gradient_count,
        fixedbars, framerate, bar_width, bar_spacing, bar_height, autosens, overshoot, waves,
        active, remix, virtual_node, samplerate, samplebits, channels, autoconnect, sleep_timer,
        sdl_width, sdl_height, sdl_x, sdl_y, sdl_full_screen, draw_and_quit, zero_test,
        non_zero_test, reverse, sync_updates, continuous_rendering, disable_blanking,
        show_idle_bar_heads, waveform, center_align, inAtty, inAterminal, fp, x_axis_info;
#ifdef _WIN32
    HANDLE hFile;
#endif
};

struct error_s {
    char message[MAX_ERROR_LEN];
    int length;
};

enum input_method input_method_by_name(const char *str);

bool load_config(char configPath[PATH_MAX], struct config_params *p, struct error_s *error);
bool load_colors(char *themeFile, struct config_params *p, struct error_s *error);
void free_config(struct config_params *p);
bool get_themeFile(char configPath[PATH_MAX], struct config_params *p, char *cava_config_home,
                   struct error_s *error, char **themeFile);
