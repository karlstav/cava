#include "common.h"

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

int audio_raw_init(struct audio_data *audio, struct audio_raw *audio_raw, struct config_params *prm,
                   struct cava_plan **plan) {
    audio_raw->channels = audio->channels;

    if (prm->upper_cut_off > audio->rate / 2) {
        cleanup(prm->output);
        fprintf(stderr,
                "higher cutoff frequency can't be higher than sample rate / 2\nhigher "
                "cutoff frequency is set to: %d, got sample rate: %d\n",
                prm->upper_cut_off, audio->rate);
        exit(EXIT_FAILURE);
    }

    // force stereo if only one channel is available
    if (prm->stereo && audio_raw->channels == 1)
        prm->stereo = 0;

    audio_raw->output_channels = 1;
    if (prm->stereo)
        audio_raw->output_channels = 2;

    if (prm->orientation == ORIENT_LEFT || prm->orientation == ORIENT_RIGHT) {
        audio_raw->dimension_bar = &audio_raw->height;
        audio_raw->dimension_value = &audio_raw->width;
    } else {
        audio_raw->dimension_bar = &audio_raw->width;
        audio_raw->dimension_value = &audio_raw->height;
    }

    // frequencies on x axis require a bar width of four or more
    if (prm->xaxis == FREQUENCY && prm->bar_width < 4)
        prm->bar_width = 4;

    switch (prm->output) {
#ifdef NCURSES
    // output: start ncurses mode
    case OUTPUT_NCURSES:
        init_terminal_ncurses(prm->color, prm->bcolor, prm->col, prm->bgcol, prm->gradient,
                              prm->gradient_count, prm->gradient_colors, &audio_raw->width,
                              &audio_raw->lines);
        if (prm->xaxis != NONE)
            audio_raw->lines--;

        audio_raw->height = audio_raw->lines * 8 * prm->max_height;
        *audio_raw->dimension_value *=
            8; // we have 8 times as much height due to using 1/8 block characters
        break;
#endif
#ifdef SDL
    // output: get sdl window size
    case OUTPUT_SDL:
        init_sdl_surface(&audio_raw->width, &audio_raw->height, prm->color, prm->bcolor,
                         prm->gradient, prm->gradient_count, prm->gradient_colors);
        break;
#endif
#ifdef SDL_GLSL
    // output: get sdl window size
    case OUTPUT_SDL_GLSL:
        init_sdl_glsl_surface(&audio_raw->width, &audio_raw->height, prm->color, prm->bcolor,
                              prm->bar_width, prm->bar_spacing, prm->gradient, prm->gradient_count,
                              prm->gradient_colors);
        break;
#endif
    case OUTPUT_NONCURSES:
        get_terminal_dim_noncurses(&audio_raw->width, &audio_raw->lines);

        if (prm->xaxis != NONE)
            audio_raw->lines--;

        audio_raw->height = audio_raw->lines * 8;
        break;
    case OUTPUT_RAW:
    case OUTPUT_NORITAKE:
        if (strcmp(prm->raw_target, "/dev/stdout") != 0) {
#ifndef _WIN32
            int fptest;
            // checking if file exists
            if (access(prm->raw_target, F_OK) != -1) {
                // file exists, testopening in case it's a fifo
                fptest = open(prm->raw_target, O_RDONLY | O_NONBLOCK, 0644);

                if (fptest == -1) {
                    fprintf(stderr, "could not open file %s for writing\n", prm->raw_target);
                    exit(1);
                }
            } else {
                printf("creating fifo %s\n", prm->raw_target);
                if (mkfifo(prm->raw_target, 0664) == -1) {
                    fprintf(stderr, "could not create fifo %s\n", prm->raw_target);
                    exit(1);
                }
                // fifo needs to be open for reading in order to write to it
                fptest = open(prm->raw_target, O_RDONLY | O_NONBLOCK, 0644);
            }
            prm->fp = open(prm->raw_target, O_WRONLY | O_NONBLOCK | O_CREAT, 0644);
#else
            int pipeLength = strlen("\\\\.\\pipe\\") + strlen(prm->raw_target) + 1;
            char *pipePath = malloc(pipeLength);
            pipePath[pipeLength - 1] = '\0';
            strcat(pipePath, "\\\\.\\pipe\\");
            strcat(pipePath, prm->raw_target);
            DWORD pipeMode = strcmp(prm->data_format, "ascii")
                                 ? PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE
                                 : PIPE_TYPE_BYTE | PIPE_READMODE_BYTE;
            prm->hFile = CreateNamedPipeA(pipePath, PIPE_ACCESS_OUTBOUND | FILE_FLAG_OVERLAPPED,
                                          pipeMode | PIPE_NOWAIT, PIPE_UNLIMITED_INSTANCES, 0, 0,
                                          NMPWAIT_USE_DEFAULT_WAIT, NULL);
            free(pipePath);
#endif
        } else {
#ifndef _WIN32
            prm->fp = fileno(stdout);
#else
            prm->hFile = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
        }
#ifndef _WIN32
        if (prm->fp == -1) {
#else
        if (prm->hFile == INVALID_HANDLE_VALUE) {
#endif
            fprintf(stderr, "could not open file %s for writing\n", prm->raw_target);
            exit(1);
        }

        // width must be hardcoded for raw output. only used to calculate the number of
        // bars in auto mode
        audio_raw->width = 512 * audio_raw->output_channels;
        prm->bar_width = 1;
        prm->bar_spacing = 0;

        if (strcmp(prm->data_format, "ascii") != 0) {
            // "binary" or "noritake"
            audio_raw->height = pow(2, prm->bit_format) - 1;
        } else {
            audio_raw->height = prm->ascii_range;
        }
        break;
    default:
        exit(EXIT_FAILURE); // Can't happen.
    }

    // getting numbers of bars
    audio_raw->number_of_bars = prm->fixedbars;

    // handle for user setting too many bars
    if (prm->fixedbars) {
        audio_raw->number_of_bars = prm->fixedbars;
        if (audio_raw->number_of_bars * prm->bar_width + prm->fixedbars * prm->bar_spacing -
                prm->bar_spacing >
            audio_raw->width) {
            cleanup(prm->output);
            fprintf(stderr, "window is too narrow for number of bars set, maximum is %d\n",
                    (audio_raw->width - prm->bar_spacing) / (prm->bar_width + prm->bar_spacing));
            audio->terminate = 1;
            exit(EXIT_FAILURE);
        }
        if (audio_raw->number_of_bars < audio_raw->output_channels) {
            cleanup(prm->output);
            fprintf(stderr, "fixed number of bars must be at least 1 with mono output and "
                            "2 with stereo output\n");
            exit(EXIT_FAILURE);
        }
        if (audio_raw->output_channels == 2 && audio_raw->number_of_bars % 2 != 0) {
            cleanup(prm->output);
            fprintf(stderr, "must have even number of bars with stereo output\n");
            exit(EXIT_FAILURE);
        }
    } else {
        audio_raw->number_of_bars =
            (*audio_raw->dimension_bar + prm->bar_spacing) / (prm->bar_width + prm->bar_spacing);

        if (prm->output == OUTPUT_SDL_GLSL) {
            if (audio_raw->number_of_bars > 512)
                audio_raw->number_of_bars = 512; // cant have more than 512 bars in glsl due to
                                                 // shader program implementation limitations
        } else {
            if (audio_raw->number_of_bars / audio_raw->output_channels > 512)
                audio_raw->number_of_bars =
                    512 * audio_raw->output_channels; // cant have more than 512 bars on
                                                      // 44100 rate per channel
        }

        if (audio_raw->number_of_bars <= 1) {
            audio_raw->number_of_bars = 1; // must have at least 1 bars
            if (prm->stereo) {
                audio_raw->number_of_bars = 2; // stereo have at least 2 bars
            }
        }
        if (audio_raw->output_channels == 2 && audio_raw->number_of_bars % 2 != 0)
            audio_raw->number_of_bars--; // stereo must have even numbers of bars
    }

    // checks if there is stil extra room, will use this to center
    if (prm->center_align) {
        audio_raw->remainder =
            (*audio_raw->dimension_bar - audio_raw->number_of_bars * prm->bar_width -
             audio_raw->number_of_bars * prm->bar_spacing + prm->bar_spacing) /
            2;
        if (audio_raw->remainder < 0)
            audio_raw->remainder = 0;
    } else {
        audio_raw->remainder = 0;
    }

    if (prm->output == OUTPUT_NONCURSES) {
        init_terminal_noncurses(prm->inAtty, prm->color, prm->bcolor, prm->col, prm->bgcol,
                                prm->gradient, prm->gradient_count, prm->gradient_colors,
                                prm->horizontal_gradient, prm->horizontal_gradient_count,
                                prm->horizontal_gradient_colors, audio_raw->number_of_bars,
                                audio_raw->width, audio_raw->lines, prm->bar_width,
                                prm->orientation, prm->blendDirection);
    }

    if (prm->userEQ_enabled && (audio_raw->number_of_bars / audio_raw->output_channels > 0)) {
        audio_raw->userEQ_keys_to_bars_ratio =
            (double)(((double)prm->userEQ_keys) /
                     ((double)(audio_raw->number_of_bars / audio_raw->output_channels)));
    }

    *plan = cava_init(audio_raw->number_of_bars / audio_raw->output_channels, audio->rate,
                      audio->channels, prm->autosens, prm->noise_reduction, prm->lower_cut_off,
                      prm->upper_cut_off);

    if ((*plan)->status == -1) {
        cleanup(prm->output);
        fprintf(stderr, "Error initializing cava . %s", (*plan)->error_message);
        exit(EXIT_FAILURE);
    }

    // if the sample rate is unusual high or low, we need to adjust the input buffer size
    // after the audio thread has started
    if ((*plan)->input_buffer_size != audio->cava_buffer_size) {
        pthread_mutex_lock(&audio->lock);
        audio->cava_buffer_size = (*plan)->input_buffer_size;
        free(audio->cava_in);
        audio->cava_in = (double *)malloc(audio->cava_buffer_size * sizeof(double));
        memset(audio->cava_in, 0, sizeof(double) * audio->cava_buffer_size);
        pthread_mutex_unlock(&audio->lock);
    }

    audio_raw->bars_left = (float *)malloc(
        audio_raw->number_of_bars / audio_raw->output_channels * sizeof(float) + sizeof(float));
    audio_raw->bars_right = (float *)malloc(
        audio_raw->number_of_bars / audio_raw->output_channels * sizeof(float) + sizeof(float));
    memset(audio_raw->bars_left, 0,
           sizeof(float) * audio_raw->number_of_bars / audio_raw->output_channels);
    memset(audio_raw->bars_right, 0,
           sizeof(float) * audio_raw->number_of_bars / audio_raw->output_channels);

    audio_raw->bars = (int *)malloc(audio_raw->number_of_bars * sizeof(int));
    audio_raw->bars_raw = (float *)malloc(audio_raw->number_of_bars * sizeof(float));
    audio_raw->previous_bars_raw = (float *)malloc(audio_raw->number_of_bars * sizeof(float));
    audio_raw->previous_frame = (int *)malloc(audio_raw->number_of_bars * sizeof(int));
    audio_raw->cava_out = (double *)malloc(audio_raw->number_of_bars * audio->channels /
                                           audio_raw->output_channels * sizeof(double));

    memset(audio_raw->bars, 0, sizeof(int) * audio_raw->number_of_bars);
    memset(audio_raw->bars_raw, 0, sizeof(float) * audio_raw->number_of_bars);
    memset(audio_raw->previous_bars_raw, 0, sizeof(float) * audio_raw->number_of_bars);
    memset(audio_raw->previous_frame, 0, sizeof(int) * audio_raw->number_of_bars);
    memset(audio_raw->cava_out, 0,
           sizeof(double) * audio_raw->number_of_bars * audio->channels /
               audio_raw->output_channels);

    // process: calculate x axis values
    prm->x_axis_info = 0;

    if (prm->xaxis != NONE) {
        prm->x_axis_info = 1;
        double cut_off_frequency;
        if (prm->output == OUTPUT_NONCURSES) {
            printf("\r\033[%dB", audio_raw->lines + 1);
            if (audio_raw->remainder)
                printf("\033[%dC", audio_raw->remainder);
        }
        for (int n = 0; n < audio_raw->number_of_bars; n++) {
            if (prm->stereo) {
                if (n < audio_raw->number_of_bars / 2)
                    cut_off_frequency =
                        (*plan)->cut_off_frequency[audio_raw->number_of_bars / 2 - 1 - n];
                else
                    cut_off_frequency =
                        (*plan)->cut_off_frequency[n - audio_raw->number_of_bars / 2];
            } else {
                cut_off_frequency = (*plan)->cut_off_frequency[n];
            }

            float freq_kilohz = cut_off_frequency / 1000;
            int freq_floor = cut_off_frequency;

            if (prm->output == OUTPUT_NCURSES) {
#ifdef NCURSES
                if (cut_off_frequency < 1000)
                    mvprintw(audio_raw->lines,
                             n * (prm->bar_width + prm->bar_spacing) + audio_raw->remainder, "%-4d",
                             freq_floor);
                else if (cut_off_frequency > 1000 && cut_off_frequency < 10000)
                    mvprintw(audio_raw->lines,
                             n * (prm->bar_width + prm->bar_spacing) + audio_raw->remainder, "%.2f",
                             freq_kilohz);
                else
                    mvprintw(audio_raw->lines,
                             n * (prm->bar_width + prm->bar_spacing) + audio_raw->remainder, "%.1f",
                             freq_kilohz);
#endif
            } else if (prm->output == OUTPUT_NONCURSES) {
                if (cut_off_frequency < 1000)
                    printf("%-4d", freq_floor);
                else if (cut_off_frequency > 1000 && cut_off_frequency < 10000)
                    printf("%.2f", freq_kilohz);
                else
                    printf("%.1f", freq_kilohz);

                if (n < audio_raw->number_of_bars - 1)
                    printf("\033[%dC", prm->bar_width + prm->bar_spacing - 4);
            }
        }
        printf("\r\033[%dA", audio_raw->lines + 1);
    }

    return 0;
}
#ifndef SDL_GLSL
int audio_raw_fetch(struct audio_raw *audio_raw, struct config_params *prm,
                    struct cava_plan *plan) {
#else
int audio_raw_fetch(struct audio_raw *audio_raw, struct config_params *prm, int *re_paint,
                    struct cava_plan *plan) {
#endif
    (void)plan;
    for (int n = 0; n < audio_raw->number_of_bars; n++) {
        if (!prm->waveform) {
            audio_raw->cava_out[n] *= prm->sens;
        } else {
            if (audio_raw->cava_out[n] > 1.0)
                prm->sens *= 0.999;
            else
                prm->sens *= 1.0001;

            if (prm->orientation != ORIENT_SPLIT_H)
                audio_raw->cava_out[n] = (audio_raw->cava_out[n] + 1.0) / 2.0;
        }

        if (prm->output == OUTPUT_SDL_GLSL) {
            if (audio_raw->cava_out[n] > 1.0)
                audio_raw->cava_out[n] = 1.0;
            else if (audio_raw->cava_out[n] < 0.0)
                audio_raw->cava_out[n] = 0.0;
        } else {
            audio_raw->cava_out[n] *= *audio_raw->dimension_value;
            if (prm->orientation == ORIENT_SPLIT_H || prm->orientation == ORIENT_SPLIT_V) {
                audio_raw->cava_out[n] /= 2;
            }
        }
        if (prm->waveform) {
            audio_raw->bars_raw[n] = audio_raw->cava_out[n];
        }
    }

    if (!prm->waveform) {
        if (audio_raw->channels == 2) {
            for (int n = 0; n < audio_raw->number_of_bars / audio_raw->output_channels; n++) {
                if (prm->userEQ_enabled)
                    audio_raw->cava_out[n] *=
                        prm->userEQ[(int)floor(((double)n) * audio_raw->userEQ_keys_to_bars_ratio)];
                audio_raw->bars_left[n] = audio_raw->cava_out[n];
            }
            for (int n = 0; n < audio_raw->number_of_bars / audio_raw->output_channels; n++) {
                if (prm->userEQ_enabled)
                    audio_raw
                        ->cava_out[n + audio_raw->number_of_bars / audio_raw->output_channels] *=
                        prm->userEQ[(int)floor(((double)n) * audio_raw->userEQ_keys_to_bars_ratio)];
                audio_raw->bars_right[n] =
                    audio_raw->cava_out[n + audio_raw->number_of_bars / audio_raw->output_channels];
            }
        } else {
            for (int n = 0; n < audio_raw->number_of_bars; n++) {
                if (prm->userEQ_enabled)
                    audio_raw->cava_out[n] *=
                        prm->userEQ[(int)floor(((double)n) * audio_raw->userEQ_keys_to_bars_ratio)];
                audio_raw->bars_raw[n] = audio_raw->cava_out[n];
            }
        }

        // process [filter]
        if (prm->monstercat) {
            if (audio_raw->channels == 2) {
                audio_raw->bars_left = monstercat_filter(
                    audio_raw->bars_left, audio_raw->number_of_bars / audio_raw->output_channels,
                    prm->waves, prm->monstercat, *audio_raw->dimension_value);
                audio_raw->bars_right = monstercat_filter(
                    audio_raw->bars_right, audio_raw->number_of_bars / audio_raw->output_channels,
                    prm->waves, prm->monstercat, *audio_raw->dimension_value);
            } else {
                audio_raw->bars_raw =
                    monstercat_filter(audio_raw->bars_raw, audio_raw->number_of_bars, prm->waves,
                                      prm->monstercat, *audio_raw->dimension_value);
            }
        }
        if (audio_raw->channels == 2) {
            if (prm->stereo) {
                // mirroring stereo channels
                for (int n = 0; n < audio_raw->number_of_bars; n++) {
                    if (n < audio_raw->number_of_bars / 2) {
                        if (prm->reverse) {
                            audio_raw->bars_raw[n] = audio_raw->bars_left[n];
                        } else {
                            audio_raw->bars_raw[n] =
                                audio_raw->bars_left[audio_raw->number_of_bars / 2 - n - 1];
                        }
                    } else {
                        if (prm->reverse) {
                            audio_raw->bars_raw[n] =
                                audio_raw->bars_right[audio_raw->number_of_bars - n - 1];
                        } else {
                            audio_raw->bars_raw[n] =
                                audio_raw->bars_right[n - audio_raw->number_of_bars / 2];
                        }
                    }
                }
            } else {
                // stereo mono output
                for (int n = 0; n < audio_raw->number_of_bars; n++) {
                    if (prm->reverse) {
                        if (prm->mono_opt == AVERAGE) {
                            audio_raw->bars_raw[audio_raw->number_of_bars - n - 1] =
                                (audio_raw->bars_left[n] + audio_raw->bars_right[n]) / 2;
                        } else if (prm->mono_opt == LEFT) {
                            audio_raw->bars_raw[audio_raw->number_of_bars - n - 1] =
                                audio_raw->bars_left[n];
                        } else if (prm->mono_opt == RIGHT) {
                            audio_raw->bars_raw[audio_raw->number_of_bars - n - 1] =
                                audio_raw->bars_right[n];
                        }
                    } else {
                        if (prm->mono_opt == AVERAGE) {
                            audio_raw->bars_raw[n] =
                                (audio_raw->bars_left[n] + audio_raw->bars_right[n]) / 2;
                        } else if (prm->mono_opt == LEFT) {
                            audio_raw->bars_raw[n] = audio_raw->bars_left[n];
                        } else if (prm->mono_opt == RIGHT) {
                            audio_raw->bars_raw[n] = audio_raw->bars_right[n];
                        }
                    }
                }
            }
        }
    }
#ifdef SDL_GLSL
    *re_paint = 0;
#endif

    for (int n = 0; n < audio_raw->number_of_bars; n++) {
        audio_raw->bars[n] = audio_raw->bars_raw[n];
        // show idle bar heads
        if (prm->output != OUTPUT_RAW && prm->output != OUTPUT_NORITAKE && audio_raw->bars[n] < 1 &&
            prm->waveform == 0 && prm->show_idle_bar_heads == 1)
            audio_raw->bars[n] = 1;
#ifdef SDL_GLSL

        if (prm->output == OUTPUT_SDL_GLSL)
            audio_raw->bars[n] =
                audio_raw->bars_raw[n] * 1000; // values are 0-1, only used to check for changes

        if (audio_raw->bars[n] != audio_raw->previous_frame[n])
            *re_paint = 1;
#endif
    }

    return 0;
}

int audio_raw_clean(struct audio_raw *audio_raw) {
    if (audio_raw->channels == 2) {
        free(audio_raw->bars_left);
        free(audio_raw->bars_right);
    }
    free(audio_raw->cava_out);
    free(audio_raw->bars);
    free(audio_raw->bars_raw);
    free(audio_raw->previous_bars_raw);
    free(audio_raw->previous_frame);

    return 0;
}

int audio_raw_destroy(struct audio_raw *audio_raw) {
    audio_raw_clean(audio_raw);

    free(audio_raw->dimension_bar);
    free(audio_raw->dimension_value);
    free(audio_raw);

    return 0;
}

// general: cleanup
void cleanup(int output_mode) {
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
