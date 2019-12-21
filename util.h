#pragma once

#define max(a, b)                                                                                  \
    ({                                                                                             \
        __typeof__(a) _a = (a);                                                                    \
        __typeof__(b) _b = (b);                                                                    \
        _a > _b ? _a : _b;                                                                         \
    })

#define ARRAY_SIZE(x) ((sizeof(x) / sizeof(0 [x])) / ((size_t)(!(sizeof(x) % sizeof(0 [x])))))
