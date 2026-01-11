#include "terminal_noncurses.h"

#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif
#include <wchar.h>

#define MAX_GRADIENT_COLOR_DEFS 8
#define WCHAR_SIZE 4 // wchar is 4 bytes on linux but only 2 on windows, we need at least 3

wchar_t *frame_buffer;
wchar_t *barstring[8];
wchar_t *top_barstring[8];
const wchar_t *left_barchars[] = {L"\u2588", L"\u258F", L"\u258E", L"\u258D",
                                  L"\u258C", L"\u258B", L"\u258A", L"\u2589"};
const wchar_t *right_barchars[] = {L"\u2588", L"\u2595",     L"\U0001FB87", L"\U0001FB88",
                                   L"\u2590", L"\U0001FB89", L"\U0001FB8A", L"\U0001FB8B"};

wchar_t *spacestring;
int buf_length;
char *ttyframe_buffer;
char *ttybarstring[8];
char *ttyspacestring;
int ttybuf_length;

int setecho(int fd, int onoff) {

#ifdef _WIN32
    HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hStdin, &mode);
    if (onoff == 0)
        SetConsoleMode(hStdin, mode & (~ENABLE_ECHO_INPUT));
    else
        SetConsoleMode(hStdin, mode & (ENABLE_ECHO_INPUT));

#else
    struct termios t;

    if (tcgetattr(fd, &t) == -1)
        return -1;

    if (onoff == 0) {
        t.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL | ICANON);
        t.c_cc[VTIME] = 0;
        t.c_cc[VMIN] = 0;
    } else {
        t.c_lflag |= (ECHO | ECHOE | ECHOK | ECHONL | ICANON);
        t.c_cc[VTIME] = 0;
        t.c_cc[VMIN] = 1;
    }

    if (tcsetattr(fd, TCSANOW, &t) == -1)
        return -1;
#endif
    return 0;
}

struct colors {
    short unsigned int rgb[3];
};

struct colors *gradient_colors;

struct colors parse_color(char *color_string) {
    struct colors color;
    sscanf(++color_string, "%02hx%02hx%02hx", &color.rgb[0], &color.rgb[1], &color.rgb[2]);
    return color;
}

int change_color(int color_idx, int buf_idx, int tty) {
    if (tty) {
        return snprintf(ttyframe_buffer + buf_idx, ttybuf_length - buf_idx, "\033[38;2;%d;%d;%dm",
                        gradient_colors[color_idx].rgb[0], gradient_colors[color_idx].rgb[1],
                        gradient_colors[color_idx].rgb[2]);
    } else {
        return swprintf(frame_buffer + buf_idx, buf_length - buf_idx, L"\033[38;2;%d;%d;%dm",
                        gradient_colors[color_idx].rgb[0], gradient_colors[color_idx].rgb[1],
                        gradient_colors[color_idx].rgb[2]);
    }
}

// general: cleanup
void free_terminal_noncurses(void) {
    free(frame_buffer);
    free(ttyframe_buffer);
    free(spacestring);
    free(ttyspacestring);
    for (int i = 0; i < 8; i++) {
        free(barstring[i]);
        free(top_barstring[i]);
        free(ttybarstring[i]);
    }
}

