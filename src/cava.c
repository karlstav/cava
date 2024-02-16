#include <locale.h>

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#else
#include <stdlib.h>
#endif

#include <math.h>

#ifdef _MSC_VER
#include <windows.h>
#define PATH_MAX 260
#define PACKAGE "cava"
#define _CRT_SECURE_NO_WARNINGS 1
#endif // _MSC_VER

#include <signal.h>

#include "cava/common.h"

#include "cava/debug.h"

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

// general: handle signals
void sig_handler(int sig_no) {
#ifndef _MSC_VER

    if (sig_no == SIGUSR1) {
        should_reload = 1;
        return;
    }

    if (sig_no == SIGUSR2) {
        reload_colors = 1;
        return;
    }
#endif

    cleanup(output_mode);

    if (sig_no == SIGINT) {
        printf("CTRL-C pressed -- goodbye\n");
    }

    signal(sig_no, SIG_DFL);
    raise(sig_no);
}

// general: entry point
int main(int argc, char **argv) {

#ifndef _MSC_VER
    // general: console title
    printf("%c]0;%s%c", '\033', PACKAGE, '\007');
#endif // !_MSC_VER

    // general: handle command-line arguments
    char configPath[PATH_MAX];
    configPath[0] = '\0';
#ifdef _MSC_VER
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
        q         Quit\n\
\n\
as of 0.4.0 all options are specified in config file, see in '/home/username/.config/cava/' \n";
#ifndef _MSC_VER

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
#else
    if (argc > 1)
        snprintf(configPath, sizeof(configPath), "%s", argv[1]);
#endif

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

        p.inAtty = 0;

        output_mode = p.output;
#ifndef _MSC_VER
        if (output_mode == OUTPUT_NCURSES || output_mode == OUTPUT_NONCURSES) {
            // Check if we're running in a tty
            if (strncmp(ttyname(0), "/dev/tty", 8) == 0 || strcmp(ttyname(0), "/dev/console") == 0)
                p.inAtty = 1;

            // in macos vitual terminals are called ttys(xyz) and there are no ttys
            if (strncmp(ttyname(0), "/dev/ttys", 9) == 0)
                p.inAtty = 0;
            if (p.inAtty) {
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
        struct audio_raw audio_raw;
        struct cava_plan *plan = (struct cava_plan *)malloc(sizeof(struct cava_plan));
        memset(&audio, 0, sizeof(audio));
        memset(&audio_raw, 0, sizeof(audio_raw));

        audio.source = malloc(1 + strlen(p.audio_source));
        strcpy(audio.source, p.audio_source);

        audio.format = -1;
        audio.rate = 0;
        audio.samples_counter = 0;
        audio.channels = 2;
        audio.IEEE_FLOAT = 0;
        audio.autoconnect = 0;

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

        // Run audio source reader
        pthread_mutex_init(&audio.lock, NULL);
        thr_id = pthread_create(&p_thread, NULL, get_input(&audio, &p), (void *)&audio);

        timeout_counter = 0;
        while (true) {
#ifdef _MSC_VER
            Sleep(1);
#else
            nanosleep(&timeout_timer, NULL);
#endif
            pthread_mutex_lock(&audio.lock);
            if ((audio.threadparams == 0) && (audio.format != -1) && (audio.rate != 0))
                break;

            pthread_mutex_unlock(&audio.lock);
            timeout_counter++;
            if (timeout_counter > 2000) {
                cleanup(output_mode);
                fprintf(stderr, "could not get rate and/or format, problems with audio thread? "
                                "quiting...\n");
                exit(EXIT_FAILURE);
            }
        }
        pthread_mutex_unlock(&audio.lock);
        debug("got format: %d and rate %d\n", audio.format, audio.rate);

        bool reloadConf = false;
        while (!reloadConf) { // jumping back to this loop means that you resized the screen
            // Initialize audio raw data structures
            audio_raw_init(&audio, &audio_raw, &p, plan);

            bool resizeTerminal = false;

            int frame_time_msec = (1 / (float)p.framerate) * 1000;
            struct timespec framerate_timer = {.tv_sec = 0, .tv_nsec = 0};
            if (p.framerate <= 1) {
                framerate_timer.tv_sec = frame_time_msec / 1000;
            } else {
                framerate_timer.tv_sec = 0;
                framerate_timer.tv_nsec = frame_time_msec * 1e6;
            }
#ifdef _MSC_VER
            LARGE_INTEGER frequency; // ticks per second
            LARGE_INTEGER t1, t2;    // ticks
            double elapsedTime;
            QueryPerformanceFrequency(&frequency);
#endif // _MSC_VER

            int sleep_counter = 0;
            bool silence = false;
            char ch = '\0';

            struct timespec sleep_mode_timer = {.tv_sec = 1, .tv_nsec = 0};

            int total_frames = 0;

            while (!resizeTerminal) {

// general: keyboard controls
#ifdef NCURSES
                if (output_mode == OUTPUT_NCURSES)
                    ch = getch();
#endif

#ifndef _MSC_VER
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
                    if (!load_config(configPath, (void *)&p, 1, &error)) {
                        cleanup(output_mode);
                        fprintf(stderr, "Error loading config. %s", error.message);
                        exit(EXIT_FAILURE);
                    }
                    resizeTerminal = true;
                    reload_colors = 0;
                }

#ifndef NDEBUG
                // clear();
#ifndef _MSC_VER
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
#ifndef _MSC_VER

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

#endif // !_MSC_VER

                // process: execute cava
                pthread_mutex_lock(&audio.lock);
                if (p.waveform) {
                    for (int n = 0; n < audio.samples_counter; n++) {

                        for (int i = audio_raw.number_of_bars - 1; i > 0; i--) {
                            audio_raw.cava_out[i] = audio_raw.cava_out[i - 1];
                        }
                        if (audio.channels == 2) {
                            audio_raw.cava_out[0] =
                                p.sens * (audio.cava_in[n] / 2 + audio.cava_in[n + 1] / 2);
                            ++n;
                        } else {
                            audio_raw.cava_out[0] = p.sens * audio.cava_in[n];
                        }
                    }
                } else {
                    cava_execute(audio.cava_in, audio.samples_counter, audio_raw.cava_out, plan);
                }
                if (audio.samples_counter > 0) {
                    audio.samples_counter = 0;
                }
                pthread_mutex_unlock(&audio.lock);

                int re_paint = 0;

                // Do transformation under raw data
                audio_raw_fetch(&audio_raw, &p, &re_paint, plan);

// output: draw processed input
#ifdef NDEBUG
                if (p.sync_updates) {
                    printf("\033P=1s\033\\");
                    fflush(stdout);
                }
                int rc;
#ifdef _MSC_VER
                (void)audio_raw.x_axis_info;

                QueryPerformanceCounter(&t1);
#endif
                switch (output_mode) {
#ifdef SDL
                case OUTPUT_SDL:
                    rc = draw_sdl(audio_raw.number_of_bars, p.bar_width, p.bar_spacing,
                                  audio_raw.remainder, *audio_raw.dimension_value, audio_raw.bars,
                                  audio_raw.previous_frame, frame_time_msec, p.orientation,
                                  p.gradient);

                    break;
#endif
#ifdef SDL_GLSL
                case OUTPUT_SDL_GLSL:
                    rc = draw_sdl_glsl(audio_raw.number_of_bars, audio_raw.bars_raw,
                                       frame_time_msec, re_paint, p.continuous_rendering);
                    break;
#endif
                case OUTPUT_NONCURSES:
                    rc = draw_terminal_noncurses(
                        p.inAtty, audio_raw.lines, audio_raw.width, audio_raw.number_of_bars,
                        p.bar_width, p.bar_spacing, audio_raw.remainder, audio_raw.bars,
                        audio_raw.previous_frame, p.gradient, p.x_axis_info);
                    break;
                case OUTPUT_NCURSES:
#ifdef NCURSES
                    rc = draw_terminal_ncurses(p.inAtty, *audio_raw.dimension_value / 8,
                                               *audio_raw.dimension_bar, audio_raw.number_of_bars,
                                               p.bar_width, p.bar_spacing, audio_raw.remainder,
                                               audio_raw.bars, audio_raw.previous_frame, p.gradient,
                                               p.x_axis_info, p.orientation);
                    break;
#endif
#ifndef _MSC_VER
                case OUTPUT_RAW:
                    rc = print_raw_out(audio_raw.number_of_bars, p.fp, p.raw_format, p.bit_format,
                                       p.ascii_range, p.bar_delim, p.frame_delim, audio_raw.bars);
                    break;
                case OUTPUT_NORITAKE:
                    rc = print_ntk_out(audio_raw.number_of_bars, p.fp, p.bit_format, p.bar_width,
                                       p.bar_spacing, p.bar_height, audio_raw.bars);
                    break;

#endif // !_MSC_VER
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

                memcpy(audio_raw.previous_frame, audio_raw.bars,
                       audio_raw.number_of_bars * sizeof(int));

                // checking if audio thread has exited unexpectedly
                pthread_mutex_lock(&audio.lock);
                if (audio.terminate == 1) {
                    cleanup(output_mode);
                    fprintf(stderr, "Audio thread exited unexpectedly. %s\n", audio.error_message);
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&audio.lock);
#ifdef _MSC_VER
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
#ifdef _MSC_VER
                    Sleep(fps_sync_time);
#else
                    nanosleep(&framerate_timer, NULL);
#endif
                }

                if (p.draw_and_quit > 0) {
                    total_frames++;
                    if (total_frames >= p.draw_and_quit) {
                        for (int n = 0; n < audio_raw.number_of_bars; n++) {
                            if (output_mode != OUTPUT_RAW && output_mode != OUTPUT_NORITAKE &&
                                audio_raw.bars[n] == 1) {
                                audio_raw.bars[n] = 0;
                            }
                            total_bar_height += audio_raw.bars[n];
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
            audio_raw_clean(&audio_raw);
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
        cleanup(output_mode);

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
