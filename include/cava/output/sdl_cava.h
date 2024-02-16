#include "../config.h"

void init_sdl_window(int width, int height, int x, int y, int full_screen);
void init_sdl_surface(int *width, int *height, char *const fg_color_string,
                      char *const bg_color_string, int gradient, int gradient_count,
                      char **gradient_color_strings);
int draw_sdl(int bars_count, int bar_width, int bar_spacing, int remainder, int height,
             const int bars[], int previous_frame[], int frame_timer, enum orientation orientation,
             int gradient);
void cleanup_sdl(void);
