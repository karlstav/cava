#include "cava/output/terminal_ncurses.h"

#include <curses.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "cava/util.h"

int gradient_size = 64;

struct colors {
    NCURSES_COLOR_T color;
    NCURSES_COLOR_T R;
    NCURSES_COLOR_T G;
    NCURSES_COLOR_T B;
};

#define COLOR_REDEFINITION -2

#define MAX_COLOR_REDEFINITION 256

const int num_bar_heights = 8;
const wchar_t *bar_heights_bottom[] = {L"\u2581", L"\u2582", L"\u2583", L"\u2584",
                                       L"\u2585", L"\u2586", L"\u2587", L"\u2588"};
const wchar_t *bar_heights_top[] = {L"\u2594",     L"\U0001FB82", L"\U0001FB83", L"\u2580",
                                    L"\U0001FB84", L"\U0001FB85", L"\U0001FB86", L"\u2588"};
const wchar_t *bar_heights_left[] = {L"\u258F", L"\u258E", L"\u258D", L"\u258C",
                                     L"\u258B", L"\u258A", L"\u2589", L"\u2588"};
const wchar_t *bar_heights_right[] = {L"\u2595",     L"\U0001FB87", L"\U0001FB88", L"\u2590",
                                      L"\U0001FB89", L"\U0001FB8A", L"\U0001FB8B", L"\u2588"};
const wchar_t **bar_heights[] = {bar_heights_bottom, bar_heights_top, bar_heights_left,
                                 bar_heights_right};

// static struct colors the_color_redefinitions[MAX_COLOR_REDEFINITION];

static void parse_color(char *color_string, struct colors *color) {
    if (color_string[0] == '#') {
        if (!can_change_color()) {
            cleanup_terminal_ncurses();
            fprintf(stderr, "Your terminal can not change color definitions, "
                            "please use one of the predefined colors.\n");
            exit(EXIT_FAILURE);
        }
        color->color = COLOR_REDEFINITION;
        sscanf(++color_string, "%02hx%02hx%02hx", &color->R, &color->G, &color->B);
    }
}
/*
static void remember_color_definition(NCURSES_COLOR_T color_number) {
        int index = color_number - 1; // array starts from zero and colors - not
        if(the_color_redefinitions[index].color == 0) {
                the_color_redefinitions[index].color = color_number;
                color_content(color_number,
                        &the_color_redefinitions[index].R,
                        &the_color_redefinitions[index].G,
                        &the_color_redefinitions[index].B);
        }
}
*/
// ncurses use color range [0, 1000], and we - [0, 255]
#define CURSES_COLOR_COEFFICIENT(X) ((X)*1000.0 / 0xFF + 0.5)
#define COLORS_STRUCT_NORMALIZE(X)                                                                 \
    CURSES_COLOR_COEFFICIENT(X.R), CURSES_COLOR_COEFFICIENT(X.G), CURSES_COLOR_COEFFICIENT(X.B)

static NCURSES_COLOR_T change_color_definition(NCURSES_COLOR_T color_number,
                                               char *const color_string,
                                               NCURSES_COLOR_T predef_color) {
    struct colors color = {0};
    parse_color(color_string, &color);
    NCURSES_COLOR_T return_color_number = predef_color;
    if (color.color == COLOR_REDEFINITION) {
        // remember_color_definition(color_number);
        init_color(color_number, COLORS_STRUCT_NORMALIZE(color));
        return_color_number = color_number;
    }
    return return_color_number;
}

static void get_screen_coords(int line, int col, int max_value, enum orientation orientation,
                              int *x, int *y) {
    switch (orientation) {
    case ORIENT_LEFT:
        *x = line;
        *y = col;
        break;
    case ORIENT_RIGHT:
        *x = max_value - line;
        *y = col;
        break;
    case ORIENT_TOP:
        *x = col;
        *y = line;
        break;
    default:
        *x = col;
        *y = max_value - line;
        break;
    }
}

