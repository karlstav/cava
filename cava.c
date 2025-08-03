#include <locale.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#else
#include <stdlib.h>
#endif

#include <fcntl.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.1415926535897932385
#endif

#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <ctype.h>
#include <dirent.h>
#include <getopt.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

#ifdef _WIN32
#include "input/winscap.h"
#include <windows.h>
#define PATH_MAX 260
#define PACKAGE "cava"
#define VERSION "0.10.4"
#define _CRT_SECURE_NO_WARNINGS 1
#endif // _WIN32

#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "cavacore.h"

#include "config.h"

#include "debug.h"
#include "util.h"

#ifdef SDL
#include "output/sdl_cava.h"
#endif

#ifdef SDL_GLSL
#include "output/sdl_glsl.h"
#endif

#include "input/common.h"

#include "output/noritake.h"
#include "output/raw.h"
#include "output/terminal_noncurses.h"

#ifndef _WIN32
#ifdef NCURSES
#include "output/terminal_bcircle.h"
#include "output/terminal_ncurses.h"
#include <curses.h>
#endif

#include "input/alsa.h"
#include "input/fifo.h"
#include "input/jack.h"
#include "input/oss.h"
#include "input/pipewire.h"
#include "input/portaudio.h"
#include "input/pulse.h"
#include "input/shmem.h"
#include "input/sndio.h"
#endif

#ifdef __GNUC__
// curses.h or other sources may already define
#undef GCC_UNUSED
#define GCC_UNUSED __attribute__((unused))
#else
#define GCC_UNUSED /* nothing */
#endif

#ifdef _WIN32
char *optarg = NULL;
int optind = 1;

static int getopt(int argc, char *const argv[], const char *optstring) {
    if ((optind >= argc) || (argv[optind][0] != '-') || (argv[optind][0] == 0)) {
        return -1;
    }

    int opt = argv[optind][1];
    const char *p = strchr(optstring, opt);

    if (p == NULL) {
        return '?';
    }
    if (p[1] == ':') {
        optind++;
        if (optind >= argc) {
            return '?';
        }
        optarg = argv[optind];
        optind++;
    }
    return opt;
}
#endif

// used by sig handler
// needs to know output mode in order to clean up terminal
int output_mode;
// whether we should reload the config or not
int should_reload = 0;
// whether we should only reload colors or not
int reload_colors = 0;
// whether we should quit
int should_quit = 0;
int signal_received = 0;

// these variables are used only in main, but making them global
// will allow us to not free them on exit without ASan complaining
struct config_params p;

// general: cleanup
void cleanup(void) {
    if (output_mode == OUTPUT_NCURSES) {
#ifdef NCURSES
        cleanup_terminal_ncurses();
#else
        ;
#endif
    } else if (output_mode == OUTPUT_NONCURSES) {
        cleanup_terminal_noncurses();
    } else if (output_mode == OUTPUT_SDL) {
#ifdef SDL
        cleanup_sdl();
#else
        ;
#endif
#ifdef SDL_GLSL
    } else if (output_mode == OUTPUT_SDL_GLSL) {
        cleanup_sdl_glsl();
#else
        ;
#endif
    }
}

