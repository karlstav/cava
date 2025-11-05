#pragma once
#include "common.h"

#ifndef _WIN32
int print_ntk_out(int bars_count, int fd, int bit_format, int bar_width, int bar_spacing,
                  int bar_height, int const f[]);
#else
int print_ntk_out(int bars_count, HANDLE hFile, int bit_format, int bar_width, int bar_spacing,
                  int bar_height, int const f[]);
#endif
