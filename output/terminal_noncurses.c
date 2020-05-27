#include <locale.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include <wchar.h>

wchar_t *line_buffer;
wchar_t *barstring[8];
wchar_t *wspacestring;
int buf_length;
char *ttyline_buffer;
char ttybarstring[8][100];
char ttyspacestring[100];
int ttybuf_length;

int setecho(int fd, int onoff) {

    struct termios t;

    if (tcgetattr(fd, &t) == -1)
        return -1;

    if (onoff == 0)
        t.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
    else
        t.c_lflag |= (ECHO | ECHOE | ECHOK | ECHONL);

    if (tcsetattr(fd, TCSANOW, &t) == -1)
        return -1;

    return 0;
}

int init_terminal_noncurses(int col, int bgcol, int w, int h, int bar_width) {

    int n, i;

    ttybuf_length = sizeof(char) * w * h * 10;
    ttyline_buffer = (char *)malloc(ttybuf_length);

    buf_length = sizeof(wchar_t) * w * h * 10;
    line_buffer = (wchar_t *)malloc(buf_length);
    wspacestring = (wchar_t *)malloc(sizeof(wchar_t) * bar_width);

    // clearing barstrings
    for (n = 0; n < 8; n++) {
        barstring[n] = (wchar_t *)malloc(sizeof(wchar_t) * bar_width);
        ttybarstring[n][0] = '\0';
        barstring[n][0] = '\0';
        ttyspacestring[0] = '\0';
    }
    wspacestring[0] = '\0';
    line_buffer[0] = '\0';
    ttyline_buffer[0] = '\0';

    // creating barstrings for drawing
    for (n = 0; n < bar_width; n++) {

        wcscat(barstring[0], L"\u2588");
        wcscat(barstring[1], L"\u2581");
        wcscat(barstring[2], L"\u2582");
        wcscat(barstring[3], L"\u2583");
        wcscat(barstring[4], L"\u2584");
        wcscat(barstring[5], L"\u2585");
        wcscat(barstring[6], L"\u2586");
        wcscat(barstring[7], L"\u2587");
        wcscat(wspacestring, L" ");

        strcat(ttybarstring[0], "8");
        strcat(ttybarstring[1], "1");
        strcat(ttybarstring[2], "2");
        strcat(ttybarstring[3], "3");
        strcat(ttybarstring[4], "4");
        strcat(ttybarstring[5], "5");
        strcat(ttybarstring[6], "6");
        strcat(ttybarstring[7], "7");
        strcat(ttyspacestring, " ");
    }

    col += 30;

    system("setterm -cursor off");
    system("setterm -blank 0");

    // output: reset console
    printf("\033[0m\n");
    system("clear");

    if (col)
        printf("\033[%dm", col); // setting color

    // printf("\033[1m"); // setting "bright" color mode, looks cooler... I think

    if (bgcol != 0) {

        bgcol += 40;
        printf("\033[%dm", bgcol);

        for (n = (h); n >= 0; n--) {
            for (i = 0; i < w; i++) {

                printf(" "); // setting backround color
            }
            printf("\n");
        }
        printf("\033[%dA", h); // moving cursor back up
    }

    setecho(STDIN_FILENO, 0);

    return 0;
}

void get_terminal_dim_noncurses(int *w, int *h) {

    struct winsize dim;

    ioctl(STDOUT_FILENO, TIOCGWINSZ, &dim);

    *h = (int)dim.ws_row;
    *w = (int)dim.ws_col;

    system("clear"); // clearing in case of resieze
}

