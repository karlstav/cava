
int init_terminal_ncurses(int col, int bgcol);
int get_terminal_height_ncurses(void);
int get_terminal_width_ncurses(void);
void get_terminal_dim_ncurses(int *w, int *h);
int draw_terminal_ncurses(int virt, int height, int width, int bands, int bw, int rest, int f[200], int flastd[200]);
void cleanup_terminal_ncurses(void);
