#include "cava/common.h"
#include "cava/debug.h"
#include "cava/util.h"
#include <math.h>
#include <sys/stat.h>

float *monstercat_filter(float *bars, int number_of_bars, int waves, double monstercat) {
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
                   struct cava_plan *plan) {
    audio_raw->channels = audio->channels;

    if (prm->upper_cut_off > audio->rate / 2) {
        cleanup(prm->output);
        fprintf(stderr, "higher cuttoff frequency can't be higher than sample rate / 2");
        exit(EXIT_FAILURE);
    }

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
        audio_raw->height = audio_raw->lines;
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

        init_terminal_noncurses(prm->inAtty, prm->color, prm->bcolor, prm->col, prm->bgcol,
                                prm->gradient, prm->gradient_count, prm->gradient_colors,
                                audio_raw->width, audio_raw->lines, prm->bar_width);
        audio_raw->height = audio_raw->lines * 8;
        break;
#ifndef _MSC_VER
    case OUTPUT_RAW:
    case OUTPUT_NORITAKE:
        if (strcmp(prm->raw_target, "/dev/stdout") != 0) {
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
        } else {
            prm->fp = fileno(stdout);
        }

        if (prm->fp == -1) {
            fprintf(stderr, "could not open file %s for writing\n", prm->raw_target);
            exit(1);
        }

#ifndef NDEBUG
        debug("open file %s for writing raw output\n", prm->raw_target);
#endif
        // width must be hardcoded for raw output.
        audio_raw->width = 512;

        prm->bar_width = 1; // not used
        prm->bar_spacing = 1;

        if (strcmp(prm->data_format, "ascii") != 0) {
            // "binary" or "noritake"
            audio_raw->height = pow(2, prm->bit_format) - 1;
        } else {
            audio_raw->height = prm->ascii_range;
        }
        break;
#endif
    default:
        exit(EXIT_FAILURE); // Can't happen.
    }

    // handle for user setting too many bars
    if (prm->fixedbars) {
        prm->autobars = 0;
        if (prm->fixedbars * prm->bar_width + prm->fixedbars * prm->bar_spacing - prm->bar_spacing >
            audio_raw->width)
            prm->autobars = 1;
    }

    // getting numbers of bars
    audio_raw->number_of_bars = prm->fixedbars;

    if (prm->autobars == 1)
        audio_raw->number_of_bars =
            (*audio_raw->dimension_bar + prm->bar_spacing) / (prm->bar_width + prm->bar_spacing);

    if (audio_raw->number_of_bars <= 1) {
        audio_raw->number_of_bars = 1; // must have at least 1 bars
        if (prm->stereo) {
            audio_raw->number_of_bars = 2; // stereo have at least 2 bars
        }
    }
    if (audio_raw->number_of_bars > 512)
        audio_raw->number_of_bars = 512; // cant have more than 512 bars on 44100 rate

    audio_raw->output_channels = 1;
    if (prm->stereo) { // stereo must have even numbers of bars
        if (audio->channels == 1) {
            fprintf(stderr, "stereo output configured, but only one channel in audio input.\n");
            exit(1);
        }
        audio_raw->output_channels = 2;
        if (audio_raw->number_of_bars % 2 != 0)
            audio_raw->number_of_bars--;
    }
    // checks if there is stil extra room, will use this to center
    audio_raw->remainder = (*audio_raw->dimension_bar - audio_raw->number_of_bars * prm->bar_width -
                            audio_raw->number_of_bars * prm->bar_spacing + prm->bar_spacing) /
                           2;
    if (audio_raw->remainder < 0)
        audio_raw->remainder = 0;

#ifndef NDEBUG
    debug("height: %d width: %d dimension_bar: %d dimension_value: %d bars:%d bar width: "
          "%d remainder: %d\n",
          audio_raw->height, audio_raw->width, *audio_raw->dimension_bar,
          *audio_raw->dimension_value, audio_raw->number_of_bars, prm->bar_width,
          audio_raw->remainder);
