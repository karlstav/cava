int init_terminal_noncurses(int inAtty, int col, int bgcol, int w, int h, int bar_width);
void get_terminal_dim_noncurses(int *w, int *h);
int draw_terminal_noncurses(int inAtty, int lines, int width, int number_of_bars, int bar_width,
                            int bar_spacing, int rest, int bars[256], int previous_frame[256],
                            int x_axis_info);
void cleanup_terminal_noncurses(void);
