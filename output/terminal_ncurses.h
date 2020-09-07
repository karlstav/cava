void init_terminal_ncurses(char *const fg_color_string, char *const bg_color_string,
                           int predef_fg_color, int predef_bg_color, int gradient,
                           int gradient_count, char **gradient_colors, int *width, int *height);
void get_terminal_dim_ncurses(int *width, int *height);
int draw_terminal_ncurses(int is_tty, int terminal_height, int terminal_width, int bars_count,
                          int bar_width, int bar_spacing, int rest, const int bars[256],
                          int previous_frame[256], int gradient, int x_axis_info);
void cleanup_terminal_ncurses(void);
