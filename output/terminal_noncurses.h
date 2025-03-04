#include "../config.h"

int init_terminal_noncurses(int inAtty, char *const fg_color_string, char *const bg_color_string,
                            int col, int bgcol, int gradient, int gradient_count,
                            char **gradient_colors, int horizontal_gradient,
                            int horizontal_gradient_count, char **horizontal_gradient_colors,
                            int number_of_bars, int w, int h, int bar_width,
                            enum orientation orientation, enum orientation blendDirection);
void get_terminal_dim_noncurses(int *w, int *h);
int draw_terminal_noncurses(int inAtty, int lines, int width, int number_of_bars, int bar_width,
                            int bar_spacing, int rest, int bars[], int previous_frame[],
                            int gradient, int horizontal_gradient, int x_axis_info,
                            enum orientation orientation, int offset);
void cleanup_terminal_noncurses(void);
