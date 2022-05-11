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
#include <termios.h>

#include <ctype.h>
#include <dirent.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "cavacore.h"

#include "config.h"

#include "debug.h"
#include "util.h"

#ifdef NCURSES
#include "output/terminal_bcircle.h"
#include "output/terminal_ncurses.h"
#include <curses.h>
#endif

#ifdef SDL
#include "output/sdl_cava.h"
#endif

#include "output/noritake.h"
#include "output/raw.h"
#include "output/terminal_noncurses.h"

#include "input/alsa.h"
#include "input/common.h"
#include "input/fifo.h"
#include "input/portaudio.h"
#include "input/pulse.h"
#include "input/shmem.h"
#include "input/sndio.h"

#ifdef __GNUC__
// curses.h or other sources may already define
#undef GCC_UNUSED
#define GCC_UNUSED __attribute__((unused))
#else
#define GCC_UNUSED /* nothing */
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
    }
}

// general: handle signals
void sig_handler(int sig_no) {
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

int *monstercat_filter(int *bars, int number_of_bars, int waves, double monstercat) {

    int z;

    // process [smoothing]: monstercat-style "average"

    int m_y, de;
    if (waves > 0) {
        for (z = 0; z < number_of_bars; z++) { // waves
            bars[z] = bars[z] / 1.25;
            // if (bars[z] < 1) bars[z] = 1;
            for (m_y = z - 1; m_y >= 0; m_y--) {
                de = z - m_y;
                bars[m_y] = max(bars[z] - pow(de, 2), bars[m_y]);
            }
            for (m_y = z + 1; m_y < number_of_bars; m_y++) {
                de = m_y - z;
                bars[m_y] = max(bars[z] - pow(de, 2), bars[m_y]);
            }
        }
    } else if (monstercat > 0) {
        for (z = 0; z < number_of_bars; z++) {
            // if (bars[z] < 1)bars[z] = 1;
            for (m_y = z - 1; m_y >= 0; m_y--) {
                de = z - m_y;
                bars[m_y] = max(bars[z] / pow(monstercat, de), bars[m_y]);
            }
            for (m_y = z + 1; m_y < number_of_bars; m_y++) {
                de = m_y - z;
                bars[m_y] = max(bars[z] / pow(monstercat, de), bars[m_y]);
            }
        }
    }

    return bars;
}

// general: entry point
int main(int argc, char **argv) {

    // general: console title
    printf("%c]0;%s%c", '\033', PACKAGE, '\007');

    // general: handle command-line arguments
    char configPath[PATH_MAX];
    configPath[0] = '\0';

    // general: handle Ctrl+C
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = &sig_handler;
    sigaction(SIGINT, &action, NULL);
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGUSR1, &action, NULL);
    sigaction(SIGUSR2, &action, NULL);

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

    int c;
    while ((c = getopt(argc, argv, "p:vh")) != -1) {
        switch (c) {
        case 'p': // argument: fifo path
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

    // general: main loop
    while (1) {

        debug("loading config\n");
        // config: load
        struct error_s error;
        error.length = 0;
        if (!load_config(configPath, &p, 0, &error)) {
            fprintf(stderr, "Error loading config. %s", error.message);
            exit(EXIT_FAILURE);
        }

        int inAtty;

        output_mode = p.output;

        if (output_mode != OUTPUT_RAW && output_mode != OUTPUT_NORITAKE) {
            // Check if we're running in a tty
            inAtty = 0;
            if (strncmp(ttyname(0), "/dev/tty", 8) == 0 || strcmp(ttyname(0), "/dev/console") == 0)
                inAtty = 1;

            // in macos vitual terminals are called ttys(xyz) and there are no ttys
            if (strncmp(ttyname(0), "/dev/ttys", 9) == 0)
                inAtty = 0;
            if (inAtty) {
                // checking if cava psf font is installed in FONTDIR
                FILE *font_file;
                font_file = fopen(FONTDIR "/cava.psf", "r");
                if (font_file) {
                    fclose(font_file);
                    system("setfont " FONTDIR "/cava.psf  >/dev/null 2>&1");
                } else {
                    // if not it might still be available, we dont know, must try
                    system("setfont cava.psf  >/dev/null 2>&1");
                }
                system("setterm -blank 0");
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

        // input: init

        struct audio_data audio;
        memset(&audio, 0, sizeof(audio));

        audio.source = malloc(1 + strlen(p.audio_source));
        strcpy(audio.source, p.audio_source);

        audio.format = -1;
        audio.rate = 0;
        audio.samples_counter = 0;
        audio.channels = 2;

        audio.input_buffer_size = BUFFER_SIZE * audio.channels;
        audio.cava_buffer_size = audio.input_buffer_size * 8;

        audio.cava_in = (double *)malloc(audio.cava_buffer_size * sizeof(double));
        memset(audio.cava_in, 0, sizeof(int) * audio.cava_buffer_size);

        audio.terminate = 0;

        debug("starting audio thread\n");

        pthread_t p_thread;
        int timeout_counter = 0;
        int total_bar_height = 0;

        struct timespec timeout_timer = {.tv_sec = 0, .tv_nsec = 1000000};
        int thr_id GCC_UNUSED;

        switch (p.input) {
#ifdef ALSA
        case INPUT_ALSA:
            // input_alsa: wait for the input to be ready
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

            thr_id = pthread_create(&p_thread, NULL, input_alsa,
                                    (void *)&audio); // starting alsamusic listener

            timeout_counter = 0;

            while (audio.format == -1 || audio.rate == 0) {
                nanosleep(&timeout_timer, NULL);
                timeout_counter++;
                if (timeout_counter > 2000) {
                    cleanup();
                    fprintf(stderr, "could not get rate and/or format, problems with audio thread? "
                                    "quiting...\n");
                    exit(EXIT_FAILURE);
                }
            }
            debug("got format: %d and rate %d\n", audio.format, audio.rate);
            break;
#endif
        case INPUT_FIFO:
            // starting fifomusic listener
            thr_id = pthread_create(&p_thread, NULL, input_fifo, (void *)&audio);
            audio.rate = p.fifoSample;
            audio.format = p.fifoSampleBits;
            break;
#ifdef PULSE
        case INPUT_PULSE:
            if (strcmp(audio.source, "auto") == 0) {
                getPulseDefaultSink((void *)&audio);
            }
            // starting pulsemusic listener
            thr_id = pthread_create(&p_thread, NULL, input_pulse, (void *)&audio);
            audio.rate = 44100;
            break;
#endif
#ifdef SNDIO
        case INPUT_SNDIO:
            thr_id = pthread_create(&p_thread, NULL, input_sndio, (void *)&audio);
            audio.rate = 44100;
            break;
#endif
        case INPUT_SHMEM:
            thr_id = pthread_create(&p_thread, NULL, input_shmem, (void *)&audio);

            timeout_counter = 0;
            while (audio.rate == 0) {
                nanosleep(&timeout_timer, NULL);
                timeout_counter++;
                if (timeout_counter > 2000) {
                    cleanup();
                    fprintf(stderr, "could not get rate and/or format, problems with audio thread? "
                                    "quiting...\n");
                    exit(EXIT_FAILURE);
                }
            }
            debug("got format: %d and rate %d\n", audio.format, audio.rate);
            // audio.rate = 44100;
            break;
#ifdef PORTAUDIO
        case INPUT_PORTAUDIO:
            thr_id = pthread_create(&p_thread, NULL, input_portaudio, (void *)&audio);
            audio.rate = 44100;
            break;
#endif
        default:
            exit(EXIT_FAILURE); // Can't happen.
        }

        if (p.upper_cut_off > audio.rate / 2) {
            cleanup();
            fprintf(stderr, "higher cuttoff frequency can't be higher than sample rate / 2");
            exit(EXIT_FAILURE);
        }

        int *bars;
        int *previous_frame;

        int *bars_left, *bars_right;
        double *cava_out;

        int height, lines, width, remainder, fp;

#ifdef SDL
        // output: start sdl mode
        if (output_mode == OUTPUT_SDL) {
            init_sdl_window(p.sdl_width, p.sdl_height, p.sdl_x, p.sdl_y);
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
                // we have 8 times as much height due to using 1/8 block characters
                height = lines * 8;
                break;
#endif
#ifdef SDL
            // output: get sdl window size
            case OUTPUT_SDL:
                init_sdl_surface(&width, &height, p.color, p.bcolor);
                break;
#endif
            case OUTPUT_NONCURSES:
                get_terminal_dim_noncurses(&width, &lines);

                if (p.xaxis != NONE)
                    lines--;

                init_terminal_noncurses(inAtty, p.col, p.bgcol, width, lines, p.bar_width);
                height = lines * 8;
                break;

            case OUTPUT_RAW:
            case OUTPUT_NORITAKE:
                if (strcmp(p.raw_target, "/dev/stdout") != 0) {
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
                } else {
                    fp = fileno(stdout);
                }
                if (fp == -1) {
                    fprintf(stderr, "could not open file %s for writing\n", p.raw_target);
                    exit(1);
                }
#ifndef NDEBUG
                debug("open file %s for writing raw output\n", p.raw_target);
#endif

                // width must be hardcoded for raw output.
                width = 512;

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

            // handle for user setting too many bars
            if (p.fixedbars) {
                p.autobars = 0;
                if (p.fixedbars * p.bar_width + p.fixedbars * p.bar_spacing - p.bar_spacing > width)
                    p.autobars = 1;
            }

            // getting numbers of bars
            int number_of_bars = p.fixedbars;

            if (p.autobars == 1)
                number_of_bars = (width + p.bar_spacing) / (p.bar_width + p.bar_spacing);

            if (number_of_bars <= 1) {
                number_of_bars = 1; // must have at least 1 bars
                if (p.stereo) {
                    number_of_bars = 2; // stereo have at least 2 bars
                }
            }
            if (number_of_bars > 512)
                number_of_bars = 512; // cant have more than 512 bars on 44100 rate

            int output_channels = 1;
            if (p.stereo) { // stereo must have even numbers of bars
                if (audio.channels == 1) {
                    fprintf(stderr,
                            "stereo output configured, but only one channel in audio input.\n");
                    exit(1);
                }
                output_channels = 2;
                if (number_of_bars % 2 != 0)
                    number_of_bars--;
            }

            // checks if there is stil extra room, will use this to center
            remainder = (width - number_of_bars * p.bar_width - number_of_bars * p.bar_spacing +
                         p.bar_spacing) /
                        2;
            if (remainder < 0)
                remainder = 0;

#ifndef NDEBUG
            debug("height: %d width: %d bars:%d bar width: %d remainder: %d\n", height, width,
                  number_of_bars, p.bar_width, remainder);
#endif

            struct cava_plan *plan =
                cava_init(number_of_bars / output_channels, audio.rate, audio.channels, p.autosens,
                          p.noise_reduction, p.lower_cut_off, p.upper_cut_off);

            if (audio.channels == 2) {
                bars_left = (int *)malloc(number_of_bars / output_channels * sizeof(int));
                bars_right = (int *)malloc(number_of_bars / output_channels * sizeof(int));
                memset(bars_left, 0, sizeof(int) * number_of_bars / output_channels);
                memset(bars_right, 0, sizeof(int) * number_of_bars / output_channels);
            }
            bars = (int *)malloc(number_of_bars * sizeof(int));
            previous_frame = (int *)malloc(number_of_bars * sizeof(int));
            cava_out = (double *)malloc(number_of_bars * audio.channels / output_channels *
                                        sizeof(double));

            memset(bars, 0, sizeof(int) * number_of_bars);
            memset(previous_frame, 0, sizeof(int) * number_of_bars);
            memset(cava_out, 0, sizeof(double) * number_of_bars * audio.channels / output_channels);

            // process: calculate x axis values
            int x_axis_info = 0;

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
                /*
                // disabled key controls in non-curses mode, caused garbage on screen
                if (output_mode == OUTPUT_NONCURSES)
                    ch = fgetc(stdin);
                */

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

                case 'q':
                    should_reload = 1;
                    should_quit = 1;
                }

                if (should_reload) {

                    reloadConf = true;
                    resizeTerminal = true;
                    should_reload = 0;
                }

                if (reload_colors) {
                    struct error_s error;
                    error.length = 0;
                    if (!load_config(configPath, (void *)&p, 1, &error)) {
                        cleanup();
                        fprintf(stderr, "Error loading config. %s", error.message);
                        exit(EXIT_FAILURE);
                    }
                    resizeTerminal = true;
                    reload_colors = 0;
                }

#ifndef NDEBUG
                // clear();
                refresh();
#endif

                // process: check if input is present
                silence = true;

                for (int n = 0; n < audio.input_buffer_size * 4; n++) {
                    if (audio.cava_in[n]) {
                        silence = false;
                        break;
                    }
                }

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

                // process: execute cava
                pthread_mutex_lock(&audio.lock);
                cava_execute(audio.cava_in, audio.samples_counter, cava_out, plan);
                if (audio.samples_counter > 0) {
                    audio.samples_counter = 0;
                }
                pthread_mutex_unlock(&audio.lock);

                for (uint32_t n = 0; n < (number_of_bars / output_channels) * audio.channels; n++) {
                    if (p.autosens)
                        cava_out[n] *= height;
                    else
                        cava_out[n] *= p.sens;
                }

                if (audio.channels == 2) {
                    for (int n = 0; n < number_of_bars / output_channels; n++) {
                        bars_left[n] = cava_out[n];
                    }
                    for (int n = 0; n < number_of_bars / output_channels; n++) {
                        bars_right[n] = cava_out[n + number_of_bars / output_channels];
                    }
                } else {
                    for (int n = 0; n < number_of_bars; n++) {
                        bars[n] = cava_out[n];
                    }
                }

                // process [filter]
                if (p.monstercat) {
                    if (audio.channels == 2) {
                        bars_left =
                            monstercat_filter(bars_left, number_of_bars / 2, p.waves, p.monstercat);
                        bars_right = monstercat_filter(bars_right, number_of_bars / 2, p.waves,
                                                       p.monstercat);
                    } else {
                        bars = monstercat_filter(bars, number_of_bars, p.waves, p.monstercat);
                    }
                }
                if (audio.channels == 2) {
                    if (p.stereo) {
                        // mirroring stereo channels
                        for (int n = 0; n < number_of_bars; n++) {
                            if (n < number_of_bars / 2) {
                                if (p.reverse) {
                                    bars[n] = bars_left[n];
                                } else {
                                    bars[n] = bars_left[number_of_bars / 2 - n - 1];
                                }
                            } else {
                                if (p.reverse) {
                                    bars[n] = bars_right[number_of_bars - n - 1];
                                } else {
                                    bars[n] = bars_right[n - number_of_bars / 2];
                                }
                            }
                        }
                    } else {
                        // stereo mono output
                        for (int n = 0; n < number_of_bars; n++) {
                            if (p.reverse) {
                                if (p.mono_opt == AVERAGE) {
                                    bars[number_of_bars - n - 1] =
                                        (bars_left[n] + bars_right[n]) / 2;
                                } else if (p.mono_opt == LEFT) {
                                    bars[number_of_bars - n - 1] = bars_left[n];
                                } else if (p.mono_opt == RIGHT) {
                                    bars[number_of_bars - n - 1] = bars_right[n];
                                }
                            } else {
                                if (p.mono_opt == AVERAGE) {
                                    bars[n] = (bars_left[n] + bars_right[n]) / 2;
                                } else if (p.mono_opt == LEFT) {
                                    bars[n] = bars_left[n];
                                } else if (p.mono_opt == RIGHT) {
                                    bars[n] = bars_right[n];
                                }
                            }
                        }
                    }
                }

                for (int n = 0; n < number_of_bars; n++) {
                    // zero values causes divided by zero segfault (if not raw)
                    if (output_mode != OUTPUT_RAW && output_mode != OUTPUT_NORITAKE && bars[n] < 1)
                        bars[n] = 1;
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
                (void)x_axis_info;
#endif

// output: draw processed input
#ifdef NDEBUG
                if (p.sync_updates) {
                    printf("\033P=1s\033\\");
                    fflush(stdout);
                }
                int rc;
                switch (output_mode) {
                case OUTPUT_NCURSES:
#ifdef NCURSES
                    rc = draw_terminal_ncurses(inAtty, lines, width, number_of_bars, p.bar_width,
                                               p.bar_spacing, remainder, bars, previous_frame,
                                               p.gradient, x_axis_info);
                    break;
#endif
#ifdef SDL
                case OUTPUT_SDL:
                    rc = draw_sdl(number_of_bars, p.bar_width, p.bar_spacing, remainder, height,
                                  bars, previous_frame, frame_time_msec);
                    break;
#endif
                case OUTPUT_NONCURSES:
                    rc = draw_terminal_noncurses(inAtty, lines, width, number_of_bars, p.bar_width,
                                                 p.bar_spacing, remainder, bars, previous_frame,
                                                 x_axis_info);
                    break;
                case OUTPUT_RAW:
                    rc = print_raw_out(number_of_bars, fp, p.raw_format, p.bit_format,
                                       p.ascii_range, p.bar_delim, p.frame_delim, bars);
                    break;
                case OUTPUT_NORITAKE:
                    rc = print_ntk_out(number_of_bars, fp, p.bit_format, p.bar_width, p.bar_spacing,
                                       p.bar_height, bars);
                    break;
                default:
                    exit(EXIT_FAILURE); // Can't happen.
                }
                if (p.sync_updates) {
                    printf("\033P=2s\033\\");
                    fflush(stdout);
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

                // checking if audio thread has exited unexpectedly
                if (audio.terminate == 1) {
                    cleanup();
                    fprintf(stderr, "Audio thread exited unexpectedly. %s\n", audio.error_message);
                    exit(EXIT_FAILURE);
                }

                if (output_mode != OUTPUT_SDL)
                    nanosleep(&framerate_timer, NULL);

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
            if (audio.channels == 2) {
                free(bars_left);
                free(bars_right);
            }
            free(cava_out);
            free(bars);
            free(previous_frame);
        } // reloading config

        //**telling audio thread to terminate**//
        audio.terminate = 1;
        pthread_join(p_thread, NULL);

        if (p.userEQ_enabled)
            free(p.userEQ);

        free(audio.source);
        free(audio.cava_in);
        cleanup();

        if (should_quit) {
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
        // fclose(fp);
    }
}
