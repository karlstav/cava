#include "cava/common.h"

// general: cleanup
void cleanup(int output_mode) {
    if (output_mode == OUTPUT_NCURSES) {
#ifdef NCURSES
        cleanup_terminal_ncurses();
#else
        ;
#endif
    } else if (output_mode == OUTPUT_NONCURSES) {
        cleanup_terminal_noncurses();
    } else if (output_mode == OUTPUT_SDL) {
#ifdef SDL
        cleanup_sdl();
#else
        ;
#endif
#ifdef SDL_GLSL
    } else if (output_mode == OUTPUT_SDL_GLSL) {
        cleanup_sdl_glsl();
#else
        ;
#endif
    }
}
