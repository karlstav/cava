#include <curses.h>
#include <locale.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.141592
#endif
#define DEGTORAD(deg) (deg * (180.0f/M_PI))
#define DOT 0x2588


int init_terminal_bcircle(int col, int bgcol) {

  initscr();
  curs_set(0);
  timeout(0);
  noecho();
  start_color();
  use_default_colors();
  init_pair(1, col, bgcol);
  if(bgcol != -1)
    bkgd(COLOR_PAIR(1));
  attron(COLOR_PAIR(1));
//  attron(A_BOLD);
  return 0;
}


void get_terminal_dim_bcircle(int *w, int *h) {

  getmaxyx(stdscr,*h,*w);
  clear(); //clearing in case of resieze
}


int draw_terminal_bcircle(int virt, int h, int w, int f[200]) {

  const wchar_t* bars[] = {L"\u2581", L"\u2582", L"\u2583", L"\u2584", L"\u2585", L"\u2586", L"\u2587", L"\u2588"};

  // output: check if terminal has been resized
  if (virt != 0) {
    if ( LINES != h || COLS != w) {
      return -1;
    }
  }

  float deg, width, height;
  int y, x;

  /* Convert to int */
  width  = f[1]/10;
  height = f[1]/15;

  int oy, ox;
  oy = LINES/2 - height/2;
  ox = COLS/2 - width/2;
  for (x = 0; x < COLS; x++) {
    for (y = 0; y < LINES; y++) {
      mvaddstr(y,x," ");
    }
  }
  /* Draw circle */
  for (deg = 0; deg < 360.0f; deg += 1.0f) {
    x = ox + width+(int)(width * cos(DEGTORAD(deg)));
    y = oy + height+(int)(height * sin(DEGTORAD(deg)));

    mvaddwstr(y,x,bars[7]);
  }


  refresh();
  return 0;
}


// general: cleanup
void cleanup_terminal_bcircle(void)
{
  echo();
  system("setfont >/dev/null 2>&1");
  system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz  >/dev/null 2>&1");
  system("setterm -blank 10");
  endwin();
  system("clear");
}

