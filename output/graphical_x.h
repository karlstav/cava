int init_window_x(char *color, char *bcolor, int col, int bgcol, char **argv, int argc);
int apply_window_settings_x(void);
int get_window_input_x(int *should_reload, int *bs, double *sens, int *bw, int *modes, int *mode, int *w, int *h, char *color, char *bcolor);
void draw_graphical_x(int window_height, int bars_count, int bar_width, int bar_spacing, int rest, int f[200], int flastd[200]);
void cleanup_graphical_x(void);