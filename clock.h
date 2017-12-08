#define CLOCK
// comment above line to disable clock functionality

#ifndef _CLOCK_H
#define _CLOCK_H
#ifdef CLOCK

#define SILENCE 1
#define NORMAL 0

#define LINE_MAX 128

#define CLOCK_DELAY (int)0x2f
#define CLOCK_SILENCE_DELAY (int)0x3

#define DIGIT_LINE_WIDTH 2


#define BOX_TOP_LEFT L'\u250C'
#define BOX_TOP_RIGHT L'\u2510'
#define BOX_BOTTOM_LEFT L'\u2514'
#define BOX_BOTTOM_RIGHT L'\u2518'
#define BOX_HORIZ_LINE L'\u2500'
#define BOX_VERT_LINE L'\u2502'

// Function Prototypes
void draw_clock(int);
void refresh_clock(void);


#endif
#endif