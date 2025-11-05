#pragma once

#include <errno.h>
#include <fcntl.h>
#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#ifdef NCURSES
#include <curses.h>
#endif
#include <locale.h>
#include <math.h>
#include <stddef.h>
#include <sys/stat.h>

#include "cava/common.h"
#include "input/common.h"
#include "output/common.h"
#ifdef NCURSES
#include "output/terminal_bcircle.h"
#include "output/terminal_ncurses.h"
#endif
#include "debug.h"
#include "util.h"
