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
struct colors *horizontal_gradient_colors;
struct colors *twodim_gradient_colors;

struct colors parse_color(char *color_string) {
    struct colors color;
    sscanf(++color_string, "%02hx%02hx%02hx", &color.rgb[0], &color.rgb[1], &color.rgb[2]);
    return color;
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
                            int col, int bgcol, int gradient, int gradient_count,
                            char **gradient_color_strings, int horizontal_gradient,
                            int horizontal_gradient_count, char **horizontal_gradient_color_strings,
                            int number_of_bars, int width, int lines, int bar_width,
                            enum orientation orientation, enum orientation blendDirection) {

    free_terminal_noncurses();

    if (gradient)
        free(gradient_colors);
    if (horizontal_gradient)
        free(horizontal_gradient_colors);
    if (gradient && horizontal_gradient)
        free(twodim_gradient_colors);

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
                printf(" "); // setting backround color
            }
            if (n != 0)
                printf("\n");
            else
                printf("\r");
        }
        printf("\033[%dA", lines); // moving cursor back up
    }

    if (gradient) {
        if (orientation == ORIENT_SPLIT_H || orientation == ORIENT_SPLIT_V) {
            lines = lines / 2;
        }
        struct colors gradient_color_defs[MAX_GRADIENT_COLOR_DEFS];
        for (int i = 0; i < gradient_count; i++) {
            gradient_color_defs[i] = parse_color(gradient_color_strings[i]);
        }

        gradient_colors = (struct colors *)malloc((lines * 2) * sizeof(struct colors));

        int individual_size = lines / (gradient_count - 1);

        float rest = lines / (float)(gradient_count - 1) - individual_size;

        float rest_total = 0;
        int gradient_lines = 0;

        for (int i = 0; i < gradient_count - 1; i++) {
            individual_size = lines / (gradient_count - 1);
            if (rest_total > 1.0) {
                individual_size++;
                rest_total = rest_total - 1.0;
            }
            for (int n = 0; n < individual_size; n++) {
                for (int c = 0; c < 3; c++) {
                    float next_color =
                        gradient_color_defs[i + 1].rgb[c] - gradient_color_defs[i].rgb[c];
                    next_color *= n / (float)individual_size;
                    gradient_colors[gradient_lines].rgb[c] =
                        gradient_color_defs[i].rgb[c] + next_color;
                }
                gradient_lines++;
            }
            rest_total = rest_total + rest;
        }
        gradient_colors[lines - 1] = gradient_color_defs[gradient_count - 1];
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
    }

    if (horizontal_gradient && gradient) {
        twodim_gradient_colors =
            (struct colors *)malloc((number_of_bars * lines) * sizeof(struct colors));
        for (int i = 0; i < lines; i++) {
            float current_height = i / (float)lines;
            for (int n = 0; n < number_of_bars; n++) {
                float current_width = n / (float)number_of_bars;
                for (int c = 0; c < 3; c++) {
                    if (blendDirection == ORIENT_BOTTOM) {
                        twodim_gradient_colors[i * number_of_bars + n].rgb[c] =
                            gradient_colors[i].rgb[c] * current_height +
                            horizontal_gradient_colors[n].rgb[c] * (1 - current_height);
                    } else if (blendDirection == ORIENT_TOP) {
                        twodim_gradient_colors[i * number_of_bars + n].rgb[c] =
                            gradient_colors[i].rgb[c] * (1 - current_height) +
                            horizontal_gradient_colors[n].rgb[c] * current_height;
                    } else if (blendDirection == ORIENT_LEFT) {
                        twodim_gradient_colors[i * number_of_bars + n].rgb[c] =
                            gradient_colors[i].rgb[c] * current_width +
                            horizontal_gradient_colors[n].rgb[c] * (1 - current_width);
                    } else if (blendDirection == ORIENT_RIGHT) {
                        twodim_gradient_colors[i * number_of_bars + n].rgb[c] =
                            gradient_colors[i].rgb[c] * (1 - current_width) +
                            horizontal_gradient_colors[n].rgb[c] * current_width;
                    }
                }
            }
        }
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
                            int gradient, int horizontal_gradient, int x_axis_info,
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

    if (offset)
        lines /= 2;

    for (int current_line = lines - 1; current_line >= 0; current_line--) {

        if (orientation == ORIENT_BOTTOM) {
            if (gradient & !horizontal_gradient) {
                if (tty) {
                    cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx, "\033[38;2;%d;%d;%dm",
                                   gradient_colors[current_line].rgb[0],
                                   gradient_colors[current_line].rgb[1],
                                   gradient_colors[current_line].rgb[2]);
                } else if (!tty) {
                    cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[38;2;%d;%d;%dm",
                                   gradient_colors[current_line].rgb[0],
                                   gradient_colors[current_line].rgb[1],
                                   gradient_colors[current_line].rgb[2]);
                }
            }
        } else if (orientation == ORIENT_TOP) {
            if (gradient & !horizontal_gradient) {
                if (tty) {
                    cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx, "\033[38;2;%d;%d;%dm",
                                   gradient_colors[lines - current_line - 1].rgb[0],
                                   gradient_colors[lines - current_line - 1].rgb[1],
                                   gradient_colors[lines - current_line - 1].rgb[2]);
                } else if (!tty) {
                    cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[38;2;%d;%d;%dm",
                                   gradient_colors[lines - current_line - 1].rgb[0],
                                   gradient_colors[lines - current_line - 1].rgb[1],
                                   gradient_colors[lines - current_line - 1].rgb[2]);
                }
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
                                cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx, "\033[%dH",
                                               lines * 2 - current_line);
                        } else {

                            if (rest || same_bar)
                                cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx,
                                               "\033[%d;%dH", lines - current_line,
                                               1 + rest + (bar_width + bar_spacing) * same_bar);
                            else
                                cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx, "\033[%dH",
                                               lines - current_line);
                        }
                        new_line = 0;
                        same_bar = 0;
                    }

                    if (same_bar > 0) {
                        cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx, "\033[%dC",
                                       (bar_width + bar_spacing) * same_bar); // move forward
                        same_bar = 0;
                    }

                    // horizontal gradient
                    if (current_cell > 0) {
                        if (!gradient && horizontal_gradient) {
                            cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx,
                                           "\033[38;2;%d;%d;%dm",
                                           horizontal_gradient_colors[i].rgb[0],
                                           horizontal_gradient_colors[i].rgb[1],
                                           horizontal_gradient_colors[i].rgb[2]);
                        }
                        if (gradient && horizontal_gradient) {
                            if (orientation == ORIENT_BOTTOM) {
                                cx += snprintf(
                                    ttyframe_buffer + cx, ttybuf_length - cx, "\033[38;2;%d;%d;%dm",
                                    twodim_gradient_colors[current_line * number_of_bars + i]
                                        .rgb[0],
                                    twodim_gradient_colors[current_line * number_of_bars + i]
                                        .rgb[1],
                                    twodim_gradient_colors[current_line * number_of_bars + i]
                                        .rgb[2]);
                            } else if (orientation == ORIENT_TOP) {
                                cx += snprintf(ttyframe_buffer + cx, ttybuf_length - cx,
                                               "\033[38;2;%d;%d;%dm",
                                               twodim_gradient_colors[(lines - current_line - 1) *
                                                                          number_of_bars +
                                                                      i]
                                                   .rgb[0],
                                               twodim_gradient_colors[(lines - current_line - 1) *
                                                                          number_of_bars +
                                                                      i]
                                                   .rgb[1],
                                               twodim_gradient_colors[(lines - current_line - 1) *
                                                                          number_of_bars +
                                                                      i]
                                                   .rgb[2]);
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
                                cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[%d;%dH",
                                               lines * 2 - current_line,
                                               1 + rest + (bar_width + bar_spacing) * same_bar);
                            else
                                cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[%dH",
                                               lines * 2 - current_line);
                        } else {
                            if (rest || same_bar)
                                cx += swprintf(frame_buffer + cx, buf_length - cx, L"\033[%d;%dH",
                                               lines - current_line,
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

                    // horizontal gradient
                    if (current_cell > 0) {
                        if (!gradient && horizontal_gradient) {
                            cx +=
                                swprintf(frame_buffer + cx, buf_length - cx, L"\033[38;2;%d;%d;%dm",
                                         horizontal_gradient_colors[i].rgb[0],
                                         horizontal_gradient_colors[i].rgb[1],
                                         horizontal_gradient_colors[i].rgb[2]);
                        }

                        if (gradient && horizontal_gradient) {
                            if (orientation == ORIENT_BOTTOM) {
                                cx += swprintf(
                                    frame_buffer + cx, buf_length - cx, L"\033[38;2;%d;%d;%dm",
                                    twodim_gradient_colors[current_line * number_of_bars + i]
                                        .rgb[0],
                                    twodim_gradient_colors[current_line * number_of_bars + i]
                                        .rgb[1],
                                    twodim_gradient_colors[current_line * number_of_bars + i]
                                        .rgb[2]);
                            } else if (orientation == ORIENT_TOP) {
                                cx += swprintf(frame_buffer + cx, buf_length - cx,
                                               L"\033[38;2;%d;%d;%dm",
                                               twodim_gradient_colors[(lines - current_line - 1) *
                                                                          number_of_bars +
                                                                      i]
                                                   .rgb[0],
                                               twodim_gradient_colors[(lines - current_line - 1) *
                                                                          number_of_bars +
                                                                      i]
                                                   .rgb[1],
                                               twodim_gradient_colors[(lines - current_line - 1) *
                                                                          number_of_bars +
                                                                      i]
                                                   .rgb[2]);
                            }
                        }
                    }

                    if (current_cell < 1) {
                        cx += swprintf(frame_buffer + cx, buf_length - cx, spacestring); // clear
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