#endif

    if (prm->userEQ_enabled && (audio_raw->number_of_bars / audio_raw->output_channels > 0)) {
        audio_raw->userEQ_keys_to_bars_ratio =
            (double)(((double)prm->userEQ_keys) /
                     ((double)(audio_raw->number_of_bars / audio_raw->output_channels)));
    }

    *plan = *cava_init(audio_raw->number_of_bars / audio_raw->output_channels, audio->rate,
                       audio->channels, prm->autosens, prm->noise_reduction, prm->lower_cut_off,
                       prm->upper_cut_off);

    if (plan->status == -1) {
        cleanup(prm->output);
        fprintf(stderr, "Error initalizing cava . %s", plan->error_message);
        exit(EXIT_FAILURE);
    }

    audio_raw->bars_left =
        (float *)malloc(audio_raw->number_of_bars / audio_raw->output_channels * sizeof(float));
    audio_raw->bars_right =
        (float *)malloc(audio_raw->number_of_bars / audio_raw->output_channels * sizeof(float));
    memset(audio_raw->bars_left, 0,
           sizeof(float) * audio_raw->number_of_bars / audio_raw->output_channels);
    memset(audio_raw->bars_right, 0,
           sizeof(float) * audio_raw->number_of_bars / audio_raw->output_channels);

    audio_raw->bars = (int *)malloc(audio_raw->number_of_bars * sizeof(int));
    audio_raw->bars_raw = (float *)malloc(audio_raw->number_of_bars * sizeof(float));
    audio_raw->previous_frame = (int *)malloc(audio_raw->number_of_bars * sizeof(int));
    audio_raw->cava_out = (double *)malloc(audio_raw->number_of_bars * audio->channels /
                                           audio_raw->output_channels * sizeof(double));

    memset(audio_raw->bars, 0, sizeof(int) * audio_raw->number_of_bars);
    memset(audio_raw->bars_raw, 0, sizeof(float) * audio_raw->number_of_bars);
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
                        plan->cut_off_frequency[audio_raw->number_of_bars / 2 - 1 - n];
                else
                    cut_off_frequency = plan->cut_off_frequency[n - audio_raw->number_of_bars / 2];
            } else {
                cut_off_frequency = plan->cut_off_frequency[n];
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

int audio_raw_fetch(struct audio_raw *audio_raw, struct config_params *prm, int *re_paint,
                    struct cava_plan *plan) {
    (void)plan;
    for (int n = 0; n < audio_raw->number_of_bars; n++) {
        if (!prm->waveform) {
            audio_raw->cava_out[n] *= prm->sens;
        } else {
            if (audio_raw->cava_out[n] > 1.0)
                prm->sens *= 0.999;
            else
                prm->sens *= 1.0001;
            audio_raw->cava_out[n] = (audio_raw->cava_out[n] + 1.0) / 2.0;
        }

        if (prm->output == OUTPUT_SDL_GLSL) {
            if (audio_raw->cava_out[n] > 1.0)
                audio_raw->cava_out[n] = 1.0;
            else if (audio_raw->cava_out[n] < 0.0)
                audio_raw->cava_out[n] = 0.0;
        } else {
            audio_raw->cava_out[n] *= *audio_raw->dimension_value;
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
                    prm->waves, prm->monstercat);
                audio_raw->bars_right = monstercat_filter(
                    audio_raw->bars_right, audio_raw->number_of_bars / audio_raw->output_channels,
                    prm->waves, prm->monstercat);
            } else {
                audio_raw->bars_raw = monstercat_filter(
                    audio_raw->bars_raw, audio_raw->number_of_bars, prm->waves, prm->monstercat);
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

#ifndef NDEBUG
    int maxvalue = 0;
    int minvalue = 0;
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

#ifndef NDEBUG
        mvprintw(n, 0, "%d: f:%f->%f (%d->%d), eq:%15e, peak:%d \n", n, plan->cut_off_frequency[n],
                 plan->cut_off_frequency[n + 1], plan->FFTbuffer_lower_cut_off[n],
                 plan->FFTbuffer_upper_cut_off[n], plan->eq[n], audio_raw->bars[n]);

        if (audio_raw->bars[n] < minvalue) {
            minvalue = audio_raw->bars[n];
            debug("min value: %d\n", minvalue); // checking maxvalue 10000
        }
        if (audio_raw->bars[n] > maxvalue) {
            maxvalue = audio_raw->bars[n];
        }

        if (audio_raw->bars[n] < 0) {
            debug("negative bar value!! %d\n", audio_raw->bars[n]);
            //    exit(EXIT_FAILURE); // Can't happen.
        }

#endif
    }

#ifndef NDEBUG
    mvprintw(audio_raw->number_of_bars + 1, 0, "sensitivity %.10e", prm->sens);
    mvprintw(audio_raw->number_of_bars + 2, 0, "min value: %d\n",
             minvalue); // checking maxvalue 10000
    mvprintw(audio_raw->number_of_bars + 3, 0, "max value: %d\n",
             maxvalue); // checking maxvalue 10000
#endif

    return 0;
}

int audio_raw_clean(struct audio_raw *audio_raw) {
    free(audio_raw->bars);
    free(audio_raw->previous_frame);
    if (audio_raw->channels == 2) {
        free(audio_raw->bars_left);
        free(audio_raw->bars_right);
    }
    free(audio_raw->bars_raw);
    free(audio_raw->cava_out);

    return 0;
}

int audio_raw_destroy(struct audio_raw *audio_raw) {
    audio_raw_clean(audio_raw);

    free(audio_raw->dimension_bar);
    free(audio_raw->dimension_value);
    free(audio_raw);

    return 0;
}
