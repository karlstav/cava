#include "clock.h"
#include <time.h>
#include <curses.h>
#include <wchar.h>

#ifdef CLOCK

#define DIGIT_HEIGHT 5
#define DIGIT_WIDTH 3
const wchar_t digit[10][DIGIT_HEIGHT][DIGIT_WIDTH] = {
    /* 0 */ {{L'\u2588',L'\u2588',L'\u2588'},{L'\u2588',L'\u2005',L'\u2588'},{L'\u2588',L'\u2005',L'\u2588'},{L'\u2588',L'\u2005',L'\u2588'},{L'\u2588',L'\u2588',L'\u2588'}},
    /* 1 */ {{L'\u2005',L'\u2005',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'}},
    /* 2 */ {{L'\u2588',L'\u2588',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2588',L'\u2588',L'\u2588'},{L'\u2588',L'\u2005',L'\u2005'},{L'\u2588',L'\u2588',L'\u2588'}},
    /* 3 */ {{L'\u2588',L'\u2588',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2588',L'\u2588',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2588',L'\u2588',L'\u2588'}},
    /* 4 */ {{L'\u2588',L'\u2005',L'\u2588'},{L'\u2588',L'\u2005',L'\u2588'},{L'\u2588',L'\u2588',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'}},
    /* 5 */ {{L'\u2588',L'\u2588',L'\u2588'},{L'\u2588',L'\u2005',L'\u2005'},{L'\u2588',L'\u2588',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2588',L'\u2588',L'\u2588'}},
    /* 6 */ {{L'\u2588',L'\u2588',L'\u2588'},{L'\u2588',L'\u2005',L'\u2005'},{L'\u2588',L'\u2588',L'\u2588'},{L'\u2588',L'\u2005',L'\u2588'},{L'\u2588',L'\u2588',L'\u2588'}},
    /* 7 */ {{L'\u2588',L'\u2588',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'}},
    /* 8 */ {{L'\u2588',L'\u2588',L'\u2588'},{L'\u2588',L'\u2005',L'\u2588'},{L'\u2588',L'\u2588',L'\u2588'},{L'\u2588',L'\u2005',L'\u2588'},{L'\u2588',L'\u2588',L'\u2588'}},
    /* 9 */ {{L'\u2588',L'\u2588',L'\u2588'},{L'\u2588',L'\u2005',L'\u2588'},{L'\u2588',L'\u2588',L'\u2588'},{L'\u2005',L'\u2005',L'\u2588'},{L'\u2588',L'\u2588',L'\u2588'}}
};
#define COLON_HEIGHT 5
#define COLON_WIDTH 1
const wchar_t colon[COLON_HEIGHT][COLON_WIDTH] = {{L'\u2005'},{L'\u2588'},{L'\u2005'},{L'\u2588'},{L'\u2005'}};

int clock_cd = 0;
struct tm *tm;
char s[64];
wchar_t line[LINE_MAX] = {0};

const int time_width = 4*(DIGIT_LINE_WIDTH*DIGIT_WIDTH+2)+1*(DIGIT_LINE_WIDTH*COLON_WIDTH+2)+2;

// const wchar_t* bar_heights[] = {L'\u2581', L'\u2582', L'\u2583',L'\u2584', L'\u2585', L'\u2586', L'\u2587', L'\u2588'};

void refresh_clock() {
    // refresh clock
    time_t t = time(NULL);
    tm = localtime(&t);
    strftime(s, sizeof(s), "%H:%M", tm);
    // reset clock delay counter
    clock_cd = 0;
    return;
}

void draw_clock(int mode, int show_clock, int cpos, int terminal_height, int terminal_width) {

    static int prev_show_clock=0, prev_cpos=0, x_offset=0, y_offset=0, prev_h=0, prev_w=0;

    if (show_clock) {
        prev_show_clock = show_clock;

        // set clock position
        if (cpos!=prev_cpos || terminal_height!=prev_h || terminal_width!=prev_w) {
            // erasing previous clock
            for (int i=0;i<time_width;i++) {
                line[i] = L'\u2005';
            }
            for (int i=0;i<DIGIT_HEIGHT+2;i++) {
                mvaddnwstr(y_offset+i,x_offset,(const wchar_t*)&line,time_width);
            }
            switch (cpos) {
                case CLOCK_POS_TOP_LEFT: x_offset=0; y_offset=0; break;
                case CLOCK_POS_TOP_RIGHT: x_offset=terminal_width-time_width; y_offset=0; break;
                case CLOCK_POS_BOTTOM_LEFT: x_offset=0; y_offset=terminal_height-(DIGIT_HEIGHT+2); break;
                case CLOCK_POS_BOTTOM_RIGHT: x_offset=terminal_width-time_width; y_offset=terminal_height-(DIGIT_HEIGHT+2); break;
                case CLOCK_POS_LEFT_CENTER: x_offset=0; y_offset=(terminal_height-(DIGIT_HEIGHT+2))/2; break;
                case CLOCK_POS_RIGHT_CENTER: x_offset=terminal_width-time_width; y_offset=(terminal_height-(DIGIT_HEIGHT+2))/2; break;
                case CLOCK_POS_TOP_CENTER: x_offset=(terminal_width-time_width)/2; y_offset=0; break;
                case CLOCK_POS_BOTTOM_CENTER: x_offset=(terminal_width-time_width)/2; y_offset=terminal_height-(DIGIT_HEIGHT+2); break;
                case CLOCK_POS_MIDDLE: x_offset=(terminal_width-time_width)/2; y_offset=(terminal_height-(DIGIT_HEIGHT+2))/2; break;
            }
            prev_cpos = cpos;
            prev_w = terminal_width;
            prev_h = terminal_height;
            // show_clock = 0;
        }

        // wait for redraw
        if (mode == NORMAL) {
            if ((clock_cd++) > CLOCK_DELAY) {
                refresh_clock();
            }
        } else if (mode == SILENCE) {
            if ((clock_cd++) > CLOCK_SILENCE_DELAY) {
                refresh_clock();
            }
        }


        // draw clock    
        for (int i=0;i<DIGIT_HEIGHT+2;i++) {
            if (i==0) {
                line[0] = BOX_TOP_LEFT;
                for (int o=1;o<time_width-1;o++) {
                    line[o] = BOX_HORIZ_LINE;
                }
                line[time_width-1] = BOX_TOP_RIGHT;
            } else if (i==DIGIT_HEIGHT+1) {
                line[0] = BOX_BOTTOM_LEFT;
                for (int o=1;o<time_width-1;o++) {
                    line[o] = BOX_HORIZ_LINE;
                }
                line[time_width-1] = BOX_BOTTOM_RIGHT;
            } else {
                line[0] = BOX_VERT_LINE;
                int j=1;
                for (int k=0;k<5;k++) {
                    char x = s[k]-'0';
                    if (x==10) {
                        line [j++] = L'\u2005';
                        for (int a=0;a<DIGIT_LINE_WIDTH;a++) {
                            line [j++] = colon[i-1][0];
                        }
                        line [j++] = L'\u2005';
                    } else {
                        line [j++] = L'\u2005';
                        for (int yc=0;yc<DIGIT_WIDTH;yc++,j++) {
                            for (int a=0;a<DIGIT_LINE_WIDTH;a++) {
                                line[j+a] = digit[(int)x][i-1][yc];
                            }
                            j+=(DIGIT_LINE_WIDTH-1);
                        }
                        line [j++] = L'\u2005';
                    }
                }
                line[j] = BOX_VERT_LINE;
            }
            mvaddnwstr(y_offset+i,x_offset,(const wchar_t*)&line,time_width);
        }
    } else if (prev_show_clock!=show_clock) {
        // clock is disabled, erasing it
        for (int i=0;i<time_width;i++) {
            line[i] = L'\u2005';
        }
        for (int i=0;i<DIGIT_HEIGHT+2;i++) {
            mvaddnwstr(y_offset+i,x_offset,(const wchar_t*)&line,time_width);
        }
        prev_show_clock = show_clock;
    }

    return;
}

#endif