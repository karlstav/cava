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
#define VERSION "1.0.0"
#define _CRT_SECURE_NO_WARNINGS 1
#endif // _WIN32

#include <signal.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

#include "cavacore.h"

#include "config.h"

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
#include "input/coreaudio.h"
#include "input/fifo.h"
#include "input/jack.h"
#include "input/oss.h"
#include "input/pipewire.h"
#include "input/portaudio.h"
#include "input/pulse.h"
#include "input/shmem.h"
#include "input/sndio.h"
#ifdef COREAUDIO_TAP
#include "input/coreaudio_tap.h"
#endif
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
int optopt = 0;

static int getopt(int argc, char *const argv[], const char *optstring) {
    if ((optind >= argc) || (argv[optind][0] != '-') || (argv[optind][0] == 0)) {
        return -1;
    }

    if (argv[optind][0] == '-' && argv[optind][1] == '\0') {
        return -1;
    }

    if (strcmp(argv[optind], "--") == 0) {
        optind++;
        return -1;
    }

    if (argv[optind][0] == '-' && argv[optind][1] == '-' && argv[optind][2] != '\0') {
        const char *longopt = argv[optind] + 2;
        if (strcmp(longopt, "help") == 0) {
            optopt = 'h';
            optind++;
            return 'h';
        }
        if (strcmp(longopt, "version") == 0) {
            optopt = 'v';
            optind++;
            return 'v';
        }
        if (strcmp(longopt, "config") == 0) {
            optopt = 'p';
            optind++;
            if (optind >= argc) {
                return ':';
            }
            optarg = argv[optind];
            optind++;
            return 'p';
        }
        optopt = 0;
        optind++;
        return '?';
    }

    int opt = argv[optind][1];
    const char *opt_position = strchr(optstring, opt);

    if (opt_position == NULL) {
        optopt = opt;
        optind++;
        return '?';
    }
    if (opt_position[1] == ':') {
        optopt = opt;
        optind++;
        if (optind >= argc) {
            return ':';
        }
        optarg = argv[optind];
        optind++;
        return opt;
    }

    optopt = opt;
    optind++;
    return opt;
}
#endif

// used by sig handler
// needs to know output mode in order to clean up terminal
int output_mode;
// whether we should reload the config or not
volatile sig_atomic_t should_reload = 0;
// whether we should only reload colors or not
volatile sig_atomic_t reload_colors = 0;
// whether we should quit
volatile sig_atomic_t should_quit = 0;
volatile sig_atomic_t signal_received = 0;

static bool get_file_state(const char *path, time_t *mtime, long long *size) {
#ifdef _WIN32
    struct _stat st;
    if (_stat(path, &st) != 0)
        return false;
#else
    struct stat st;
    if (stat(path, &st) != 0)
        return false;
#endif
    *mtime = st.st_mtime;
    *size = (long long)st.st_size;
    return true;
}

// these variables are used only in main, but making them global
// will allow us to not free them on exit without ASan complaining
struct config_params p = {0};

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