int draw_terminal_noncurses(int tty, int h, int w, int bars, int bar_width, int bs, int rest,
                            int f[200], int flastd[200]) {
    int current_char, last_char, n, o, same_line, new_line, cx;

    struct winsize dim;

    same_line = 0;
    new_line = 0;
    cx = 0;

    if (tty) {

        ttyline_buffer[0] = '\0';

        for (n = h - 1; n >= 0; n--) {

            int same_bar = 0;
            int center_adjusted = 0;

            for (o = 0; o < bars; o++) {

                current_char = f[o] - n * 8;
                last_char = flastd[o] - n * 8;

                // same as last frame
                if ((current_char < 1 && last_char < 1) || (current_char > 7 && last_char > 7) ||
                    (current_char == last_char)) {
                    same_bar++;
                } else {
                    if (same_line > 0) {
                        cx += snprintf(ttyline_buffer + cx, ttybuf_length - cx, "\033[%dB",
                                       same_line); // move down
                        new_line += same_line;
                        same_line = 0;
                    }

                    if (same_bar > 0) {
                        cx += snprintf(ttyline_buffer + cx, ttybuf_length - cx, "\033[%dC",
                                       (bar_width + bs) * same_bar); // move forward
                        same_bar = 0;
                    }

                    if (!center_adjusted) {
                        cx += snprintf(ttyline_buffer + cx, ttybuf_length - cx, "\033[%dC", rest);
                        center_adjusted = 1;
                    }

                    if (current_char < 1)
                        cx +=
                            snprintf(ttyline_buffer + cx, ttybuf_length - cx, "%s", ttyspacestring);
                    else if (current_char > 7)
                        cx += snprintf(ttyline_buffer + cx, ttybuf_length - cx, "%s",
                                       ttybarstring[0]);
                    else
                        cx += snprintf(ttyline_buffer + cx, ttybuf_length - cx, "%s",
                                       ttybarstring[current_char]);

                    cx += snprintf(ttyline_buffer + cx, ttybuf_length - cx, "\033[%dC", bs);
                }
            }

            if (same_bar != bars) {
                if (n != 0) {
                    cx += snprintf(ttyline_buffer + cx, ttybuf_length - cx, "\n");
                    new_line++;
                }
            } else {
                same_line++;
            }
        }
        if (same_line != h) {
            printf("%s\r\033[%dA", ttyline_buffer,
                   new_line); //\r\033[%dA", ttyline_buffer,  h - same_line);
            fflush(stdout);
        }
    } else if (!tty) {

        // output: check if terminal has been resized
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &dim);

        if ((int)dim.ws_row != h || (int)dim.ws_col != w) {
            free(line_buffer);
            free(wspacestring);
            for (int i = 0; i < 8; i++)
                free(barstring[i]);
            return -1;
        }

        line_buffer[0] = '\0';

        for (n = h - 1; n >= 0; n--) {

            int same_bar = 0;
            int center_adjusted = 0;

            for (o = 0; o < bars; o++) {

                current_char = f[o] - n * 8;
                last_char = flastd[o] - n * 8;

                // same as last frame
                if ((current_char < 1 && last_char < 1) || (current_char > 7 && last_char > 7) ||
                    (current_char == last_char)) {
                    same_bar++;
                } else {
                    if (same_line > 0) {
                        cx += swprintf(line_buffer + cx, buf_length - cx, L"\033[%dB",
                                       same_line); // move down
                        new_line += same_line;
                        same_line = 0;
                    }

                    if (same_bar > 0) {
                        cx += swprintf(line_buffer + cx, buf_length - cx, L"\033[%dC",
                                       (bar_width + bs) * same_bar); // move forward
                        same_bar = 0;
                    }

                    if (!center_adjusted && rest) {
                        cx += swprintf(line_buffer + cx, buf_length - cx, L"\033[%dC", rest);
                        center_adjusted = 1;
                    }

                    if (current_char < 1)
                        cx += swprintf(line_buffer + cx, buf_length - cx, wspacestring);
                    else if (current_char > 7)
                        cx += swprintf(line_buffer + cx, buf_length - cx, barstring[0]);
                    else
                        cx += swprintf(line_buffer + cx, buf_length - cx, barstring[current_char]);

                    cx += swprintf(line_buffer + cx, buf_length - cx, L"\033[%dC", bs);
                }
            }

            if (same_bar != bars) {
                if (n != 0) {
                    cx += swprintf(line_buffer + cx, buf_length - cx, L"\n");
                    new_line++;
                }
            } else {
                same_line++;
            }
        }
        if (same_line != h) {
            printf("%ls\r\033[%dA", line_buffer,
                   new_line); //\r\033[%dA", line_buffer,  h - same_line);
            fflush(stdout);
        }
    }
    return 0;
}

// general: cleanup
void cleanup_terminal_noncurses(void) {
    setecho(STDIN_FILENO, 1);
    printf("\033[0m\n");
    system("setfont  >/dev/null 2>&1");
    system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz  >/dev/null 2>&1");
    system("setterm -cursor on");
    system("setterm -blank 10");
    system("clear");
}
