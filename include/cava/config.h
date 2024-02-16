#pragma once

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define MAX_ERROR_LEN 1024

#ifdef PORTAUDIO
#define HAS_PORTAUDIO true
#else
#define HAS_PORTAUDIO false
#endif

#ifdef ALSA
#define HAS_ALSA true
#else
#define HAS_ALSA false
#endif

#ifdef PULSE
#define HAS_PULSE true
#else
#define HAS_PULSE false
#endif

#ifdef PIPEWIRE
#define HAS_PIPEWIRE true
#else
#define HAS_PIPEWIRE false
#endif

#ifdef SNDIO
#define HAS_SNDIO true
#else
#define HAS_SNDIO false
#endif

#ifdef OSS
#define HAS_OSS true
#else
#define HAS_OSS false
#endif

#ifdef JACK
#define HAS_JACK true
#else
#define HAS_JACK false
#endif

#ifdef _MSC_VER
#define HAS_WINSCAP true
#define SDL true
#define HAS_FIFO false
#define HAS_SHMEM false
#define PATH_MAX 260
#else
#define HAS_WINSCAP false
#define HAS_FIFO true
#define HAS_SHMEM true

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

enum orientation { ORIENT_BOTTOM, ORIENT_TOP, ORIENT_LEFT, ORIENT_RIGHT };

struct config_params {
    char *color, *bcolor, *raw_target, *audio_source,
        /**gradient_color_1, *gradient_color_2,*/ **gradient_colors, *data_format, *vertex_shader,
        *fragment_shader;

    char bar_delim, frame_delim;
    double monstercat, integral, gravity, ignore, sens, noise_reduction;
    unsigned int lower_cut_off, upper_cut_off;
    double *userEQ;
    enum input_method input;
    enum output_method output;
    enum xaxis_scale xaxis;
    enum mono_option mono_opt;
    enum orientation orientation;
    int userEQ_keys, userEQ_enabled, col, bgcol, autobars, stereo, raw_format, ascii_range,
        bit_format, gradient, gradient_count, fixedbars, framerate, bar_width, bar_spacing,
        bar_height, autosens, overshoot, waves, samplerate, samplebits, channels, autoconnect,
        sleep_timer, sdl_width, sdl_height, sdl_x, sdl_y, sdl_full_screen, draw_and_quit, zero_test,
        non_zero_test, reverse, sync_updates, continuous_rendering, disable_blanking,
        show_idle_bar_heads, waveform, inAtty, fp, x_axis_info;
};

struct error_s {
    char message[MAX_ERROR_LEN];
    int length;
};
bool load_config(char configPath[PATH_MAX], struct config_params *p, bool colorsOnly,
                 struct error_s *error);
enum input_method input_method_by_name(const char *str);
