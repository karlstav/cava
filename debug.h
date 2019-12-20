#include <stdio.h>

#ifdef NDEBUG
#define debug(...)                                                                                 \
    do {                                                                                           \
    } while (0)
#else
#define debug(...) fprintf(stderr, __VA_ARGS__)
#endif
