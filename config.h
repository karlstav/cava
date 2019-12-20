#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

bool load_config(char configPath[255], char supportedInput[255], void *p, bool colorsOnly,
                 void *error);
