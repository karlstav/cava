#define GL_GLEXT_PROTOTYPES 0
#ifdef _WIN32
#include <GL/glew.h>
#else
#include <SDL2/SDL_opengl.h>
#endif

#include "../config.h"

void init_sdl_glsl_window(int width, int height, int x, int y, int full_screen,
                          char *const vertex_shader, char *const fragment_shader);
GLuint create_gradient_texture(int gradient_count, char **gradient_color_strings);
void init_sdl_glsl_surface(int *width, int *height, char *const fg_color_string,
                           char *const bg_color_string, int bar_width, int bar_spacing,
                           int gradient, int gradient_count, char **gradient_color_strings);
int draw_sdl_glsl(int bars_count, const float bars[], const float previous_bars[], int frame_timer,
                  int re_paint, int continuous_rendering);
void cleanup_sdl_glsl(void);
