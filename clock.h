#define CLOCK
// comment above line to disable clock functionality

#ifndef _CLOCK_H
#define _CLOCK_H
#ifdef CLOCK

#include <stdbool.h>

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

#define CLOCK_POS_TOP_LEFT 0
#define CLOCK_POS_TOP_RIGHT 1
#define CLOCK_POS_BOTTOM_LEFT 2
#define CLOCK_POS_BOTTOM_RIGHT 3
#define CLOCK_POS_LEFT_CENTER 4
#define CLOCK_POS_RIGHT_CENTER 5
#define CLOCK_POS_TOP_CENTER 6
#define CLOCK_POS_BOTTOM_CENTER 7
#define CLOCK_POS_MIDDLE 8

// Function Prototypes
void draw_clock(int,int,int,int,int);
void refresh_clock(void);


#endif
#endif