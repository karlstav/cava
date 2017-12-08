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

// const wchar_t* bar_heights[] = {L'\u2581', L'\u2582', L'\u2583',L'\u2584', L'\u2585', L'\u2586', L'\u2587', L'\u2588'};

void refresh_clock() {
    // refresh clock
    time_t t = time(NULL);
    tm = localtime(&t);
    strftime(s, sizeof(s), "%H:%M:%S", tm);
    // reset clock delay counter
    clock_cd = 0;
    return;
}

void draw_clock(int mode) {

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
    int time_width = 4*(DIGIT_LINE_WIDTH*DIGIT_WIDTH+2)+1*(DIGIT_LINE_WIDTH*COLON_WIDTH+2)+2;
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
        mvaddnwstr(i,0,(const wchar_t*)&line,time_width);
    }

    return;
}

#endif