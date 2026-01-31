#include "../config.h"

int init_terminal_noncurses(int inAtty, char *const fg_color_string, char *const bg_color_string,
                            int col, int bgcol, int gradient, int gradient_count,
                            char **gradient_colors, int horizontal_gradient,
                            int horizontal_gradient_count, char **horizontal_gradient_colors,
                            int number_of_bars, int w, int h, int bar_width,
                            enum orientation orientation, enum orientation blendDirection);
void get_terminal_dim_noncurses(int *w, int *h);
int draw_terminal_noncurses(int bars[], int previous_frame[], enum orientation orientation,
                            void *config);
void cleanup_terminal_noncurses(void);