static void parse_arguments(int argc, char **argv, char *configPath) {
  char *usage = "\n\
                Usage: " PACKAGE " [options]\n\
                        Visualize audio input in terminal.\n\
\n\
                        Options:\n\
\t-p, --config <path>    Path to config file\n\
\t-v, --version          Print version and exit\n\
\t-h, --help             Show this help and exit\n\
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
#ifndef _WIN32
  static struct option long_options[] = {
    {"config", required_argument, NULL, 'p'},
    {"version", no_argument, NULL, 'v'},
    {"help", no_argument, NULL, 'h'},
    {0, 0, 0, 0},
  };
  opterr = 0;
  while ((c = getopt_long(argc, argv, ":p:vh", long_options, NULL)) != -1) {
#else
  while ((c = getopt(argc, argv, ":p:vh")) != -1) {
#endif
      switch (c) {
        case 'p': // argument: config path
          snprintf(configPath, sizeof(configPath), "%s", optarg);
          break;
        case 'h': // argument: print usage
          printf("%s", usage);
          exit(0);
        case 'v': // argument: print version
          printf(PACKAGE " " VERSION "\n");
          exit(0);
        case ':': // missing argument
          fprintf(stderr, PACKAGE ": error: option requires an argument -- '%c'\n", optopt);
          fprintf(stderr, "Try '%s --help' for more information.\n", PACKAGE);
          exit(1);
        case '?': // unknown option
          if (optopt != 0) {
              fprintf(stderr, PACKAGE ": error: invalid option -- '%c'\n", optopt);
            } else {
              fprintf(stderr, PACKAGE ": error: invalid option\n");
            }
          fprintf(stderr, "Try '%s --help' for more information.\n", PACKAGE);
          exit(1);
        default: // argument: no arguments; exit
          abort();
        }
    }

  if (optind < argc) {
      fprintf(stderr, PACKAGE ": error: unknown argument '%s'\n", argv[optind]);
      fprintf(stderr, "Try '%s --help' for more information.\n", PACKAGE);
      exit(1);
    }
}

static void set_console_title(){
    // general: console title
    printf("%c]0;%s%c", '\033', PACKAGE, '\007');
}

static void setup_signal_handlers() {
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
}

static void setup_tty_environment(struct config_params *cfg, int *inAtty, int *inAterminal) {
#ifndef _WIN32
  if ((output_mode == OUTPUT_NCURSES || output_mode == OUTPUT_NONCURSES) &&
      ttyname(0) != NULL) {
      // Check if we're running in a tty
      if (strncmp(ttyname(0), "/dev/tty", 8) == 0 ||
          strcmp(ttyname(0), "/dev/console") == 0 ||
          strncmp(ttyname(0), "/dev/ttyS", 9) == 0 ||
          strncmp(ttyname(0), "/dev/ttyUSB", 11) == 0)
        *inAtty = 1;

             // Check if we are running in a dumb terminal
      if (strncmp(ttyname(0), "/dev/ttyS", 9) == 0 ||
          strncmp(ttyname(0), "/dev/ttyUSB", 11) == 0)
        *inAterminal = 1;

             // in macos virtual terminals are called ttys(xyz) and there are no ttys
      if (strncmp(ttyname(0), "/dev/ttys", 9) == 0)
        *inAtty = 0;

      if (*inAtty) {
          if (*inAterminal) {
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
              if (cfg->disable_blanking)
                system("setterm -blank 0");
#endif
            }
          if (cfg->orientation != ORIENT_BOTTOM) {
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
}


static void start_audio_thread(struct config_params *cfg, struct audio_data *audio, pthread_t *p_thread) {
  int timeout_counter = 0;
  struct timespec timeout_timer = {.tv_sec = 0, .tv_nsec = 1000000};
  int thr_id GCC_UNUSED;

  switch (cfg->input) {
#ifndef _WIN32

#ifdef ALSA
    case INPUT_ALSA:
      if (is_loop_device_for_sure(audio->source)) {
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

      thr_id = pthread_create(p_thread, NULL, input_alsa, (void *)audio);
      break;
#endif

    case INPUT_FIFO:
      audio->rate = cfg->samplerate;
      audio->format = cfg->samplebits;
      thr_id = pthread_create(p_thread, NULL, input_fifo, (void *)audio);
      break;
#ifdef PULSE
    case INPUT_PULSE:
      audio->format = 16;
      audio->rate = 44100;
      if (strcmp(audio->source, "auto") == 0) {
          getPulseDefaultSink((void *)audio);
        }
      thr_id = pthread_create(p_thread, NULL, input_pulse, (void *)audio);
      break;
#endif
#ifdef SNDIO
    case INPUT_SNDIO:
      audio->format = cfg->samplebits;
      audio->rate = cfg->samplerate;
      audio->channels = cfg->channels;
      audio->threadparams = 1; // Sndio can adjust parameters
      thr_id = pthread_create(p_thread, NULL, input_sndio, (void *)audio);
      break;
#endif
#ifdef OSS
    case INPUT_OSS:
      audio->format = cfg->samplebits;
      audio->rate = cfg->samplerate;
      audio->channels = cfg->channels;
      audio->threadparams = 1; // OSS can adjust parameters
      thr_id = pthread_create(p_thread, NULL, input_oss, (void *)audio);
      break;
#endif
#ifdef JACK
    case INPUT_JACK:
      audio->channels = cfg->channels;
      audio->autoconnect = cfg->autoconnect;
      audio->threadparams = 1; // JACK server provides parameters
      thr_id = pthread_create(p_thread, NULL, input_jack, (void *)audio);
      break;
#endif
    case INPUT_SHMEM:
      audio->format = 16;
      thr_id = pthread_create(p_thread, NULL, input_shmem, (void *)audio);
      break;
#ifdef PORTAUDIO
    case INPUT_PORTAUDIO:
      audio->format = 16;
      audio->rate = 44100;
      audio->threadparams = 1;
      if (!strcmp(audio->source, "list")) {
          input_portaudio((void *)audio);
        } else {
          thr_id = pthread_create(p_thread, NULL, input_portaudio, (void *)audio);
        }
      break;
#endif
#ifdef COREAUDIO
    case INPUT_COREAUDIO:
      audio->format = cfg->samplebits;
      audio->rate = cfg->samplerate;
      audio->channels = cfg->channels;
      audio->threadparams = 1;
      if (!strcmp(audio->source, "list")) {
          input_coreaudio((void *)audio);
#ifdef COREAUDIO_TAP
        } else if (coreaudio_tap_source_enabled(audio->source)) {
          thr_id = pthread_create(p_thread, NULL, input_coreaudio_tap, (void *)audio);
#endif
        } else {
          thr_id = pthread_create(p_thread, NULL, input_coreaudio, (void *)audio);
        }
      break;
#endif
#ifdef PIPEWIRE
    case INPUT_PIPEWIRE:
      audio->format = cfg->samplebits;
      audio->rate = cfg->samplerate;
      audio->channels = cfg->channels;
      audio->active = cfg->active;
      audio->remix = cfg->remix;
      audio->virtual_node = cfg->virtual_node;
      thr_id = pthread_create(p_thread, NULL, input_pipewire, (void *)audio);
      break;
#endif
#endif
#ifdef _WIN32
    case INPUT_WINSCAP:
      thr_id = pthread_create(p_thread, NULL, input_winscap, (void *)audio);
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
      pthread_mutex_lock(&audio->lock);
      if ((audio->threadparams == 0) && (audio->format != -1) && (audio->rate != 0))
        break;

      pthread_mutex_unlock(&audio->lock);
      timeout_counter++;
      if (timeout_counter > 5000) {
          cleanup();
          fprintf(stderr, "could not get rate and/or format, problems with audio thread? "
                           "quitting...\n");
          exit(EXIT_FAILURE);
        }
    }
  pthread_mutex_unlock(&audio->lock);
}

// general: entry point
int main(int argc, char **argv) {

    struct config_params cfg;
    memset(&cfg, 0, sizeof(cfg));
    // handle command-line arguments
    char configPath[PATH_MAX];
    configPath[0] = '\0';

    setup_signal_handlers();
    parse_arguments(argc, argv, configPath);

#ifndef _WIN32
    set_console_title();
#endif // !_WIN32

    // general: main loop
    while (1) {

        // config: load
        struct error_s error;
        error.length = 0;
        if (!load_config(configPath, &cfg, &error)) {
            fprintf(stderr, "Error loading config. %s", error.message);
            exit(EXIT_FAILURE);
        }

        time_t config_mtime = 0;
        long long config_size = 0;
        bool has_config_state = false;
        if (cfg.live_config) {
            has_config_state = get_file_state(configPath, &config_mtime, &config_size);
        }

        int inAtty = 0;
        int inAterminal = 0;

        output_mode = cfg.output;

        setup_tty_environment(&cfg, &inAtty, &inAterminal);

        struct audio_data audio;
        memset(&audio, 0, sizeof(audio));

        audio.source = malloc(1 + strlen(cfg.audio_source));
        strcpy(audio.source, cfg.audio_source);

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
        memset(audio.cava_in, 0, sizeof(double) * audio.cava_buffer_size);

        audio.threadparams = 0; // most input threads don't adjust the parameters
        audio.terminate = 0;

        int total_bar_height = 0;

        pthread_t p_thread;
        pthread_mutex_init(&audio.lock, NULL);

        start_audio_thread(&cfg, &audio, &p_thread);

        int audio_channels = audio.channels;

        if (cfg.upper_cut_off > audio.rate / 2) {
            cleanup();
            fprintf(stderr,
                    "higher cutoff frequency can't be higher than sample rate / 2\nhigher "
                    "cutoff frequency is set to: %d, got sample rate: %d\n",
                    cfg.upper_cut_off, audio.rate);
            exit(EXIT_FAILURE);
        }

        // force stereo if only one channel is available
        if (cfg.stereo && audio_channels == 1)
            cfg.stereo = 0;

        int output_channels = 1;
        if (cfg.stereo)
            output_channels = 2;

        int *bars;
        int *previous_frame;

        int *right_bars = NULL;
        int *right_previous_frame = NULL;

        float *bars_left, *bars_right;
        double *cava_out;
        float *bars_raw;
        float *previous_bars_raw;

        int height, lines, width, remainder;
        int *dimension_bar, *dimension_value;
#ifndef _WIN32
        int fp = -1;
        int keepalive_fd = -1;
        bool close_output_fd = false;
#endif
#ifdef _WIN32
        HANDLE hFile = NULL;
#endif

        if (cfg.orientation == ORIENT_LEFT || cfg.orientation == ORIENT_RIGHT ||
            cfg.orientation == ORIENT_SPLIT_V) {
            dimension_bar = &height;
            dimension_value = &width;
        } else {
            dimension_bar = &width;
            dimension_value = &height;
        }

#ifdef SDL
        // output: start sdl mode
        if (output_mode == OUTPUT_SDL) {
            init_sdl_window(cfg.sdl_width, cfg.sdl_height, cfg.sdl_x, cfg.sdl_y, cfg.sdl_full_screen);
            height = cfg.sdl_height;
            width = cfg.sdl_width;
        }
#endif
#ifdef SDL_GLSL
        if (output_mode == OUTPUT_SDL_GLSL) {
            init_sdl_glsl_window(cfg.sdl_width, cfg.sdl_height, cfg.sdl_x, cfg.sdl_y, cfg.sdl_full_screen,
                                 cfg.vertex_shader, cfg.fragment_shader);
            height = cfg.sdl_height;
            width = cfg.sdl_width;
            *dimension_value = 1;
        }
#endif

        bool reloadConf = false;
        while (!reloadConf) { // jumping back to this loop means that you resized the screen

            // frequencies on x axis require a bar width of four or more
            if (cfg.xaxis == FREQUENCY && cfg.bar_width < 4)
                cfg.bar_width = 4;

            switch (output_mode) {
#ifdef NCURSES
            // output: start ncurses mode
            case OUTPUT_NCURSES:
                init_terminal_ncurses(cfg.color, cfg.bcolor, cfg.col, cfg.bgcol, cfg.gradient,
                                      cfg.gradient_count, cfg.gradient_colors, &width, &lines);
                if (cfg.xaxis != NONE)
                    lines--;
                height = lines;
                *dimension_value *=
                    8; // we have 8 times as much height due to using 1/8 block characters
                break;
#endif
#ifdef SDL
            // output: get sdl window size
            case OUTPUT_SDL:
                init_sdl_surface(&width, &height, cfg.color, cfg.bcolor, cfg.gradient, cfg.gradient_count,
                                 cfg.gradient_colors);
                break;
#endif
#ifdef SDL_GLSL
            // output: get sdl window size
            case OUTPUT_SDL_GLSL:
                init_sdl_glsl_surface(&width, &height, cfg.color, cfg.bcolor, cfg.bar_width,
                                      cfg.bar_spacing, cfg.gradient, cfg.gradient_count,
                                      cfg.gradient_colors);
                break;
#endif
            case OUTPUT_NONCURSES:
                get_terminal_dim_noncurses(&width, &lines);

                if (cfg.xaxis != NONE)
                    lines--;

                height = lines;
                break;
            case OUTPUT_RAW:
            case OUTPUT_NORITAKE:
                if (strcmp(cfg.raw_target, "/dev/stdout") != 0) {
#ifndef _WIN32
                    if (close_output_fd && fp != -1) {
                        close(fp);
                        fp = -1;
                        close_output_fd = false;
                    }
                    if (keepalive_fd != -1) {
                        close(keepalive_fd);
                        keepalive_fd = -1;
                    }
                    // checking if file exists
                    if (access(cfg.raw_target, F_OK) != -1) {
                        // file exists, testopening in case it's a fifo
                        keepalive_fd = open(cfg.raw_target, O_RDONLY | O_NONBLOCK, 0644);

                        if (keepalive_fd == -1) {
                            fprintf(stderr, "could not open file %s for writing\n", cfg.raw_target);
                            exit(1);
                        }
                    } else {
                        printf("creating fifo %s\n", cfg.raw_target);
                        if (mkfifo(cfg.raw_target, 0664) == -1) {
                            fprintf(stderr, "could not create fifo %s\n", cfg.raw_target);
                            exit(1);
                        }
                        // fifo needs to be open for reading in order to write to it
                        keepalive_fd = open(cfg.raw_target, O_RDONLY | O_NONBLOCK, 0644);
                        if (keepalive_fd == -1) {
                            fprintf(stderr, "could not open file %s for writing\n", cfg.raw_target);
                            exit(1);
                        }
                    }
                    fp = open(cfg.raw_target, O_WRONLY | O_NONBLOCK | O_CREAT, 0644);
                    close_output_fd = true;
#else
                    int pipeLength = strlen("\\\\.\\pipe\\") + strlen(cfg.raw_target) + 1;
                    char *pipePath = malloc(pipeLength);
                    pipePath[pipeLength - 1] = '\0';
                    strcat(pipePath, "\\\\.\\pipe\\");
                    strcat(pipePath, cfg.raw_target);
                    DWORD pipeMode = strcmp(cfg.data_format, "ascii")
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
                    fprintf(stderr, "could not open file %s for writing\n", cfg.raw_target);
                    exit(1);
                }

                // width must be hardcoded for raw output. only used to calculate the number of
                // bars in auto mode
                width = 512 * output_channels;
                cfg.bar_width = 1;
                cfg.bar_spacing = 0;

                if (strcmp(cfg.data_format, "ascii") != 0) {
                    // "binary" or "noritake"
                    height = pow(2, cfg.bit_format) - 1;
                } else {
                    height = cfg.ascii_range;
                }
                break;
            default:
                exit(EXIT_FAILURE); // Can't happen.
            }

            // getting numbers of bars
            int number_of_bars = -1;
            // handle for user setting too many bars
            if (cfg.fixedbars) {
                number_of_bars = cfg.fixedbars;
                if (number_of_bars * cfg.bar_width + cfg.fixedbars * cfg.bar_spacing - cfg.bar_spacing >
                    width) {
                    cleanup();
                    fprintf(stderr, "window is too narrow for number of bars set, maximum is %d\n",
                            (width - cfg.bar_spacing) / (cfg.bar_width + cfg.bar_spacing));
                    audio.terminate = 1;
                    exit(EXIT_FAILURE);
                }
                if (number_of_bars < output_channels) {
                    cleanup();
                    fprintf(stderr, "fixed number of bars must be at least 1 with mono output and "
                                    "2 with stereo output\n");
                    exit(EXIT_FAILURE);
                }
                if (output_channels == 2 && number_of_bars % 2 != 0) {
                    cleanup();
                    fprintf(stderr, "must have even number of bars with stereo output\n");
                    exit(EXIT_FAILURE);
                }
            } else {
                number_of_bars = (*dimension_bar + cfg.bar_spacing) / (cfg.bar_width + cfg.bar_spacing);

                if (output_mode == OUTPUT_SDL_GLSL) {
                    if (number_of_bars > 512)
                        number_of_bars = 512; // can't have more than 512 bars in glsl due to shader
                                              // program implementation limitations
                } else {
                    if (number_of_bars / output_channels > 512)
                        number_of_bars = 512 * output_channels; // can't have more than 512 bars on
                                                                // 44100 rate per channel
                }

                if (number_of_bars <= 1) {
                    number_of_bars = 1; // must have at least 1 bars
                    if (cfg.stereo) {
                        number_of_bars = 2; // stereo have at least 2 bars
                    }
                }
                if (output_channels == 2 && number_of_bars % 2 != 0)
                    number_of_bars--; // stereo must have even numbers of bars
            }

            // checks if there is still extra room, will use this to center
            if (cfg.center_align) {
                remainder = (*dimension_bar - number_of_bars * cfg.bar_width -
                             number_of_bars * cfg.bar_spacing + cfg.bar_spacing) /
                            2;
                if (remainder < 0)
                    remainder = 0;
            } else {
                remainder = 0;
            }

            if (output_mode == OUTPUT_NONCURSES) {
                init_terminal_noncurses(inAtty, cfg.color, cfg.bcolor, cfg.col, cfg.bgcol, cfg.gradient,
                                        cfg.gradient_count, cfg.gradient_colors, cfg.horizontal_gradient,
                                        cfg.horizontal_gradient_count, cfg.horizontal_gradient_colors,
                                        number_of_bars, width, lines, cfg.bar_width, cfg.orientation,
                                        cfg.blendDirection);
            }
            if ((cfg.orientation == ORIENT_SPLIT_H || cfg.orientation == ORIENT_SPLIT_V) &&
                cfg.split_stereo) {
                number_of_bars *= 2;
            }
            cfg.number_of_bars = number_of_bars;
            cfg.terminal_lines = lines;
            cfg.terminal_width = width;
            cfg.terminal_remainder = remainder;
            cfg.is_tty = inAtty;

            int raw_number_of_bars = (number_of_bars / output_channels) * audio_channels;
            if (cfg.waveform) {
                raw_number_of_bars = number_of_bars;
            }
            double userEQ_keys_to_bars_ratio;

            if (cfg.userEQ_enabled && (number_of_bars / output_channels > 0)) {
                userEQ_keys_to_bars_ratio = (double)(((double)cfg.userEQ_keys) /
                                                     ((double)(number_of_bars / output_channels)));
            }

            struct cava_plan *plan =
                cava_init(number_of_bars / output_channels, audio.rate, audio.channels, cfg.autosens,
                          cfg.noise_reduction, cfg.lower_cut_off, cfg.upper_cut_off);

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

            if (cfg.split_stereo) {
                right_bars = (int *)malloc(number_of_bars * sizeof(int));
                right_previous_frame = (int *)malloc(number_of_bars * sizeof(int));
                memset(right_bars, 0, sizeof(int) * number_of_bars);
                memset(right_previous_frame, 0, sizeof(int) * number_of_bars);
            }

            // process: calculate x axis values
            if (cfg.xaxis != NONE) {
                double cut_off_frequency;
                if (output_mode == OUTPUT_NONCURSES) {
                    printf("\r\033[%dB", lines + 1);
                    if (remainder)
                        printf("\033[%dC", remainder);
                }
                for (int n = 0; n < number_of_bars; n++) {
                    if (cfg.stereo) {
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
                            mvprintw(lines, n * (cfg.bar_width + cfg.bar_spacing) + remainder, "%-4d",
                                     freq_floor);
                        else if (cut_off_frequency > 1000 && cut_off_frequency < 10000)
                            mvprintw(lines, n * (cfg.bar_width + cfg.bar_spacing) + remainder, "%.2f",
                                     freq_kilohz);
                        else
                            mvprintw(lines, n * (cfg.bar_width + cfg.bar_spacing) + remainder, "%.1f",
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
                            printf("\033[%dC", cfg.bar_width + cfg.bar_spacing - 4);
                    }
                }
                printf("\r\033[%dA", lines + 1);
            }

            bool resizeTerminal = false;

            long long frame_time_ns = (long long)(1000000000.0 / (double)cfg.framerate);
            if (frame_time_ns < 1)
                frame_time_ns = 1;
            struct timespec sleep_timer = {.tv_sec = frame_time_ns / 1000000000,
                                           .tv_nsec = frame_time_ns % 1000000000};
            int frame_time_msec = (int)(frame_time_ns / 1000000LL);
            if (frame_time_msec < 1)
                frame_time_msec = 1;

#ifdef _WIN32
            LARGE_INTEGER frequency; // ticks per second
            LARGE_INTEGER t1, t2;    // ticks
            double elapsedTime;
            QueryPerformanceFrequency(&frequency);
#else
            struct timespec t1, t2, t3;
            double elapsedTimens;
            double wakeupTimens = 0.0;
#endif // _WIN32

            int sleep_counter = 0;
            bool silence = false;
            char ch = '\0';

            struct timespec sleep_mode_timer = {.tv_sec = 1, .tv_nsec = 0};

            int total_frames = 0;

            // audio samples to use per visual frame.
            int samples_per_frame = audio.rate / cfg.framerate;

            // on sdl we use SDL_delay instead of our own sleep and it is in whole milliseconds, so
            // we need to adjust a little
            if (output_mode == OUTPUT_SDL_GLSL || output_mode == OUTPUT_SDL) {
                float actual_framerate = 1000.0 / (float)frame_time_msec;
                samples_per_frame = audio.rate / actual_framerate;
            }

            // check if drawing loop is faster than audio loop.
            int high_framerate = 0;
            if (samples_per_frame < audio.input_buffer_size / audio_channels) {
                high_framerate = 1;
            }

            pthread_mutex_lock(&audio.lock);
            audio.samples_counter = 0;
            pthread_mutex_unlock(&audio.lock);

            while (!resizeTerminal) {
#ifdef _WIN32
                QueryPerformanceCounter(&t1);
#else
                clock_gettime(CLOCK_MONOTONIC, &t1);
#endif

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
                    cfg.sens = cfg.sens * 1.05;
                    break;
                case 66: // key down
                    cfg.sens = cfg.sens * 0.95;
                    break;
                case 68: // key right
                    cfg.bar_width++;
                    resizeTerminal = true;
                    break;
                case 67: // key left
                    if (cfg.bar_width > 1)
                        cfg.bar_width--;
                    resizeTerminal = true;
                    break;
                case 'r': // reload config
                    should_reload = 1;
                    break;
                case 'c': // reload colors
                    reload_colors = 1;
                    break;
                case 'f': // change foreground color
                    if (cfg.col < 7)
                        cfg.col++;
                    else
                        cfg.col = 0;
                    resizeTerminal = true;
                    break;
                case 'b': // change background color
                    if (cfg.bgcol < 7)
                        cfg.bgcol++;
                    else
                        cfg.bgcol = 0;
                    resizeTerminal = true;
                    break;
                case 'o': // change orientation
                    cfg.orientation++;
                    if (output_mode == OUTPUT_NONCURSES) {
                        if (cfg.orientation > ORIENT_SPLIT_V) {
                            cfg.orientation = ORIENT_BOTTOM;
                        }
                    } else if (output_mode == OUTPUT_NCURSES) {
                        if (cfg.orientation > ORIENT_RIGHT) {
                            cfg.orientation = ORIENT_BOTTOM;
                        }
                    } else {
                        cfg.orientation =
                            (cfg.orientation == ORIENT_BOTTOM) ? ORIENT_TOP : ORIENT_BOTTOM;
                    }

                    if (cfg.orientation == ORIENT_LEFT || cfg.orientation == ORIENT_RIGHT ||
                        cfg.orientation == ORIENT_SPLIT_V) {
                        dimension_bar = &height;
                        dimension_value = &width;
                    } else {
                        dimension_bar = &width;
                        dimension_value = &height;
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
                    char *themeFile;
                    error.length = 0;
                    bool result = get_themeFile(configPath, &cfg, NULL, &error, &themeFile);
                    if (!result) {
                        cleanup();
                        exit(EXIT_FAILURE);
                    }
                    if (!load_colors(themeFile, (void *)&cfg, &error)) {
                        cleanup();
                        free(themeFile);
                        fprintf(stderr, "Error loading config. %s", error.message);
                        exit(EXIT_FAILURE);
                    }
                    resizeTerminal = true;
                    reload_colors = 0;
                    free(themeFile);
                    break;
                }

                if (resizeTerminal)
                    break;

                // checking if audio thread has exited unexpectedly
                pthread_mutex_lock(&audio.lock);
                if (audio.terminate == 1) {
                    cleanup();
                    fprintf(stderr, "Audio thread exited unexpectedly. %s\n", audio.error_message);
                    exit(EXIT_FAILURE);
                }
                pthread_mutex_unlock(&audio.lock);

                // process: check if input is present
                silence = true;

                for (int n = 0; n < audio.samples_counter; n++) {
                    if (audio.cava_in[n]) {
                        silence = false;
                        break;
                    }
                }
#ifndef _WIN32

                if (output_mode != OUTPUT_SDL) {
                    if (cfg.sleep_timer) {
                        int effective_framerate = cfg.framerate;
                        if (effective_framerate <= 0)
                            effective_framerate = 60;
                        int sleep_limit = effective_framerate * cfg.sleep_timer;
                        if (silence && sleep_counter <= sleep_limit)
                            sleep_counter++;
                        else if (!silence)
                            sleep_counter = 0;

                        if (sleep_counter > sleep_limit) {
                            nanosleep(&sleep_mode_timer, NULL);
                            if (cfg.live_config) {
                                time_t new_mtime = 0;
                                long long new_size = 0;
                                if (get_file_state(configPath, &new_mtime, &new_size) &&
                                    (!has_config_state || new_mtime != config_mtime ||
                                     new_size != config_size)) {
                                    should_reload = 1;
                                    config_mtime = new_mtime;
                                    config_size = new_size;
                                    has_config_state = true;
                                }
                            }
                            continue;
                        }
                    }
                }

#endif // !_WIN32

                // process: execute cava
                pthread_mutex_lock(&audio.lock);

                int samples_to_use = 0;

                if (!high_framerate) {
                    // we dont need buffers and use all available samples.
                    samples_to_use = audio.samples_counter;
                } else {
                    // we are running with higher framerates and need buffers!
                    // only use the calculated samples per frame
                    samples_to_use = samples_per_frame * audio_channels;

                    // buffer underrun! only use what we have
                    if (audio.samples_counter < samples_to_use) {
                        samples_to_use = audio.samples_counter;
                    }

                    // buffer overflow! we have more samples than we need, just use them.
                    if (audio.samples_counter > audio.input_buffer_size + samples_to_use) {
                        samples_to_use = audio.samples_counter - audio.input_buffer_size;
                    }
                }

                if (cfg.waveform) {
                    for (int n = 0; n < samples_to_use; n++) {

                        for (int i = number_of_bars - 1; i > 0; i--) {
                            cava_out[i] = cava_out[i - 1];
                        }
                        if (audio_channels == 2) {
                            cava_out[0] =
                                cfg.sens * (audio.cava_in[n] / 2 + audio.cava_in[n + 1] / 2);
                            n++;
                        } else {
                            cava_out[0] = cfg.sens * audio.cava_in[n];
                        }
                    }
                } else {
                    cava_execute(audio.cava_in, samples_to_use, cava_out, plan);
                }

                audio.samples_counter -= samples_to_use;

                if (audio.samples_counter != 0) {
                    // shift the input buffer
                    for (int n = 0; n < audio.samples_counter; n++) {
                        audio.cava_in[n] = audio.cava_in[n + samples_to_use];
                    }
                }

                pthread_mutex_unlock(&audio.lock);

                for (int n = 0; n < raw_number_of_bars; n++) {

                    if (!cfg.waveform) {
                        cava_out[n] *= cfg.sens;
                    } else {
                        if (cava_out[n] > 1.0)
                            cfg.sens *= 0.999;
                        else
                            cfg.sens *= 1.00001;

                        if (cfg.orientation != ORIENT_SPLIT_H)
                            cava_out[n] = (cava_out[n] + 1.0) / 2.0;
                    }

                    if (cfg.sdl_glsl_gain != 1.0) {
                        cava_out[n] *= cfg.sdl_glsl_gain;
                    }

                    if (cava_out[n] > 1.0)
                        cava_out[n] = 1.0;
                    else if (cava_out[n] < 0.0)
                        cava_out[n] = 0.0;

                    if (output_mode != OUTPUT_SDL_GLSL) {
                        cava_out[n] *= *dimension_value;
                    }
                    if (cfg.orientation == ORIENT_SPLIT_H || cfg.orientation == ORIENT_SPLIT_V) {
                        cava_out[n] /= 2;
                    }
                    if (output_mode == OUTPUT_NONCURSES) {
                        cava_out[n] *= 8 * cfg.max_height;
                    }

                    if (cfg.waveform) {
                        bars_raw[n] = cava_out[n];
                    }
                }

                if (!cfg.waveform) {
                    if (audio_channels == 2) {
                        for (int n = 0; n < number_of_bars / output_channels; n++) {
                            if (cfg.userEQ_enabled)
                                cava_out[n] *=
                                    cfg.userEQ[(int)floor(((double)n) * userEQ_keys_to_bars_ratio)];
                            bars_left[n] = cava_out[n];
                        }
                        for (int n = 0; n < number_of_bars / output_channels; n++) {
                            if (cfg.userEQ_enabled)
                                cava_out[n + number_of_bars / output_channels] *=
                                    cfg.userEQ[(int)floor(((double)n) * userEQ_keys_to_bars_ratio)];
                            bars_right[n] = cava_out[n + number_of_bars / output_channels];
                        }
                    } else {
                        for (int n = 0; n < number_of_bars; n++) {
                            if (cfg.userEQ_enabled)
                                cava_out[n] *=
                                    cfg.userEQ[(int)floor(((double)n) * userEQ_keys_to_bars_ratio)];
                            bars_raw[n] = cava_out[n];
                        }
                    }

                    // process [filter]
                    if (cfg.monstercat) {
                        if (audio_channels == 2) {
                            bars_left =
                                monstercat_filter(bars_left, number_of_bars / output_channels,
                                                  cfg.waves, cfg.monstercat, *dimension_value);
                            bars_right =
                                monstercat_filter(bars_right, number_of_bars / output_channels,
                                                  cfg.waves, cfg.monstercat, *dimension_value);
                        } else {
                            bars_raw = monstercat_filter(bars_raw, number_of_bars, cfg.waves,
                                                         cfg.monstercat, *dimension_value);
                        }
                    }
                    if (audio_channels == 2) {
                        if (cfg.stereo) {
                            // mirroring stereo channels
                            if ((cfg.orientation == ORIENT_SPLIT_H ||
                                 cfg.orientation == ORIENT_SPLIT_V) &&
                                cfg.split_stereo) {
                                for (int n = 0; n < number_of_bars; n++) {
                                    if (n < number_of_bars / 2) {
                                        if (cfg.reverse) {
                                            bars_raw[n] = bars_left[number_of_bars / 2 - n - 1];
                                        } else {
                                            bars_raw[n] = bars_left[n];
                                        }
                                    } else {
                                        if (cfg.reverse) {
                                            bars_raw[n] = bars_right[number_of_bars - n - 1];
                                        } else {
                                            bars_raw[n] = bars_right[n - number_of_bars / 2];
                                        }
                                    }
                                }

                            } else {
                                for (int n = 0; n < number_of_bars; n++) {
                                    if (n < number_of_bars / 2) {
                                        if (cfg.reverse) {
                                            bars_raw[n] = bars_left[n];
                                        } else {
                                            bars_raw[n] = bars_left[number_of_bars / 2 - n - 1];
                                        }
                                    } else {
                                        if (cfg.reverse) {
                                            bars_raw[n] = bars_right[number_of_bars - n - 1];
                                        } else {
                                            bars_raw[n] = bars_right[n - number_of_bars / 2];
                                        }
                                    }
                                }
                            }
                        } else {

                            // stereo mono output
                            for (int n = 0; n < number_of_bars; n++) {
                                if (cfg.reverse) {
                                    if (cfg.mono_opt == AVERAGE) {
                                        bars_raw[number_of_bars - n - 1] =
                                            (bars_left[n] + bars_right[n]) / 2;
                                    } else if (cfg.mono_opt == LEFT) {
                                        bars_raw[number_of_bars - n - 1] = bars_left[n];
                                    } else if (cfg.mono_opt == RIGHT) {
                                        bars_raw[number_of_bars - n - 1] = bars_right[n];
                                    }
                                } else {
                                    if (cfg.mono_opt == AVERAGE) {
                                        bars_raw[n] = (bars_left[n] + bars_right[n]) / 2;
                                    } else if (cfg.mono_opt == LEFT) {
                                        bars_raw[n] = bars_left[n];
                                    } else if (cfg.mono_opt == RIGHT) {
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
                        bars[n] < 1 && cfg.waveform == 0 && cfg.show_idle_bar_heads == 1)
                        bars[n] = 1;
#ifdef SDL_GLSL

                    if (output_mode == OUTPUT_SDL_GLSL)
                        bars[n] =
                            bars_raw[n] * 1000; // values are 0-1, only used to check for changes

                    if (bars[n] != previous_frame[n])
                        re_paint = 1;
#endif
                }

                // output: draw processed input
                if (cfg.sync_updates) {
                    printf("\033[2026h\033\\");
                    fflush(stdout);
                    printf("\033[2026l\033\\");
                }
                int rc = 0;
                switch (output_mode) {
#ifdef SDL
                case OUTPUT_SDL:
                    rc = draw_sdl(number_of_bars, cfg.bar_width, cfg.bar_spacing, remainder,
                                  *dimension_value, bars, previous_frame, frame_time_msec,
                                  cfg.orientation, cfg.gradient);

                    break;
#endif
#ifdef SDL_GLSL
                case OUTPUT_SDL_GLSL:
                    rc = draw_sdl_glsl(number_of_bars, bars_raw, previous_bars_raw, frame_time_msec,
                                       re_paint, cfg.continuous_rendering);
                    break;
#endif
                case OUTPUT_NONCURSES:
                    if (cfg.orientation == ORIENT_SPLIT_H || cfg.orientation == ORIENT_SPLIT_V) {
                        // in split horizontal mode we need to draw two times
                        // once for bottom and once for top
                        // ORIENT_BOTTOM will be the top bars and ORIENT_TOP the bottom bars
                        // since bottom hear means from mid upwards and top is from mid
                        // downwards

                        if (cfg.split_stereo) {
                            // in horizontal stereo mode we need to split the bars array in half
                            // first half is right channel, second half is left channel
                            for (int i = 0; i < number_of_bars / 2; i++) {
                                right_bars[i] = bars[i + number_of_bars / 2];
                                right_previous_frame[i] = previous_frame[i + number_of_bars / 2];
                            }
                            if (cfg.orientation == ORIENT_SPLIT_H) {
                                rc = draw_terminal_noncurses(bars, previous_frame, ORIENT_BOTTOM,
                                                             &cfg);
                                rc = draw_terminal_noncurses(right_bars, right_previous_frame,
                                                             ORIENT_TOP, &cfg);
                            }
                            if (cfg.orientation == ORIENT_SPLIT_V) {
                                rc = draw_terminal_noncurses(right_bars, right_previous_frame,
                                                             ORIENT_LEFT, &cfg);
                                rc =
                                    draw_terminal_noncurses(bars, previous_frame, ORIENT_RIGHT, &cfg);
                            }

                        } else {
                            // pure mirrored split, same bars for both sides
                            if (cfg.orientation == ORIENT_SPLIT_H) {
                                rc = draw_terminal_noncurses(bars, previous_frame, ORIENT_BOTTOM,
                                                             &cfg);
                                rc = draw_terminal_noncurses(bars, previous_frame, ORIENT_TOP, &cfg);
                            } else if (cfg.orientation == ORIENT_SPLIT_V) {
                                rc = draw_terminal_noncurses(bars, previous_frame, ORIENT_LEFT, &cfg);
                                rc =
                                    draw_terminal_noncurses(bars, previous_frame, ORIENT_RIGHT, &cfg);
                            }
                        }
                    } else {
                        rc = draw_terminal_noncurses(bars, previous_frame, cfg.orientation, &cfg);
                    }
                    break;
                case OUTPUT_NCURSES:
#ifdef NCURSES
                    rc = draw_terminal_ncurses(inAtty, *dimension_value / 8, *dimension_bar,
                                               number_of_bars, cfg.bar_width, cfg.bar_spacing,
                                               remainder, bars, previous_frame, cfg.gradient, cfg.xaxis,
                                               cfg.orientation);
                    break;
#endif
                case OUTPUT_RAW:
#ifndef _WIN32
                    rc = print_raw_out(number_of_bars, fp, cfg.raw_format, cfg.bit_format,
                                       cfg.ascii_range, cfg.bar_delim, cfg.frame_delim, bars);
#else
                    rc = print_raw_out(number_of_bars, hFile, cfg.raw_format, cfg.bit_format,
                                       cfg.ascii_range, cfg.bar_delim, cfg.frame_delim, bars);
#endif
                    break;
                case OUTPUT_NORITAKE:
#ifndef _WIN32
                    rc = print_ntk_out(number_of_bars, fp, cfg.bit_format, cfg.bar_width, cfg.bar_spacing,
                                       cfg.bar_height, bars);
#else
                    rc = print_ntk_out(number_of_bars, hFile, cfg.bit_format, cfg.bar_width,
                                       cfg.bar_spacing, cfg.bar_height, bars);
#endif
                    break;
                default:
                    exit(EXIT_FAILURE); // Can't happen.
                }

                if (cfg.sync_updates) {
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

                memcpy(previous_frame, bars, number_of_bars * sizeof(int));
                if (cfg.output == OUTPUT_SDL_GLSL) {
                    memcpy(previous_bars_raw, bars_raw, number_of_bars * sizeof(float));
                }

                if (cfg.live_config) {
                    time_t new_mtime = 0;
                    long long new_size = 0;
                    if (get_file_state(configPath, &new_mtime, &new_size) &&
                        (!has_config_state || new_mtime != config_mtime ||
                         new_size != config_size)) {
                        should_reload = 1;
                        config_mtime = new_mtime;
                        config_size = new_size;
                        has_config_state = true;
                    }
                }

                if (cfg.draw_and_quit > 0) {
                    total_frames++;
                    if (total_frames >= cfg.draw_and_quit) {
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
                    clock_gettime(CLOCK_MONOTONIC, &t2);
                    elapsedTimens =
                        (t2.tv_sec - t1.tv_sec) * 1000000000.0 + (t2.tv_nsec - t1.tv_nsec);

                    int sleep_time_ns = frame_time_ns - (int)elapsedTimens - (int)wakeupTimens;

                    if (sleep_time_ns > 1) {

                        sleep_timer.tv_sec = sleep_time_ns / 1000000000;
                        sleep_timer.tv_nsec = sleep_time_ns % 1000000000;
                        nanosleep(&sleep_timer, NULL);
                        clock_gettime(CLOCK_MONOTONIC, &t3);
                        double actualSleeptime =
                            (t3.tv_sec - t2.tv_sec) * 1000000000.0 + (t3.tv_nsec - t2.tv_nsec);
                        wakeupTimens = actualSleeptime - sleep_time_ns;
                    } else {
                        wakeupTimens = 0;
                    }

#endif
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
            if (cfg.split_stereo) {
                free(right_bars);
                free(right_previous_frame);
            }

#ifndef _WIN32
            if ((output_mode == OUTPUT_RAW || output_mode == OUTPUT_NORITAKE) &&
                strcmp(cfg.raw_target, "/dev/stdout") != 0) {
                if (close_output_fd && fp != -1) {
                    close(fp);
                    fp = -1;
                    close_output_fd = false;
                }
                if (keepalive_fd != -1) {
                    close(keepalive_fd);
                    keepalive_fd = -1;
                }
            }
#endif
        } // reloading config

        //**telling audio thread to terminate**//
        pthread_mutex_lock(&audio.lock);
        audio.terminate = 1;
        pthread_mutex_unlock(&audio.lock);
        pthread_join(p_thread, NULL);

        pthread_mutex_destroy(&audio.lock);

        free(audio.source);
        free(audio.cava_in);
        cleanup();
        free_config(&cfg);

        if (should_quit && signal_received == 0) {
            if (cfg.zero_test && total_bar_height > 0) {
                fprintf(stderr, "Test mode: expected total bar height to be zero, but was: %d\n",
                        total_bar_height);
                return EXIT_FAILURE;
            } else if (cfg.non_zero_test && total_bar_height == 0) {
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
