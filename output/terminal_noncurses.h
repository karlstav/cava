int init_terminal_noncurses(int col, int bgcol, int w, int h);
void get_terminal_dim_noncurses(int *w, int *h);
int draw_terminal_noncurses(int virt, int height, int width, int bands, int bw, int rest, int f[200], int flastd[200]);
void cleanup_terminal_noncurses(void);
