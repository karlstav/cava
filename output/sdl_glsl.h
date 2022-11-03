#include "../config.h"

void init_sdl_glsl_window(int width, int height, int x, int y, char *const vertex_shader,
                          char *const fragment_shader);
void init_sdl_glsl_surface(int *width, int *height, char *const fg_color_string,
                           char *const bg_color_string);
int draw_sdl_glsl(int bars_count, const float bars[], int frame_timer, int re_paint,
                  int continuous_rendering);
void cleanup_sdl_glsl(void);
