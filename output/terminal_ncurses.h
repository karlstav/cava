
int init_terminal_ncurses(int col, int bgcol);
void get_terminal_dim_ncurses(int *w, int *h);
int draw_terminal_ncurses(int virt, int height, int width, int bars, int bw, int bs, int rest, int f[200], int flastd[200]);
void cleanup_terminal_ncurses(void);