int init_terminal_noncurses(int tty, char *const fg_color_string, char *const bg_color_string,
                            int col, int bgcol, int vertical_gradient, int gradient_count,
                            char **gradient_color_strings, int horizontal_gradient,
                            int horizontal_gradient_count, char **horizontal_gradient_color_strings,
                            int number_of_bars, int width, int lines, int bar_width,
                            enum orientation orientation, enum orientation blendDirection) {

    free_terminal_noncurses();

    if (vertical_gradient || horizontal_gradient)
        free(gradient_colors);

    if (tty) {

        ttybuf_length = sizeof(char) * width * lines * 10;
        ttyframe_buffer = (char *)malloc(ttybuf_length);
        ttyspacestring = (char *)malloc(sizeof(char) * (bar_width + 1));

        // clearing barstrings
        for (int n = 0; n < 8; n++) {
            ttybarstring[n] = (char *)malloc(sizeof(char) * (bar_width + 1));
            ttybarstring[n][0] = '\0';
        }
        ttyspacestring[0] = '\0';
        ttyframe_buffer[0] = '\0';

        // creating barstrings for drawing
        for (int n = 0; n < bar_width; n++) {
            strcat(ttybarstring[0], "H");
            strcat(ttybarstring[1], "A");
            strcat(ttybarstring[2], "B");
            strcat(ttybarstring[3], "C");
            strcat(ttybarstring[4], "D");
            strcat(ttybarstring[5], "E");
            strcat(ttybarstring[6], "F");
            strcat(ttybarstring[7], "G");
            strcat(ttyspacestring, " ");
        }
    } else if (!tty) {

        buf_length = WCHAR_SIZE * width * lines * 10;
        frame_buffer = (wchar_t *)malloc(buf_length);
        spacestring = (wchar_t *)malloc(WCHAR_SIZE * (bar_width + 1));

        // clearing barstrings
        for (int n = 0; n < 8; n++) {
            barstring[n] = (wchar_t *)malloc(WCHAR_SIZE * (bar_width + 1));
            barstring[n][0] = '\0';

            top_barstring[n] = (wchar_t *)malloc(WCHAR_SIZE * (bar_width + 1));
            top_barstring[n][0] = '\0';
        }
        spacestring[0] = '\0';
        frame_buffer[0] = '\0';

        // creating barstrings for drawing
        for (int n = 0; n < bar_width; n++) {
            wcscat(barstring[0], L"\u2588");
            wcscat(barstring[1], L"\u2581");
            wcscat(barstring[2], L"\u2582");
            wcscat(barstring[3], L"\u2583");
            wcscat(barstring[4], L"\u2584");
            wcscat(barstring[5], L"\u2585");
            wcscat(barstring[6], L"\u2586");
            wcscat(barstring[7], L"\u2587");

            wcscat(top_barstring[0], L"\u2588");
            wcscat(top_barstring[1], L"\u2594");
            wcscat(top_barstring[2], L"\U0001FB82");
            wcscat(top_barstring[3], L"\U0001FB83");
            wcscat(top_barstring[4], L"\u2580");
            wcscat(top_barstring[5], L"\U0001FB84");
            wcscat(top_barstring[6], L"\U0001FB85");
            wcscat(top_barstring[7], L"\U0001FB86");

            wcscat(spacestring, L" ");
        }
    }

    col += 30;
#ifdef _WIN32
    HANDLE hStdOut = NULL;
    CONSOLE_CURSOR_INFO curInfo;

    hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleCursorInfo(hStdOut, &curInfo);
    curInfo.bVisible = FALSE;
    SetConsoleCursorInfo(hStdOut, &curInfo);

    setlocale(LC_ALL, "en_US.utf8");
    SetConsoleOutputCP(CP_UTF8);
    system("cls");

#else
    printf("\033[2J");   // clear screen
    printf("\033[?25l"); // hide cursor
#endif

    // output: reset console
    printf("\033[0m\n");

    if (col == 38) {
        struct colors fg_color = parse_color(fg_color_string);
        printf("\033[38;2;%d;%d;%dm", fg_color.rgb[0], fg_color.rgb[1], fg_color.rgb[2]);
    } else if (col < 38 && col >= 30) {
        printf("\033[%dm", col); // setting color
    }

    if (bgcol != 0) {

        bgcol += 40;

        if (bgcol == 48) {
            struct colors bg_color = parse_color(bg_color_string);
            printf("\033[48;2;%d;%d;%dm", bg_color.rgb[0], bg_color.rgb[1], bg_color.rgb[2]);
        } else {
            printf("\033[%dm", bgcol);
        }

        for (int n = lines; n >= 0; n--) {
            for (int i = 0; i < width; i++) {
                printf(" "); // setting background color
            }
            if (n != 0)
                printf("\n");
            else
                printf("\r");
        }
        printf("\033[%dA", lines); // moving cursor back up
    }

    int gradient_size = lines;
    if (orientation == ORIENT_RIGHT || orientation == ORIENT_LEFT ||
        orientation == ORIENT_SPLIT_V) {
        gradient_size = width;
    }
    struct colors *horizontal_gradient_colors;
    struct colors *vertical_gradient_colors;

    if (vertical_gradient) {
        if (orientation == ORIENT_SPLIT_H || orientation == ORIENT_SPLIT_V) {
            gradient_size = gradient_size / 2 + 1;
        }
        struct colors gradient_color_defs[MAX_GRADIENT_COLOR_DEFS];
        for (int i = 0; i < gradient_count; i++) {
            gradient_color_defs[i] = parse_color(gradient_color_strings[i]);
        }

        vertical_gradient_colors =
            (struct colors *)malloc((gradient_size * 2) * sizeof(struct colors));

        int individual_size = gradient_size / (gradient_count - 1);

        float rest = gradient_size / (float)(gradient_count - 1) - individual_size;
        float rest_total = 0;
        int gradient_lines = 0;

        for (int i = 0; i < gradient_count - 1; i++) {
            individual_size = gradient_size / (gradient_count - 1);
            if (rest_total > 1.0) {
                individual_size++;
                rest_total = rest_total - 1.0;
            }
            for (int n = 0; n < individual_size; n++) {
                for (int c = 0; c < 3; c++) {
                    float next_color =
                        gradient_color_defs[i + 1].rgb[c] - gradient_color_defs[i].rgb[c];
                    next_color *= n / (float)individual_size;
                    vertical_gradient_colors[gradient_lines].rgb[c] =
                        gradient_color_defs[i].rgb[c] + next_color;
                }
                gradient_lines++;
            }
            rest_total = rest_total + rest;
        }
        vertical_gradient_colors[gradient_size - 1] = gradient_color_defs[gradient_count - 1];

        if (!horizontal_gradient)
            gradient_colors = vertical_gradient_colors;
    }

    if (horizontal_gradient) {

        struct colors horizontal_gradient_color_defs[MAX_GRADIENT_COLOR_DEFS];
        for (int i = 0; i < horizontal_gradient_count; i++) {
            horizontal_gradient_color_defs[i] = parse_color(horizontal_gradient_color_strings[i]);
        }

        horizontal_gradient_colors =
            (struct colors *)malloc((number_of_bars) * sizeof(struct colors));

        int individual_size = number_of_bars / (horizontal_gradient_count - 1);

        float rest = number_of_bars / (float)(horizontal_gradient_count - 1) - individual_size;

        float rest_total = 0;

        int gradient_bars = 0;

        for (int i = 0; i < horizontal_gradient_count - 1; i++) {
            individual_size = number_of_bars / (horizontal_gradient_count - 1);
            if (rest_total > 1.0) {
                individual_size++;
                rest_total = rest_total - 1.0;
            }
            for (int n = 0; n < individual_size; n++) {
                for (int c = 0; c < 3; c++) {
                    float next_color = horizontal_gradient_color_defs[i + 1].rgb[c] -
                                       horizontal_gradient_color_defs[i].rgb[c];
                    next_color *= n / (float)individual_size;
                    horizontal_gradient_colors[gradient_bars].rgb[c] =
                        horizontal_gradient_color_defs[i].rgb[c] + next_color;
                }
                gradient_bars++;
            }
            rest_total = rest_total + rest;
        }
        horizontal_gradient_colors[number_of_bars - 1] =
            horizontal_gradient_color_defs[horizontal_gradient_count - 1];

        if (!vertical_gradient)
            gradient_colors = horizontal_gradient_colors;
    }

    if (horizontal_gradient && vertical_gradient) {
        gradient_colors =
            (struct colors *)malloc((number_of_bars * gradient_size) * sizeof(struct colors));
        for (int i = 0; i < gradient_size; i++) {
            float current_height = i / (float)gradient_size;
            for (int n = 0; n < number_of_bars; n++) {
                float current_width = n / (float)number_of_bars;
                for (int c = 0; c < 3; c++) {
                    if (blendDirection == ORIENT_BOTTOM) {
                        gradient_colors[i * number_of_bars + n].rgb[c] =
                            vertical_gradient_colors[i].rgb[c] * current_height +
                            horizontal_gradient_colors[n].rgb[c] * (1 - current_height);
                    } else if (blendDirection == ORIENT_TOP) {
                        gradient_colors[i * number_of_bars + n].rgb[c] =
                            vertical_gradient_colors[i].rgb[c] * (1 - current_height) +
                            horizontal_gradient_colors[n].rgb[c] * current_height;
                    } else if (blendDirection == ORIENT_LEFT) {
                        gradient_colors[i * number_of_bars + n].rgb[c] =
                            vertical_gradient_colors[i].rgb[c] * current_width +
                            horizontal_gradient_colors[n].rgb[c] * (1 - current_width);
                    } else if (blendDirection == ORIENT_RIGHT) {
                        gradient_colors[i * number_of_bars + n].rgb[c] =
                            vertical_gradient_colors[i].rgb[c] * (1 - current_width) +
                            horizontal_gradient_colors[n].rgb[c] * current_width;
                    }
                }
            }
        }
        free(horizontal_gradient_colors);
        free(vertical_gradient_colors);
    }

#ifdef _WIN32
    setecho(1, 0);
#else
    setecho(STDIN_FILENO, 0);
#endif
    return 0;
}