void init_terminal_ncurses(char *const fg_color_string, char *const bg_color_string,
                           int predef_fg_color, int predef_bg_color, int gradient,
                           int gradient_count, char **gradient_colors, int *width, int *lines) {
    initscr();
    curs_set(0);
    timeout(0);
    noecho();
    start_color();
    use_default_colors();

    getmaxyx(stdscr, *lines, *width);
    clear();

    NCURSES_COLOR_T color_pair_number = 16;

    NCURSES_COLOR_T bg_color_number;
    bg_color_number = change_color_definition(0, bg_color_string, predef_bg_color);

    if (!gradient) {

        NCURSES_COLOR_T fg_color_number;
        fg_color_number = change_color_definition(1, fg_color_string, predef_fg_color);

        init_pair(color_pair_number, fg_color_number, bg_color_number);

    } else if (gradient) {

        // 0 -> col1, 1-> col1<=>col2, 2 -> col2 and so on
        short unsigned int rgb[2 * gradient_count - 1][3];
        char next_color[14];

        gradient_size = *lines;

        if (gradient_size > COLORS)
            gradient_size = COLORS - 1;

        if (gradient_size > COLOR_PAIRS)
            gradient_size = COLOR_PAIRS - 1;

        if (gradient_size > MAX_COLOR_REDEFINITION)
            gradient_size = MAX_COLOR_REDEFINITION - 1;

        for (int i = 0; i < gradient_count; i++) {
            int col = (i + 1) * 2 - 2;
            sscanf(gradient_colors[i] + 1, "%02hx%02hx%02hx", &rgb[col][0], &rgb[col][1],
                   &rgb[col][2]);
        }

        // sscanf(gradient_color_1 + 1, "%02hx%02hx%02hx", &rgb[0][0], &rgb[0][1], &rgb[0][2]);
        // sscanf(gradient_color_2 + 1, "%02hx%02hx%02hx", &rgb[1][0], &rgb[1][1], &rgb[1][2]);

        int individual_size = gradient_size / (gradient_count - 1);

        for (int i = 0; i < gradient_count - 1; i++) {

            int col = (i + 1) * 2 - 2;
            if (i == gradient_count - 1)
                col = 2 * (gradient_count - 1) - 2;

            for (int j = 0; j < individual_size; j++) {

                for (int k = 0; k < 3; k++) {
                    rgb[col + 1][k] = rgb[col][k] + (rgb[col + 2][k] - rgb[col][k]) *
                                                        (j / (individual_size * 0.85));
                    if (rgb[col + 1][k] > 255)
                        rgb[col][k] = 0;
                    if (j > individual_size * 0.85)
                        rgb[col + 1][k] = rgb[col + 2][k];
                }

                sprintf(next_color, "#%02x%02x%02x", rgb[col + 1][0], rgb[col + 1][1],
                        rgb[col + 1][2]);

                change_color_definition(color_pair_number, next_color, color_pair_number);
                init_pair(color_pair_number, color_pair_number, bg_color_number);
                color_pair_number++;
            }
        }

        int left = individual_size * (gradient_count - 1);
        int col = 2 * (gradient_count)-2;
        while (left < gradient_size) {
            sprintf(next_color, "#%02x%02x%02x", rgb[col][0], rgb[col][1], rgb[col][2]);
            change_color_definition(color_pair_number, next_color, color_pair_number);
            init_pair(color_pair_number, color_pair_number, bg_color_number);
            color_pair_number++;
            left++;
        }
        color_pair_number--;
    }

    attron(COLOR_PAIR(color_pair_number));

    if (bg_color_number != -1)
        bkgd(COLOR_PAIR(color_pair_number));

    for (int y = 0; y < *lines; y++) {
        for (int x = 0; x < *width; x++) {
            mvaddch(y, x, ' ');
        }
    }
    refresh();
}

void change_colors(int cur_height, int tot_height) {
    tot_height /= gradient_size;
    if (tot_height < 1)
        tot_height = 1;
    cur_height /= tot_height;
    if (cur_height > gradient_size - 1)
        cur_height = gradient_size - 1;
    attron(COLOR_PAIR(cur_height + 16));
}