// general: handle signals
#ifdef _WIN32
int sig_handler(DWORD sig_no) {
#else
void sig_handler(int sig_no) {
#endif
#ifndef _WIN32

    if (sig_no == SIGUSR1) {
        should_reload = 1;
        return;
    }

    if (sig_no == SIGUSR2) {
        reload_colors = 1;
        return;
    }
#endif

#ifdef _WIN32
    if (sig_no == CTRL_C_EVENT || sig_no == CTRL_CLOSE_EVENT) {
        sig_no = SIGINT;
    } else {
        return TRUE;
    }
#endif
    if (sig_no == SIGINT || sig_no == SIGTERM) {
        should_reload = 1;
        should_quit = 1;
    }

    signal_received = sig_no;

#ifdef _WIN32
    return TRUE;
#endif
}

#ifdef ALSA
static bool is_loop_device_for_sure(const char *text) {
    const char *const LOOPBACK_DEVICE_PREFIX = "hw:Loopback,";
    return strncmp(text, LOOPBACK_DEVICE_PREFIX, strlen(LOOPBACK_DEVICE_PREFIX)) == 0;
}

static bool directory_exists(const char *path) {
    DIR *const dir = opendir(path);
    if (dir == NULL)
        return false;

    closedir(dir);
    return true;
}

#endif

float *monstercat_filter(float *bars, int number_of_bars, int waves, double monstercat,
                         int height) {

    int z;

    // process [smoothing]: monstercat-style "average"

    int m_y, de;
    float height_normalizer = 1.0;
    if (height > 1000) {
        height_normalizer = height / 912.76;
    }
    if (waves > 0) {
        for (z = 0; z < number_of_bars; z++) { // waves
            bars[z] = bars[z] / 1.25;
            // if (bars[z] < 1) bars[z] = 1;
            for (m_y = z - 1; m_y >= 0; m_y--) {
                de = z - m_y;
                bars[m_y] = max(bars[z] - height_normalizer * pow(de, 2), bars[m_y]);
            }
            for (m_y = z + 1; m_y < number_of_bars; m_y++) {
                de = m_y - z;
                bars[m_y] = max(bars[z] - height_normalizer * pow(de, 2), bars[m_y]);
            }
        }
    } else if (monstercat > 0) {
        for (z = 0; z < number_of_bars; z++) {
            // if (bars[z] < 1)bars[z] = 1;
            for (m_y = z - 1; m_y >= 0; m_y--) {
                de = z - m_y;
                bars[m_y] = max(bars[z] / pow(monstercat * 1.5, de), bars[m_y]);
            }
            for (m_y = z + 1; m_y < number_of_bars; m_y++) {
                de = m_y - z;
                bars[m_y] = max(bars[z] / pow(monstercat * 1.5, de), bars[m_y]);
            }
        }
    }
    return bars;
}

// general: entry point
int main(int argc, char **argv) {

#ifndef _WIN32
    // general: console title
    printf("%c]0;%s%c", '\033', PACKAGE, '\007');
#endif // !_WIN32

    // general: handle command-line arguments
    char configPath[PATH_MAX];
    configPath[0] = '\0';
#ifdef _WIN32
    if (!SetConsoleCtrlHandler(sig_handler, TRUE)) {
        fprintf(stderr, "ERROR: Could not set control handler");
        exit(EXIT_FAILURE);
    }
#else
    // general: handle Ctrl+C
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sig_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);
#endif
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
        o         Change orientation bottom -> right -> top -> left (ncurses), top <-> bottom (other modes)\n\
        q         Quit\n\
\n";
    int c;
    while ((c = getopt(argc, argv, "p:vh")) != -1) {
        switch (c) {
        case 'p': // argument: config path
            snprintf(configPath, sizeof(configPath), "%s", optarg);
            break;
        case 'h': // argument: print usage
            printf("%s", usage);
            return 1;
        case '?': // argument: print usage
            printf("%s", usage);
            return 1;
        case 'v': // argument: print version
            printf(PACKAGE " " VERSION "\n");
            return 0;
        default: // argument: no arguments; exit
            abort();
        }
    }
    int reload = 0;

    // general: main loop
    while (1) {

        debug("loading config\n");
        // config: load
        struct error_s error;
        error.length = 0;
        if (!load_config(configPath, &p, 0, &error, reload)) {
            fprintf(stderr, "Error loading config. %s", error.message);
            exit(EXIT_FAILURE);
        }

        int inAtty = 0;
        int inAterminal = 0;

        output_mode = p.output;
#ifndef _WIN32
        if (output_mode == OUTPUT_NCURSES || output_mode == OUTPUT_NONCURSES) {
            // Check if we're running in a tty
            if (strncmp(ttyname(0), "/dev/tty", 8) == 0 ||
                strcmp(ttyname(0), "/dev/console") == 0 ||
                strncmp(ttyname(0), "/dev/ttyS", 9) == 0 ||
                strncmp(ttyname(0), "/dev/ttyUSB", 11) == 0)
                inAtty = 1;

            // Check if we are running in a dumb terminal
            if (strncmp(ttyname(0), "/dev/ttyS", 9) == 0 ||
                strncmp(ttyname(0), "/dev/ttyUSB", 11) == 0)
                inAterminal = 1;

            // in macos vitual terminals are called ttys(xyz) and there are no ttys
            if (strncmp(ttyname(0), "/dev/ttys", 9) == 0)
                inAtty = 0;

            if (inAtty) {
                if (inAterminal) {
                    printf("\033P 1;32;1;0;0;2 { sp @ "
                           "\?\?\?\?\?\?\?\?/GGGGGGGG;"
                           "\?\?\?\?\?\?\?\?/GGGGGGGG;"
                           "\?\?\?\?\?\?\?\?/KKKKKKKK;"
                           "\?\?\?\?\?\?\?\?/MMMMMMMM;"
                           "\?\?\?\?\?\?\?\?/NNNNNNNN;"
                           "oooooooo/NNNNNNNN;"
                           "wwwwwwww/NNNNNNNN;"
                           "}}}}}}}}/NNNNNNNN;"
                           "~~~~~~~~/NNNNNNNN");

                    printf("\033( sp @ ");
                } else {
#ifdef CAVAFONT
                    // checking if cava psf font is installed in FONTDIR
                    FILE *font_file;
                    font_file = fopen(FONTDIR "/" FONTFILE, "r");
                    if (font_file) {
                        fclose(font_file);
#ifdef __FreeBSD__
                        system("vidcontrol -f " FONTDIR "/" FONTFILE " >/dev/null 2>&1");
#else
                        system("setfont " FONTDIR "/" FONTFILE " >/dev/null 2>&1");
#endif
                    } else {
                        // if not it might still be available, we dont know, must try
#ifdef __FreeBSD__
                        system("vidcontrol -f " FONTFILE " >/dev/null 2>&1");
#else
                        system("setfont " FONTFILE " >/dev/null 2>&1");
#endif
                    }
#endif // CAVAFONT
#ifndef __FreeBSD__
                    if (p.disable_blanking)
                        system("setterm -blank 0");
#endif
                }
                if (p.orientation != ORIENT_BOTTOM) {
                    cleanup();
                    fprintf(stderr, "only default bottom orientation is supported in tty\n");
                    exit(EXIT_FAILURE);
                }
            }

            // We use unicode block characters to draw the bars and
            // the locale var LANG must be set to use unicode chars.
            // For some reason this var can't be retrieved with
            // setlocale(LANG, NULL), so we get it with getenv.
            // Also we can't set it with setlocale(LANG "") so we
            // must set LC_ALL instead.
            // Attempting to set to en_US if not set, if that lang
            // is not installed and LANG is not set there will be
            // no output, for more info see #109 #344
            if (!getenv("LANG"))
                setlocale(LC_ALL, "en_US.utf8");
            else
                setlocale(LC_ALL, "");
        }
#endif
        // input: init

        struct audio_data audio;
        memset(&audio, 0, sizeof(audio));

        audio.source = malloc(1 + strlen(p.audio_source));
        strcpy(audio.source, p.audio_source);

        audio.format = -1;
        audio.rate = 0;
        audio.samples_counter = 0;
        audio.channels = 2;
        audio.IEEE_FLOAT = 0;
        audio.autoconnect = 0;

        audio.input_buffer_size = BUFFER_SIZE * audio.channels;
        audio.cava_buffer_size = 16384; // this is the size at rates of 44100 or 48k, will be
                                        // adjusted later if sample rate is unusual

        audio.cava_in = (double *)malloc(audio.cava_buffer_size * sizeof(double));
        memset(audio.cava_in, 0, sizeof(int) * audio.cava_buffer_size);

        audio.threadparams = 0; // most input threads don't adjust the parameters
        audio.terminate = 0;

        debug("starting audio thread\n");

        pthread_t p_thread;
        int timeout_counter = 0;
        int total_bar_height = 0;

        struct timespec timeout_timer = {.tv_sec = 0, .tv_nsec = 1000000};
        int thr_id GCC_UNUSED;

        pthread_mutex_init(&audio.lock, NULL);

        switch (p.input) {
#ifndef _WIN32

#ifdef ALSA
        case INPUT_ALSA:
            if (is_loop_device_for_sure(audio.source)) {
                if (directory_exists("/sys/")) {
                    if (!directory_exists("/sys/module/snd_aloop/")) {
                        cleanup();
                        fprintf(stderr,
                                "Linux kernel module \"snd_aloop\" does not seem to  be loaded.\n"
                                "Maybe run \"sudo modprobe snd_aloop\".\n");
                        exit(EXIT_FAILURE);
                    }
                }
            }

            thr_id = pthread_create(&p_thread, NULL, input_alsa, (void *)&audio);
            break;
#endif

        case INPUT_FIFO:
            audio.rate = p.samplerate;
            audio.format = p.samplebits;
            thr_id = pthread_create(&p_thread, NULL, input_fifo, (void *)&audio);
            break;
#ifdef PULSE
        case INPUT_PULSE:
            audio.format = 16;
            audio.rate = 44100;
            if (strcmp(audio.source, "auto") == 0) {
                getPulseDefaultSink((void *)&audio);
            }
            thr_id = pthread_create(&p_thread, NULL, input_pulse, (void *)&audio);
            break;
#endif
#ifdef SNDIO
        case INPUT_SNDIO:
            audio.format = p.samplebits;
            audio.rate = p.samplerate;
            audio.channels = p.channels;
            audio.threadparams = 1; // Sndio can adjust parameters
            thr_id = pthread_create(&p_thread, NULL, input_sndio, (void *)&audio);
            break;
#endif
#ifdef OSS
        case INPUT_OSS:
            audio.format = p.samplebits;
            audio.rate = p.samplerate;
            audio.channels = p.channels;
            audio.threadparams = 1; // OSS can adjust parameters
            thr_id = pthread_create(&p_thread, NULL, input_oss, (void *)&audio);
            break;
#endif
#ifdef JACK
        case INPUT_JACK:
            audio.channels = p.channels;
            audio.autoconnect = p.autoconnect;
            audio.threadparams = 1; // JACK server provides parameters
            thr_id = pthread_create(&p_thread, NULL, input_jack, (void *)&audio);
            break;
#endif
        case INPUT_SHMEM:
            audio.format = 16;
            thr_id = pthread_create(&p_thread, NULL, input_shmem, (void *)&audio);
            break;
#ifdef PORTAUDIO
        case INPUT_PORTAUDIO:
            audio.format = 16;
            audio.rate = 44100;
            audio.threadparams = 1;
            if (!strcmp(audio.source, "list")) {
                input_portaudio((void *)&audio);
            } else {
                thr_id = pthread_create(&p_thread, NULL, input_portaudio, (void *)&audio);
            }
            break;
#endif
#ifdef PIPEWIRE
        case INPUT_PIPEWIRE:
            audio.format = p.samplebits;
            audio.rate = p.samplerate;
            audio.channels = p.channels;
            audio.active = p.active;
            audio.remix = p.remix;
            audio.virtual = p.virtual;
            thr_id = pthread_create(&p_thread, NULL, input_pipewire, (void *)&audio);
            break;
#endif
#endif
#ifdef _WIN32
        case INPUT_WINSCAP:
            thr_id = pthread_create(&p_thread, NULL, input_winscap, (void *)&audio);
            break;
#endif
        default:
            exit(EXIT_FAILURE); // Can't happen.
        }

        timeout_counter = 0;
        while (true) {
#ifdef _WIN32
            Sleep(1);
#else
            nanosleep(&timeout_timer, NULL);
#endif
            pthread_mutex_lock(&audio.lock);
            if ((audio.threadparams == 0) && (audio.format != -1) && (audio.rate != 0))
                break;

            pthread_mutex_unlock(&audio.lock);
            timeout_counter++;
            if (timeout_counter > 5000) {
                cleanup();
                fprintf(stderr, "could not get rate and/or format, problems with audio thread? "
                                "quitting...\n");
                exit(EXIT_FAILURE);
            }
        }
        pthread_mutex_unlock(&audio.lock);
        debug("got format: %d and rate %d\n", audio.format, audio.rate);

        int audio_channels = audio.channels;

        if (p.upper_cut_off > audio.rate / 2) {
            cleanup();
            fprintf(stderr,
                    "higher cutoff frequency can't be higher than sample rate / 2\nhigher "
                    "cutoff frequency is set to: %d, got sample rate: %d\n",
                    p.upper_cut_off, audio.rate);
            exit(EXIT_FAILURE);
        }

        // force stereo if only one channel is available
        if (p.stereo && audio_channels == 1)
            p.stereo = 0;

        int output_channels = 1;
        if (p.stereo)
            output_channels = 2;

        int *bars;
        int *previous_frame;

        float *bars_left, *bars_right;
        double *cava_out;
        float *bars_raw;
        float *previous_bars_raw;

        int height, lines, width, remainder, fp;
        int *dimension_bar, *dimension_value;
#ifdef _WIN32
        HANDLE hFile = NULL;
#endif

        if (p.orientation == ORIENT_LEFT || p.orientation == ORIENT_RIGHT) {
            dimension_bar = &height;
            dimension_value = &width;
        } else {
            dimension_bar = &width;
            dimension_value = &height;
        }

#ifdef SDL
        // output: start sdl mode
        if (output_mode == OUTPUT_SDL) {
            init_sdl_window(p.sdl_width, p.sdl_height, p.sdl_x, p.sdl_y, p.sdl_full_screen);
            height = p.sdl_height;
            width = p.sdl_width;
        }
#endif
#ifdef SDL_GLSL
        if (output_mode == OUTPUT_SDL_GLSL) {
            init_sdl_glsl_window(p.sdl_width, p.sdl_height, p.sdl_x, p.sdl_y, p.sdl_full_screen,
                                 p.vertex_shader, p.fragment_shader);
            height = p.sdl_height;
            width = p.sdl_width;
        }
#endif

        bool reloadConf = false;
        while (!reloadConf) { // jumping back to this loop means that you resized the screen

            // frequencies on x axis require a bar width of four or more
            if (p.xaxis == FREQUENCY && p.bar_width < 4)
                p.bar_width = 4;

            switch (output_mode) {
#ifdef NCURSES
            // output: start ncurses mode
            case OUTPUT_NCURSES:
                init_terminal_ncurses(p.color, p.bcolor, p.col, p.bgcol, p.gradient,
                                      p.gradient_count, p.gradient_colors, &width, &lines);
                if (p.xaxis != NONE)
                    lines--;
                height = lines;
                *dimension_value *=
                    8; // we have 8 times as much height due to using 1/8 block characters
                break;
#endif
#ifdef SDL
            // output: get sdl window size
            case OUTPUT_SDL:
                init_sdl_surface(&width, &height, p.color, p.bcolor, p.gradient, p.gradient_count,
                                 p.gradient_colors);
                break;
#endif
#ifdef SDL_GLSL
            // output: get sdl window size
            case OUTPUT_SDL_GLSL:
                init_sdl_glsl_surface(&width, &height, p.color, p.bcolor, p.bar_width,
                                      p.bar_spacing, p.gradient, p.gradient_count,
                                      p.gradient_colors);
                break;
#endif
            case OUTPUT_NONCURSES:
                get_terminal_dim_noncurses(&width, &lines);

                if (p.xaxis != NONE)
                    lines--;

                height = lines * 8 * p.max_height;
                break;
            case OUTPUT_RAW:
            case OUTPUT_NORITAKE:
                if (strcmp(p.raw_target, "/dev/stdout") != 0) {
#ifndef _WIN32
                    int fptest;
                    // checking if file exists
                    if (access(p.raw_target, F_OK) != -1) {
                        // file exists, testopening in case it's a fifo
                        fptest = open(p.raw_target, O_RDONLY | O_NONBLOCK, 0644);

                        if (fptest == -1) {
                            fprintf(stderr, "could not open file %s for writing\n", p.raw_target);
                            exit(1);
                        }
                    } else {
                        printf("creating fifo %s\n", p.raw_target);
                        if (mkfifo(p.raw_target, 0664) == -1) {
                            fprintf(stderr, "could not create fifo %s\n", p.raw_target);
                            exit(1);
                        }
                        // fifo needs to be open for reading in order to write to it
                        fptest = open(p.raw_target, O_RDONLY | O_NONBLOCK, 0644);
                    }
                    fp = open(p.raw_target, O_WRONLY | O_NONBLOCK | O_CREAT, 0644);
#else
                    int pipeLength = strlen("\\\\.\\pipe\\") + strlen(p.raw_target) + 1;
                    char *pipePath = malloc(pipeLength);
                    pipePath[pipeLength - 1] = '\0';
                    strcat(pipePath, "\\\\.\\pipe\\");
                    strcat(pipePath, p.raw_target);
                    DWORD pipeMode = strcmp(p.data_format, "ascii")
                                         ? PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE
                                         : PIPE_TYPE_BYTE | PIPE_READMODE_BYTE;
                    hFile = CreateNamedPipeA(pipePath, PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
                                             pipeMode | PIPE_NOWAIT, PIPE_UNLIMITED_INSTANCES, 0, 0,
                                             NMPWAIT_USE_DEFAULT_WAIT, NULL);
                    free(pipePath);
#endif
                } else {
#ifndef _WIN32
                    fp = fileno(stdout);
#else
                    hFile = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
                }
#ifndef _WIN32
                if (fp == -1) {
#else
                if (hFile == INVALID_HANDLE_VALUE) {
#endif
                    fprintf(stderr, "could not open file %s for writing\n", p.raw_target);
                    exit(1);
                }

#ifndef NDEBUG
                debug("open file %s for writing raw output\n", p.raw_target);
#endif

                // width must be hardcoded for raw output. only used to calculate the number of
                // bars in auto mode
                width = 512 * output_channels;
                p.bar_width = 1;
                p.bar_spacing = 0;

                if (strcmp(p.data_format, "ascii") != 0) {
                    // "binary" or "noritake"
                    height = pow(2, p.bit_format) - 1;
                } else {
                    height = p.ascii_range;
                }
                break;
            default:
                exit(EXIT_FAILURE); // Can't happen.
            }

            // getting numbers of bars
            int number_of_bars = -1;
            // handle for user setting too many bars
            if (p.fixedbars) {
                number_of_bars = p.fixedbars;
                if (number_of_bars * p.bar_width + p.fixedbars * p.bar_spacing - p.bar_spacing >
                    width) {
                    cleanup();
                    fprintf(stderr, "window is too narrow for number of bars set, maximum is %d\n",
                            (width - p.bar_spacing) / (p.bar_width + p.bar_spacing));
                    audio.terminate = 1;
                    exit(EXIT_FAILURE);
                }
                if (number_of_bars <= 1) {
                    cleanup();
                    fprintf(stderr, "fixed number of bars must be at least 1\n");
                    exit(EXIT_FAILURE);
                }
                if (output_channels == 2 && number_of_bars % 2 != 0) {
                    cleanup();
                    fprintf(stderr, "must have even number of bars with stereo output\n");
                    exit(EXIT_FAILURE);
                }
            } else {
                number_of_bars = (*dimension_bar + p.bar_spacing) / (p.bar_width + p.bar_spacing);

                if (output_mode == OUTPUT_SDL_GLSL) {
                    if (number_of_bars > 512)
                        number_of_bars = 512; // cant have more than 512 bars in glsl due to shader
                                              // program implementation limitations
                } else {
                    if (number_of_bars / output_channels > 512)
                        number_of_bars = 512 * output_channels; // cant have more than 512 bars on
                                                                // 44100 rate per channel
                }

                if (number_of_bars <= 1) {
                    number_of_bars = 1; // must have at least 1 bars
                    if (p.stereo) {
                        number_of_bars = 2; // stereo have at least 2 bars
                    }
                }
                if (output_channels == 2 && number_of_bars % 2 != 0)
                    number_of_bars--; // stereo must have even numbers of bars
            }

            int raw_number_of_bars = (number_of_bars / output_channels) * audio_channels;
            if (p.waveform) {
                raw_number_of_bars = number_of_bars;
            }

            // checks if there is stil extra room, will use this to center
            if (p.center_align) {
                remainder = (*dimension_bar - number_of_bars * p.bar_width -
                             number_of_bars * p.bar_spacing + p.bar_spacing) /
                            2;
                if (remainder < 0)
                    remainder = 0;
            } else {
                remainder = 0;
            }

            if (output_mode == OUTPUT_NONCURSES) {
                init_terminal_noncurses(inAtty, p.color, p.bcolor, p.col, p.bgcol, p.gradient,
                                        p.gradient_count, p.gradient_colors, p.horizontal_gradient,
                                        p.horizontal_gradient_count, p.horizontal_gradient_colors,
                                        number_of_bars, width, lines, p.bar_width, p.orientation,
                                        p.blendDirection);
            }

#ifndef NDEBUG
            debug("height: %d width: %d dimension_bar: %d dimension_value: %d bars:%d bar width: "
                  "%d remainder: %d\n",
                  height, width, *dimension_bar, *dimension_value, number_of_bars, p.bar_width,
                  remainder);
#endif

            double userEQ_keys_to_bars_ratio;

            if (p.userEQ_enabled && (number_of_bars / output_channels > 0)) {
                userEQ_keys_to_bars_ratio = (double)(((double)p.userEQ_keys) /
                                                     ((double)(number_of_bars / output_channels)));
            }

            struct cava_plan *plan =
                cava_init(number_of_bars / output_channels, audio.rate, audio.channels, p.autosens,
                          p.noise_reduction, p.lower_cut_off, p.upper_cut_off);

            if (plan->status == -1) {
                cleanup();
                fprintf(stderr, "Error initializing cava . %s", plan->error_message);
                exit(EXIT_FAILURE);
            }

            // if the sample rate is unusual high or low, we need to adjust the input buffer size
            // after the audio thread has started
            if (plan->input_buffer_size != audio.cava_buffer_size) {
                pthread_mutex_lock(&audio.lock);
                audio.cava_buffer_size = plan->input_buffer_size;
                free(audio.cava_in);
                audio.cava_in = (double *)malloc(audio.cava_buffer_size * sizeof(double));
                memset(audio.cava_in, 0, sizeof(double) * audio.cava_buffer_size);
                pthread_mutex_unlock(&audio.lock);
            }

            bars_left = (float *)malloc(number_of_bars / output_channels * sizeof(float));
            bars_right = (float *)malloc(number_of_bars / output_channels * sizeof(float));
            memset(bars_left, 0, sizeof(float) * number_of_bars / output_channels);
            memset(bars_right, 0, sizeof(float) * number_of_bars / output_channels);

            bars = (int *)malloc(number_of_bars * sizeof(int));
            bars_raw = (float *)malloc(number_of_bars * sizeof(float));
            previous_bars_raw = (float *)malloc(number_of_bars * sizeof(float));
            previous_frame = (int *)malloc(number_of_bars * sizeof(int));
            cava_out = (double *)malloc(number_of_bars * audio.channels / output_channels *
                                        sizeof(double));

            memset(bars, 0, sizeof(int) * number_of_bars);
            memset(bars_raw, 0, sizeof(float) * number_of_bars);
            memset(previous_bars_raw, 0, sizeof(float) * number_of_bars);
            memset(previous_frame, 0, sizeof(int) * number_of_bars);
            memset(cava_out, 0, sizeof(double) * number_of_bars * audio.channels / output_channels);

            int x_axis_info = 0;
            // process: calculate x axis values
            if (p.xaxis != NONE) {
                x_axis_info = 1;
                double cut_off_frequency;
                if (output_mode == OUTPUT_NONCURSES) {
                    printf("\r\033[%dB", lines + 1);
                    if (remainder)
                        printf("\033[%dC", remainder);
                }
                for (int n = 0; n < number_of_bars; n++) {
                    if (p.stereo) {
                        if (n < number_of_bars / 2)
                            cut_off_frequency = plan->cut_off_frequency[number_of_bars / 2 - 1 - n];
                        else
                            cut_off_frequency = plan->cut_off_frequency[n - number_of_bars / 2];
                    } else {
                        cut_off_frequency = plan->cut_off_frequency[n];
                    }

                    float freq_kilohz = cut_off_frequency / 1000;
                    int freq_floor = cut_off_frequency;

                    if (output_mode == OUTPUT_NCURSES) {
#ifdef NCURSES
                        if (cut_off_frequency < 1000)
                            mvprintw(lines, n * (p.bar_width + p.bar_spacing) + remainder, "%-4d",
                                     freq_floor);
                        else if (cut_off_frequency > 1000 && cut_off_frequency < 10000)
                            mvprintw(lines, n * (p.bar_width + p.bar_spacing) + remainder, "%.2f",
                                     freq_kilohz);
                        else
                            mvprintw(lines, n * (p.bar_width + p.bar_spacing) + remainder, "%.1f",
                                     freq_kilohz);
#endif
                    } else if (output_mode == OUTPUT_NONCURSES) {
                        if (cut_off_frequency < 1000)
                            printf("%-4d", freq_floor);
                        else if (cut_off_frequency > 1000 && cut_off_frequency < 10000)
                            printf("%.2f", freq_kilohz);
                        else
                            printf("%.1f", freq_kilohz);

                        if (n < number_of_bars - 1)
                            printf("\033[%dC", p.bar_width + p.bar_spacing - 4);
                    }
                }
                printf("\r\033[%dA", lines + 1);
            }

            bool resizeTerminal = false;

            int frame_time_msec = (1 / (float)p.framerate) * 1000;
            struct timespec framerate_timer = {.tv_sec = 0, .tv_nsec = 0};
            if (p.framerate <= 1) {
                framerate_timer.tv_sec = frame_time_msec / 1000;
            } else {
                framerate_timer.tv_sec = 0;
                framerate_timer.tv_nsec = frame_time_msec * 1e6;
            }
#ifdef _WIN32
            LARGE_INTEGER frequency; // ticks per second
            LARGE_INTEGER t1, t2;    // ticks
            double elapsedTime;
            QueryPerformanceFrequency(&frequency);
#endif // _WIN32

            int sleep_counter = 0;
            bool silence = false;
            char ch = '\0';

#ifndef NDEBUG
            int maxvalue = 0;
            int minvalue = 0;
#endif

            struct timespec sleep_mode_timer = {.tv_sec = 1, .tv_nsec = 0};

            int total_frames = 0;

            while (!resizeTerminal) {

// general: keyboard controls
#ifdef NCURSES
                if (output_mode == OUTPUT_NCURSES)
                    ch = getch();
#endif

#ifndef _WIN32
                if (output_mode == OUTPUT_NONCURSES)
                    read(0, &ch, sizeof(ch));
#endif

                switch (ch) {
                case 65: // key up
                    p.sens = p.sens * 1.05;
                    break;
                case 66: // key down
                    p.sens = p.sens * 0.95;
                    break;
                case 68: // key right
                    p.bar_width++;
                    resizeTerminal = true;
                    break;
                case 67: // key left
                    if (p.bar_width > 1)
                        p.bar_width--;
                    resizeTerminal = true;
                    break;
                case 'r': // reload config
                    should_reload = 1;
                    break;
                case 'c': // reload colors
                    reload_colors = 1;
                    break;
                case 'f': // change forground color
                    if (p.col < 7)
                        p.col++;
                    else
                        p.col = 0;
                    resizeTerminal = true;
                    break;
                case 'b': // change backround color
                    if (p.bgcol < 7)
                        p.bgcol++;
                    else
                        p.bgcol = 0;
                    resizeTerminal = true;
                    break;
                case 'o': // change orientation
                    if (output_mode == OUTPUT_NCURSES) {
                        if (p.orientation == ORIENT_BOTTOM) {
                            p.orientation = ORIENT_RIGHT;
                        } else if (p.orientation == ORIENT_RIGHT) {
                            p.orientation = ORIENT_TOP;
                        } else if (p.orientation == ORIENT_TOP) {
                            p.orientation = ORIENT_LEFT;
                        } else {
                            p.orientation = ORIENT_BOTTOM;
                        }

                        if (p.orientation == ORIENT_LEFT || p.orientation == ORIENT_RIGHT) {
                            dimension_bar = &height;
                            dimension_value = &width;
                        } else {
                            dimension_bar = &width;
                            dimension_value = &height;
                        }
                    } else {
                        p.orientation =
                            (p.orientation == ORIENT_BOTTOM) ? ORIENT_TOP : ORIENT_BOTTOM;
                    }

                    resizeTerminal = true;
                    break;

                case 'q':
                    should_reload = 1;
                    should_quit = 1;
                }

                ch = 0;

                if (should_reload) {

                    reloadConf = true;
                    resizeTerminal = true;
                    should_reload = 0;
                }

                if (reload_colors) {
                    struct error_s error;
                    error.length = 0;
                    if (!load_config(configPath, (void *)&p, 1, &error, reload)) {
                        cleanup();
                        fprintf(stderr, "Error loading config. %s", error.message);
                        exit(EXIT_FAILURE);
                    }
                    resizeTerminal = true;
                    reload_colors = 0;
                }

#ifndef NDEBUG
                // clear();
#ifndef _WIN32
                refresh();
#endif
#endif

                // process: check if input is present
                silence = true;

                for (int n = 0; n < audio.input_buffer_size * 4; n++) {
                    if (audio.cava_in[n]) {
                        silence = false;
                        break;
                    }
                }
#ifndef _WIN32

                if (output_mode != OUTPUT_SDL) {
                    if (p.sleep_timer) {
                        if (silence && sleep_counter <= p.framerate * p.sleep_timer)
                            sleep_counter++;
                        else if (!silence)
                            sleep_counter = 0;

                        if (sleep_counter > p.framerate * p.sleep_timer) {
#ifndef NDEBUG
                            printw("no sound detected for 30 sec, going to sleep mode\n");
#endif
                            nanosleep(&sleep_mode_timer, NULL);
                            continue;
                        }
                    }
                }

#endif // !_WIN32

                // process: execute cava
                pthread_mutex_lock(&audio.lock);
                if (p.waveform) {
                    for (int n = 0; n < audio.samples_counter; n++) {

                        for (int i = number_of_bars - 1; i > 0; i--) {
                            cava_out[i] = cava_out[i - 1];
                        }
                        if (audio_channels == 2) {
                            cava_out[0] =
                                p.sens * (audio.cava_in[n] / 2 + audio.cava_in[n + 1] / 2);
                            n++;
                        } else {
                            cava_out[0] = p.sens * audio.cava_in[n];
                        }
                    }
                } else {
                    cava_execute(audio.cava_in, audio.samples_counter, cava_out, plan);
                }
                if (audio.samples_counter > 0) {
                    audio.samples_counter = 0;
                }
                pthread_mutex_unlock(&audio.lock);

                for (int n = 0; n < raw_number_of_bars; n++) {

                    if (!p.waveform) {
                        cava_out[n] *= p.sens;
                    } else {
                        if (cava_out[n] > 1.0)
                            p.sens *= 0.999;
                        else
                            p.sens *= 1.00001;

                        if (p.orientation != ORIENT_SPLIT_H)
                            cava_out[n] = (cava_out[n] + 1.0) / 2.0;
                    }

                    if (output_mode == OUTPUT_SDL_GLSL) {
                        if (cava_out[n] > 1.0)
                            cava_out[n] = 1.0;
                        else if (cava_out[n] < 0.0)
                            cava_out[n] = 0.0;
                    } else {
                        cava_out[n] *= *dimension_value;
                        if (p.orientation == ORIENT_SPLIT_H || p.orientation == ORIENT_SPLIT_V) {
                            cava_out[n] /= 2;
                        }
                    }
                    if (p.waveform) {
                        bars_raw[n] = cava_out[n];
                    }
                }
                if (!p.waveform) {
                    if (audio_channels == 2) {
                        for (int n = 0; n < number_of_bars / output_channels; n++) {
                            if (p.userEQ_enabled)
                                cava_out[n] *=
                                    p.userEQ[(int)floor(((double)n) * userEQ_keys_to_bars_ratio)];
                            bars_left[n] = cava_out[n];
                        }
                        for (int n = 0; n < number_of_bars / output_channels; n++) {
                            if (p.userEQ_enabled)
                                cava_out[n + number_of_bars / output_channels] *=
                                    p.userEQ[(int)floor(((double)n) * userEQ_keys_to_bars_ratio)];
                            bars_right[n] = cava_out[n + number_of_bars / output_channels];
                        }
                    } else {
                        for (int n = 0; n < number_of_bars; n++) {
                            if (p.userEQ_enabled)
                                cava_out[n] *=
                                    p.userEQ[(int)floor(((double)n) * userEQ_keys_to_bars_ratio)];
                            bars_raw[n] = cava_out[n];
                        }
                    }

                    // process [filter]
                    if (p.monstercat) {
                        if (audio_channels == 2) {
                            bars_left =
                                monstercat_filter(bars_left, number_of_bars / output_channels,
                                                  p.waves, p.monstercat, *dimension_value);
                            bars_right =
                                monstercat_filter(bars_right, number_of_bars / output_channels,
                                                  p.waves, p.monstercat, *dimension_value);
                        } else {
                            bars_raw = monstercat_filter(bars_raw, number_of_bars, p.waves,
                                                         p.monstercat, *dimension_value);
                        }
                    }
                    if (audio_channels == 2) {
                        if (p.stereo) {
                            // mirroring stereo channels
                            for (int n = 0; n < number_of_bars; n++) {
                                if (n < number_of_bars / 2) {
                                    if (p.reverse) {
                                        bars_raw[n] = bars_left[n];
                                    } else {
                                        bars_raw[n] = bars_left[number_of_bars / 2 - n - 1];
                                    }
                                } else {
                                    if (p.reverse) {
                                        bars_raw[n] = bars_right[number_of_bars - n - 1];
                                    } else {
                                        bars_raw[n] = bars_right[n - number_of_bars / 2];
                                    }
                                }
                            }
                        } else {
                            // stereo mono output
                            for (int n = 0; n < number_of_bars; n++) {
                                if (p.reverse) {
                                    if (p.mono_opt == AVERAGE) {
                                        bars_raw[number_of_bars - n - 1] =
                                            (bars_left[n] + bars_right[n]) / 2;
                                    } else if (p.mono_opt == LEFT) {
                                        bars_raw[number_of_bars - n - 1] = bars_left[n];
                                    } else if (p.mono_opt == RIGHT) {
                                        bars_raw[number_of_bars - n - 1] = bars_right[n];
                                    }
                                } else {
                                    if (p.mono_opt == AVERAGE) {
                                        bars_raw[n] = (bars_left[n] + bars_right[n]) / 2;
                                    } else if (p.mono_opt == LEFT) {
                                        bars_raw[n] = bars_left[n];
                                    } else if (p.mono_opt == RIGHT) {
                                        bars_raw[n] = bars_right[n];
                                    }
                                }
                            }
                        }
                    }
                }
#ifdef SDL_GLSL
                int re_paint = 0;
#endif
                for (int n = 0; n < number_of_bars; n++) {
                    bars[n] = bars_raw[n];
                    // show idle bar heads
                    if (output_mode != OUTPUT_RAW && output_mode != OUTPUT_NORITAKE &&
                        bars[n] < 1 && p.waveform == 0 && p.show_idle_bar_heads == 1)
                        bars[n] = 1;
#ifdef SDL_GLSL

                    if (output_mode == OUTPUT_SDL_GLSL)
                        bars[n] =
                            bars_raw[n] * 1000; // values are 0-1, only used to check for changes

                    if (bars[n] != previous_frame[n])
                        re_paint = 1;
#endif

#ifndef NDEBUG
                    mvprintw(n, 0, "%d: f:%f->%f (%d->%d), eq:\
						%15e, peak:%d \n",
                             n, plan->cut_off_frequency[n], plan->cut_off_frequency[n + 1],
                             plan->FFTbuffer_lower_cut_off[n], plan->FFTbuffer_upper_cut_off[n],
                             plan->eq[n], bars[n]);

                    if (bars[n] < minvalue) {
                        minvalue = bars[n];
                        debug("min value: %d\n", minvalue); // checking maxvalue 10000
                    }
                    if (bars[n] > maxvalue) {
                        maxvalue = bars[n];
                    }
                    if (bars[n] < 0) {
                        debug("negative bar value!! %d\n", bars[n]);
                        //    exit(EXIT_FAILURE); // Can't happen.
                    }

#endif
                }

#ifndef NDEBUG
                mvprintw(number_of_bars + 1, 0, "sensitivity %.10e", p.sens);
                mvprintw(number_of_bars + 2, 0, "min value: %d\n",
                         minvalue); // checking maxvalue 10000
                mvprintw(number_of_bars + 3, 0, "max value: %d\n",
                         maxvalue); // checking maxvalue 10000
#ifndef _WIN32
                (void)x_axis_info;
#endif // !_WIN32
#endif

// output: draw processed input
#ifdef NDEBUG
                if (p.sync_updates) {
                    printf("\033[2026h\033\\");
                    fflush(stdout);
                    printf("\033[2026l\033\\");
                }
                int rc;
#ifdef _WIN32
                QueryPerformanceCounter(&t1);
#endif
                switch (output_mode) {
#ifdef SDL
                case OUTPUT_SDL:
                    rc = draw_sdl(number_of_bars, p.bar_width, p.bar_spacing, remainder,
                                  *dimension_value, bars, previous_frame, frame_time_msec,
                                  p.orientation, p.gradient);

                    break;
#endif
#ifdef SDL_GLSL
                case OUTPUT_SDL_GLSL:
                    rc = draw_sdl_glsl(number_of_bars, bars_raw, previous_bars_raw, frame_time_msec,
                                       re_paint, p.continuous_rendering);
                    break;
#endif
                case OUTPUT_NONCURSES:
                    if (p.orientation == ORIENT_SPLIT_H) {
                        rc = draw_terminal_noncurses(
                            inAtty, lines, width, number_of_bars, p.bar_width, p.bar_spacing,
                            remainder, bars, previous_frame, p.gradient, p.horizontal_gradient,
                            x_axis_info, ORIENT_BOTTOM, 1);
                        rc = draw_terminal_noncurses(
                            inAtty, lines, width, number_of_bars, p.bar_width, p.bar_spacing,
                            remainder, bars, previous_frame, p.gradient, p.horizontal_gradient,
                            x_axis_info, ORIENT_TOP, 1);
                    } else {
                        rc = draw_terminal_noncurses(
                            inAtty, lines, width, number_of_bars, p.bar_width, p.bar_spacing,
                            remainder, bars, previous_frame, p.gradient, p.horizontal_gradient,
                            x_axis_info, p.orientation, 0);
                    }
                    break;
                case OUTPUT_NCURSES:
#ifdef NCURSES
                    rc = draw_terminal_ncurses(inAtty, *dimension_value / 8, *dimension_bar,
                                               number_of_bars, p.bar_width, p.bar_spacing,
                                               remainder, bars, previous_frame, p.gradient,
                                               x_axis_info, p.orientation);
                    break;
#endif
                case OUTPUT_RAW:
#ifndef _WIN32
                    rc = print_raw_out(number_of_bars, fp, p.raw_format, p.bit_format,
                                       p.ascii_range, p.bar_delim, p.frame_delim, bars);
#else
                    rc = print_raw_out(number_of_bars, hFile, p.raw_format, p.bit_format,
                                       p.ascii_range, p.bar_delim, p.frame_delim, bars);
#endif
                    break;
                case OUTPUT_NORITAKE:
#ifndef _WIN32
                    rc = print_ntk_out(number_of_bars, fp, p.bit_format, p.bar_width, p.bar_spacing,
                                       p.bar_height, bars);
#else
                    rc = print_ntk_out(number_of_bars, hFile, p.bit_format, p.bar_width,
                                       p.bar_spacing, p.bar_height, bars);
#endif
                    break;
                default:
                    exit(EXIT_FAILURE); // Can't happen.
                }

                if (p.sync_updates) {
                    printf("\033[2026h\033\\");
                    fflush(stdout);
                    printf("\033[2026l\033\\");
                }
                // terminal has been resized breaking to recalibrating values
                if (rc == -1)
                    resizeTerminal = true;

                if (rc == -2) {
                    resizeTerminal = true;
                    reloadConf = true;
                    should_quit = true;
                }

#endif

                memcpy(previous_frame, bars, number_of_bars * sizeof(int));
                if (p.output == OUTPUT_SDL_GLSL) {
                    memcpy(previous_bars_raw, bars_raw, number_of_bars * sizeof(float));
                }

                // checking if audio thread has exited unexpectedly
                pthread_mutex_lock(&audio.lock);
                if (audio.terminate == 1) {
                    cleanup();
                    fprintf(stderr, "Audio thread exited unexpectedly. %s\n", audio.error_message);
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&audio.lock);
#ifdef _WIN32
                QueryPerformanceCounter(&t2);
                elapsedTime = (t2.QuadPart - t1.QuadPart) * 1000.0 / frequency.QuadPart;
                int fps_sync_time = frame_time_msec;
                if (elapsedTime < 1.0)
                    fps_sync_time = frame_time_msec;
                else if ((int)elapsedTime > frame_time_msec)
                    fps_sync_time = 0;
                else
                    fps_sync_time = (frame_time_msec - (int)elapsedTime) / 2;
#endif
                if (output_mode != OUTPUT_SDL && output_mode != OUTPUT_SDL_GLSL) {
#ifdef _WIN32
                    Sleep(fps_sync_time);
#else
                    nanosleep(&framerate_timer, NULL);
#endif
                }

                if (p.draw_and_quit > 0) {
                    total_frames++;
                    if (total_frames >= p.draw_and_quit) {
                        for (int n = 0; n < number_of_bars; n++) {
                            if (output_mode != OUTPUT_RAW && output_mode != OUTPUT_NORITAKE &&
                                bars[n] == 1) {
                                bars[n] = 0;
                            }
                            total_bar_height += bars[n];
                        }
                        resizeTerminal = true;
                        reloadConf = true;
                        should_quit = true;
                        break;
                    }
                }
            } // resize terminal
            cava_destroy(plan);
            free(plan);
            if (audio_channels == 2) {
                free(bars_left);
                free(bars_right);
            }
            free(cava_out);
            free(bars);
            free(bars_raw);
            free(previous_bars_raw);
            free(previous_frame);
        } // reloading config

        //**telling audio thread to terminate**//
        pthread_mutex_lock(&audio.lock);
        audio.terminate = 1;
        pthread_mutex_unlock(&audio.lock);
        pthread_join(p_thread, NULL);

        if (p.userEQ_enabled)
            free(p.userEQ);

        free(audio.source);
        free(audio.cava_in);
        cleanup();
        reload = 1;

        if (should_quit && signal_received == 0) {
            if (p.zero_test && total_bar_height > 0) {
                fprintf(stderr, "Test mode: expected total bar height to be zero, but was: %d\n",
                        total_bar_height);
                return EXIT_FAILURE;
            } else if (p.non_zero_test && total_bar_height == 0) {
                fprintf(stderr,
                        "Test mode: expected total bar height to be non-zero, but was zero\n");
                return EXIT_FAILURE;
            } else {
                return EXIT_SUCCESS;
            }
        }
        if (signal_received != 0) {
            signal(signal_received, SIG_DFL);
            raise(signal_received);
        }
        // fclose(fp);
        // CloseHandle(hFile);
    }
}
