// header file for coreaudio taps, part of cava.

#pragma once

int coreaudio_tap_source_enabled(const char *source);
void *input_coreaudio_tap(void *data);