void get_terminal_dim_ncurses(int *width, int *height) {
    getmaxyx(stdscr, *height, *width);
    gradient_size = *height;
    clear(); // clearing in case of resieze
}

#define TERMINAL_RESIZED -1

int draw_terminal_ncurses(int is_tty, int dimension_value, int dimension_bar, int bars_count,
                          int bar_width, int bar_spacing, int rest, const int bars[],
                          int previous_frame[], int gradient, int x_axis_info,
                          enum orientation orientation) {
    const int max_value = dimension_value - 1;

    // output: check if terminal has been resized
    if (!is_tty) {
        if (x_axis_info)
            dimension_value++;
        int terminal_width, terminal_height;
        if (orientation == ORIENT_LEFT || orientation == ORIENT_RIGHT) {
            terminal_width = dimension_value;
            terminal_height = dimension_bar;
        } else {
            terminal_width = dimension_bar;
            terminal_height = dimension_value;
        }
        if (LINES != terminal_height || COLS != terminal_width) {
            return TERMINAL_RESIZED;
            if (x_axis_info)
                dimension_value--;
        }
    }

    // Compute how much of the screen we possibly need to update ahead-of-time.
    int max_update_value = 0;
    for (int bar = 0; bar < bars_count; bar++) {
        max_update_value = max(max_update_value, max(bars[bar], previous_frame[bar]));
    }

    max_update_value = (max_update_value + num_bar_heights) / num_bar_heights;

    for (int line = 0; line < max_update_value; line++) {
        if (gradient) {
            change_colors(line, max_value);
        }

        for (int bar = 0; bar < bars_count; bar++) {
            if (bars[bar] == previous_frame[bar]) {
                continue;
            }

            int cur_col = bar * bar_width + bar * bar_spacing + rest;
            int bar_line_height = bars[bar] / num_bar_heights;
            int previous_bar_line_heigh = previous_frame[bar] / num_bar_heights;

            if (bars[bar] >= line * num_bar_heights + 1) {
                int bar_step;

                if (bar_line_height == line) {
                    // The "cap" of the bar occurs at this [line].
                    bar_step = bars[bar] % num_bar_heights - 1;
                } else if (previous_bar_line_heigh <= line) {
                    // The bar is full at this line and wasn't before.
                    bar_step = num_bar_heights - 1;
                } else {
                    // No update necessary since last frame.
                    continue;
                }

                for (int col = cur_col, i = 0; i < bar_width; i++, col++) {
                    int x, y;
                    get_screen_coords(line, col, max_value, orientation, &x, &y);

                    if (is_tty) {
                        mvaddch(y, x, 0x41 + bar_step);
                    } else {
                        mvaddwstr(y, x, bar_heights[orientation][bar_step]);
                    }
                }
            } else if (previous_bar_line_heigh >= line) {
                // This bar was taller during the last frame than during this frame, so
                // clear the excess characters.
                for (int col = cur_col, i = 0; i < bar_width; i++, col++) {
                    int x, y;
                    get_screen_coords(line, col, max_value, orientation, &x, &y);
                    mvaddch(y, x, ' ');
                }
            }
        }
    }

    refresh();
    return 0;
}

// general: cleanup
void cleanup_terminal_ncurses(void) {
    echo();
#ifdef __FreeBSD__
    system("vidcontrol -f >/dev/null 2>&1");
#else
    system("setfont  >/dev/null 2>&1");
    system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz  >/dev/null 2>&1");
#endif

    /*for(int i = 0; i < gradient_size; ++i) {
            if(the_color_redefinitions[i].color) {
                    init_color(the_color_redefinitions[i].color,
                            the_color_redefinitions[i].R,
                            the_color_redefinitions[i].G,
                            the_color_redefinitions[i].B);
            }
    }
*/
    standend();
    endwin();
    system("clear");
}
