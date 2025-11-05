#pragma once

#include "../common.h"

#ifdef SDL
#include "sdl_cava.h"
#endif

#ifdef SDL_GLSL
#include "sdl_glsl.h"
#endif

#include "noritake.h"
#include "raw.h"
#include "terminal_noncurses.h"

#ifndef _WIN32
#include "terminal_bcircle.h"
#include "terminal_ncurses.h"
#endif
