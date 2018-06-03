#include <windows.h>
#include <windowsx.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <dwmapi.h>
#include <assert.h>
#include <tchar.h>

#include "output/graphical.h"

#ifdef assert
#define vertify(expr) if(!expr) assert(0)
#else 
#define vertify(expr) expr
#endif

// define functions
int init_window_win(char *color, char *bcolor, double foreground_opacity, int col, int bgcol, int gradient, char *gradient_color_1, char *gradient_color_2, unsigned int shdw, unsigned int shdw_col, int w, int h);
void apply_win_settings(int w, int h);
int get_window_input_win(int *should_reload, int *bs, double *sens, int *bw, int *w, int *h);
void draw_graphical_win(int window_height, int bars_count, int bar_width, int bar_spacing, int rest, int gradient, int f[200]);
void cleanup_graphical_win(void);