void get_terminal_dim_noncurses(int *width, int *lines) {

#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;

    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
    *width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    *lines = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
    struct winsize dim;

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &dim);

    *lines = (int)dim.ws_row;
    *width = (int)dim.ws_col;
#endif
}

int draw_terminal_noncurses(int tty, int lines, int width, int number_of_bars, int bar_width,
                            int bar_spacing, int rest, int bars[], int previous_frame[],
                            int vertical_gradient, int horizontal_gradient, int x_axis_info,
                            enum orientation orientation, int offset) {

    int current_cell, prev_cell, same_line, new_line, cx;

    same_line = 0;
    new_line = 0;
    cx = 0;

    if (!tty) {
        // output: check if terminal has been resized
        if (x_axis_info)
            lines++;

        int new_lines;
        int new_width;
        get_terminal_dim_noncurses(&new_width, &new_lines);

        if (new_lines != lines || new_width != width)
            return -1;

        if (x_axis_info)
            lines--;
    }

    if (tty)
        ttyframe_buffer[0] = '\0';
    else if (!tty)
        frame_buffer[0] = '\0';

    if (orientation == ORIENT_BOTTOM || orientation == ORIENT_TOP) {
        if (offset)
            lines /= 2;

        for (int current_line = lines - 1; current_line >= 0; current_line--) {

            if (orientation == ORIENT_BOTTOM) {
                if (vertical_gradient && !horizontal_gradient) {
                    cx += change_color(current_line, cx, tty);
                }
            } else if (orientation == ORIENT_TOP) {
                if (vertical_gradient && !horizontal_gradient) {
                    cx += change_color(lines - current_line - 1, cx, tty);
                }
            }

            int same_bar = 0;
            new_line = 1;

            for (int i = 0; i < number_of_bars; i++) {

                if (orientation == ORIENT_BOTTOM) {
                    current_cell = bars[i] - current_line * 8;
                    prev_cell = previous_frame[i] - current_line * 8;
                } else if (orientation == ORIENT_TOP) {
                    current_cell = bars[i] - (lines - current_line - 1) * 8;
                    prev_cell = previous_frame[i] - (lines - current_line - 1) * 8;
                }

                // same as last frame
                if ((current_cell < 1 && prev_cell < 1) || (current_cell > 7 && prev_cell > 7) ||
                    (current_cell == prev_cell)) {
                    same_bar++;
                } else {
                    if (tty) {
                        // move cursor to beginning of this line
                        if (new_line) {
                            if (orientation == ORIENT_TOP && offset) {
                                if (rest || same_bar)
                                    cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx,
                                                   "\033[%d;%dH", lines * 2 - current_line,
                                                   1 + rest + (bar_width + bar_spacing) * same_bar);
                                else
                                    cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx,
                                                   "\033[%dH", lines * 2 - current_line);
                            } else {

                                if (rest || same_bar)
                                    cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx,
                                                   "\033[%d;%dH", lines - current_line,
                                                   1 + rest + (bar_width + bar_spacing) * same_bar);
                                else
                                    cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx,
                                                   "\033[%dH", lines - current_line);
                            }
                            new_line = 0;
                            same_bar = 0;
                        }

                        if (same_bar > 0) {
                            cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx, "\033[%dC",
                                           (bar_width + bar_spacing) * same_bar); // move forward
                            same_bar = 0;
                        }

                        // horizontal vertical_gradient
                        if (current_cell > 0) {
                            if (!vertical_gradient && horizontal_gradient) {
                                cx += change_color(i, cx, tty);
                            }
                            if (vertical_gradient && horizontal_gradient) {
                                if (orientation == ORIENT_BOTTOM) {
                                    cx += change_color(current_line * number_of_bars + i, cx, tty);
                                } else if (orientation == ORIENT_TOP) {
                                    cx += change_color(
                                        (lines - current_line - 1) * number_of_bars + i, cx, tty);
                                }
                            }
                        }

                        if (current_cell < 1)
                            cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx, "%s",
                                           ttyspacestring);
                        else if (current_cell > 7)
                            cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx, "%s",
                                           ttybarstring[0]);
                        else
                            cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx, "%s",
                                           ttybarstring[current_cell]);

                        if (bar_spacing && i < number_of_bars - 1) {
                            if (bar_spacing == 1)
                                cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx, " ");
                            else
                                cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx, "\033[%dC",
                                               bar_spacing);
                        }
                    } else if (!tty) {

                        // move cursor to beginning of this line
                        if (new_line) {
                            if (orientation == ORIENT_TOP && offset) {
                                if (rest || same_bar)
                                    cx += swprintf(frame_buffer + cx, buf_length - cx,
                                                   L"\033[%d;%dH", lines * 2 - current_line,
                                                   1 + rest + (bar_width + bar_spacing) * same_bar);
                                else
                                    cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[%dH",
                                                   lines * 2 - current_line);
                            } else {
                                if (rest || same_bar)
                                    cx += swprintf(frame_buffer + cx, buf_length - cx,
                                                   L"\033[%d;%dH", lines - current_line,
                                                   1 + rest + (bar_width + bar_spacing) * same_bar);
                                else
                                    cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[%dH",
                                                   lines - current_line);
                            }
                            new_line = 0;
                            same_bar = 0;
                        }

                        if (same_bar > 0) {
                            cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[%dC",
                                           (bar_width + bar_spacing) * same_bar); // move forward
                            same_bar = 0;
                        }

                        // horizontal vertical_gradient
                        if (current_cell > 0) {
                            if (!vertical_gradient && horizontal_gradient) {
                                cx += change_color(i, cx, tty);
                            }
                            if (vertical_gradient && horizontal_gradient) {
                                if (orientation == ORIENT_BOTTOM) {
                                    cx += change_color(current_line * number_of_bars + i, cx, tty);
                                } else if (orientation == ORIENT_TOP) {
                                    cx += change_color(
                                        (lines - current_line - 1) * number_of_bars + i, cx, tty);
                                }
                            }
                        }

                        if (current_cell < 1) {
                            cx += swprintf(frame_buffer + cx, buf_length - cx,
                                           spacestring); // clear
                        } else if (current_cell > 7) {
                            if (orientation == ORIENT_BOTTOM)
                                cx += swprintf(frame_buffer + cx, buf_length - cx,
                                               barstring[0]); // draw full
                            else if (orientation == ORIENT_TOP)
                                cx += swprintf(frame_buffer + cx, buf_length - cx,
                                               top_barstring[0]); // draw fragment (top)
                        } else {
                            if (orientation == ORIENT_BOTTOM)
                                cx += swprintf(frame_buffer + cx, buf_length - cx,
                                               barstring[current_cell]); // draw fragment
                            else if (orientation == ORIENT_TOP)
                                cx += swprintf(frame_buffer + cx, buf_length - cx,
                                               top_barstring[current_cell]); // draw fragment (top)
                        }

                        if (bar_spacing && i < number_of_bars - 1) {
                            if (bar_spacing == 1)
                                cx += swprintf(frame_buffer + cx, buf_length - cx, L" ");
                            else
                                cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[%dC",
                                               bar_spacing);
                        }
                    }
                }
            }

            if (same_bar == number_of_bars) {
                same_line++;
            }
        }
    }

    if (orientation == ORIENT_LEFT || orientation == ORIENT_RIGHT) {
        // in "vertical" orientations frequency is vertical, amplitude is horizontal
        // drawing the bars from top to bottom
        // "vertical_gradient" is horizontal and "horizontal vertical_gradient" is vertical
        // tty not implemented, lacks special unicode charachters

        if (offset)
            width /= 2;

        // asuminng no changes
        same_line = lines;
        for (int i = 0; i < number_of_bars; i++) {
            if (bars[i] != previous_frame[i]) {
                same_line = 0; // change detected!
                int diff = abs((bars[i] / 8) - (previous_frame[i] / 8));
                int fragment = bars[i] % 8;
                int previous_fragment = previous_frame[i] % 8;

                for (int n = 0; n < bar_width; n++) {
                    // finding y pos of bar
                    int y_pos = rest + (bar_width + bar_spacing) * i + n + 1;
                    if (!vertical_gradient && horizontal_gradient) {
                        cx += change_color(i, cx, tty);
                    }
                    if (orientation == ORIENT_LEFT) {
                        // drawing from left to right
                        if (bars[i] > previous_frame[i]) {
                            // this bar is longer than previous

                            // drawing from tip of previous bar
                            int x_pos = previous_frame[i] / 8 + 1 + (offset * width);
                            int color_x_pos = x_pos - (offset * width) - 1;

                            // move cursor
                            cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[%d;%dH",
                                           y_pos, x_pos);

                            // drawing diff, full 8/8 blocks
                            for (int l = 0; l < diff; l++) {
                                if (vertical_gradient && !horizontal_gradient) {
                                    cx += change_color(color_x_pos + l, cx, tty);
                                }
                                if (vertical_gradient && horizontal_gradient) {
                                    cx += change_color(i + number_of_bars * (color_x_pos + l), cx,
                                                       tty);
                                }
                                cx +=
                                    swprintf(frame_buffer + cx, buf_length - cx, left_barchars[0]);
                            }

                            // drawing 1-7/8 fragment if any
                            if (fragment != 0) {
                                if (vertical_gradient && !horizontal_gradient) {
                                    cx += change_color(color_x_pos + diff, cx, tty);
                                }
                                if (vertical_gradient && horizontal_gradient) {
                                    cx += change_color(i + number_of_bars * (color_x_pos + diff),
                                                       cx, tty);
                                }
                                cx += swprintf(frame_buffer + cx, buf_length - cx,
                                               left_barchars[fragment]);
                            }
                        } else {
                            // this bar is shorter than previous

                            // clearing from tip of this bar
                            int x_pos = bars[i] / 8 + 1 + (offset * width);
                            int color_x_pos = x_pos - (offset * width) - 1;

                            // move crusor
                            cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[%d;%dH",
                                           y_pos, x_pos);

                            // diff is full blocks only
                            int chars_to_clear = diff;

                            // we first draw fragment on edge of bar, if any
                            if (fragment != 0) {
                                if (vertical_gradient && !horizontal_gradient) {
                                    cx += change_color(color_x_pos, cx, tty);
                                }
                                if (vertical_gradient && horizontal_gradient) {
                                    cx += change_color(i + number_of_bars * (color_x_pos), cx, tty);
                                }
                                cx += swprintf(frame_buffer + cx, buf_length - cx,
                                               left_barchars[fragment]);
                                // we drew fragmnet, one leas bar to clear
                                chars_to_clear--;
                            }

                            // there was a fragment in the previous bar, one more bar to
                            // clear
                            if (previous_fragment != 0)
                                chars_to_clear++;

                            // clear previous bar from tip of this bar
                            for (int l = 0; l < chars_to_clear; l++) {
                                cx += swprintf(frame_buffer + cx, buf_length - cx, L" ");
                            }
                        }
                    } else if (orientation == ORIENT_RIGHT) {
                        // drawing from right to left
                        if (bars[i] > previous_frame[i]) {
                            // this bar is longer than previous

                            // finding edge position (tip of last full block)
                            int x_pos = width - bars[i] / 8;

                            // there be no fragments! move one right
                            if (fragment == 0)
                                x_pos++;

                            // move cursor
                            cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[%d;%dH",
                                           y_pos, x_pos);

                            // draw frgment first if any
                            if (fragment != 0) {
                                if (vertical_gradient && !horizontal_gradient) {
                                    cx += change_color(width - x_pos, cx, tty);
                                }
                                if (vertical_gradient && horizontal_gradient) {
                                    cx +=
                                        change_color(i + number_of_bars * (width - x_pos), cx, tty);
                                }
                                x_pos++;
                                cx += swprintf(frame_buffer + cx, buf_length - cx,
                                               right_barchars[fragment]);
                            }

                            // fill up with full blocks to edge of previous bar
                            for (int l = 0; l < diff; l++) {
                                if (vertical_gradient && !horizontal_gradient) {
                                    cx += change_color(width - (x_pos + l), cx, tty);
                                }
                                if (vertical_gradient && horizontal_gradient) {
                                    cx += change_color(i + number_of_bars * (width - (x_pos + l)),
                                                       cx, tty);
                                }
                                cx +=
                                    swprintf(frame_buffer + cx, buf_length - cx, right_barchars[0]);
                            }
                        } else {
                            // this bar is shorter than previous

                            // finding edge position of previous bar (left of last full
                            // block)
                            int x_pos = width - previous_frame[i] / 8;

                            // diff in full blocks
                            int chars_to_clear = diff;

                            // there be no fragments! we can move one right instead of
                            // cleaning
                            if (previous_fragment == 0) {
                                x_pos++;
                                // we will draw a fragment later, one less bar to clear
                                // (drawing a fragment clears a bar)
                                if (fragment != 0) {
                                    chars_to_clear--;
                                }
                            } else if (fragment == 0) {
                                // no fragment on this bar, but fragment in previous bar,
                                // one more char to clear
                                chars_to_clear++;
                            }

                            // move cursor
                            cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[%d;%dH",
                                           y_pos, x_pos);

                            // clear bars
                            for (int l = 0; l < chars_to_clear; l++) {
                                cx += swprintf(frame_buffer + cx, buf_length - cx, L" ");
                            }

                            // draw fragment if any
                            if (fragment != 0) {
                                if (vertical_gradient && !horizontal_gradient) {
                                    cx += change_color(width - (x_pos + chars_to_clear), cx, tty);
                                }
                                if (vertical_gradient && horizontal_gradient) {
                                    cx += change_color(i + number_of_bars *
                                                               (width - (x_pos + chars_to_clear)),
                                                       cx, tty);
                                }
                                cx += swprintf(frame_buffer + cx, buf_length - cx,
                                               right_barchars[fragment]);
                            }
                        }
                    }
                }
            }
        }
    }

    if (same_line != lines) {
        if (tty)
            printf("%s", ttyframe_buffer);
        else if (!tty)
            printf("%ls", frame_buffer);

        fflush(stdout);
    }
    return 0;
}

void cleanup_terminal_noncurses(void) {
#ifdef _WIN32
    setecho(1, 1);
    HANDLE hStdOut = NULL;
    CONSOLE_CURSOR_INFO curInfo;

    hStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleCursorInfo(hStdOut, &curInfo);
    curInfo.bVisible = TRUE;
    SetConsoleCursorInfo(hStdOut, &curInfo);
    printf("\033[0m\n");
    system("cls");
#else
    setecho(STDIN_FILENO, 1);
#ifdef __FreeBSD__
    system("vidcontrol -f >/dev/null 2>&1");
#else
    system("setfont  >/dev/null 2>&1");
    system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz  >/dev/null 2>&1");
#endif
    printf("\033[0m\n"); // reset colors
    printf("\033[?25h"); // show cursor
    printf("\033c");     // reset terminal
    printf("\033[2J");   // clear screen
    fflush(stdout);

#endif
}
