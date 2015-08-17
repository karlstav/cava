#include <curses.h>
#include <locale.h>
#include <wchar.h>
#include <stdio.h>

int init_terminal_ncurses(int col, int bgcol) {

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

	///
	init_pair(2, COLOR_GREEN, bgcol);
	init_pair(3, COLOR_YELLOW, bgcol);
	init_pair(4, COLOR_RED, bgcol);


//	attron(A_BOLD);
	return 0;
}

void color_switch(float value, float max_value, float thresh1, float thresh2, float thresh3)
{
    float perc = (max_value - value) / max_value;
    if (perc < thresh1)
	attron(COLOR_PAIR(1));
    else if (perc < thresh2)
	attron(COLOR_PAIR(2));
    else if (perc < thresh3)
	attron(COLOR_PAIR(3));
    else
	attron(COLOR_PAIR(4));
}

void get_terminal_dim_ncurses(int *w, int *h) {

	getmaxyx(stdscr,*h,*w);
	clear(); //clearing in case of resieze
}

int draw_terminal_ncurses(int virt, int h, int w, int bands, int bw, int rest, int f[200], int flastd[200])
{


	int i, n, q;
	const wchar_t* bars[] = {L"\u2581", L"\u2582", L"\u2583", L"\u2584", L"\u2585", L"\u2586", L"\u2587", L"\u2588"};

	// output: check if terminal has been resized
	if (virt != 0) {
		if ( LINES != h || COLS != w) {
			return -1;
		}
	}

	/* clear(); */
	h = h - 1;

	int maxh, maxw;
	getmaxyx(stdscr,maxh,maxw);


	float thresh1 = .1, thresh2 = .2, thresh3 = .3;

	for (i = 0; i <  bands; i++) {

	    if(f[i] > flastd[i]){//higher then last one
		if (virt == 0)
			    for (n = flastd[i] / 8; n < f[i] / 8; n++)
			    {
				color_switch(h-n, maxh, thresh1, thresh2, thresh3);
				for (q = 0; q < bw; q++)
				    mvprintw((h - n), (i * bw) + q + i + rest, "%d",8);
			    }
		else 
		    for (n = flastd[i] / 8; n < f[i] / 8; n++)
		    {
			color_switch(h-n, maxh, thresh1, thresh2, thresh3);
			for (q = 0; q < bw; q++) 
			    mvaddwstr((h - n), (i * bw) + q + i + rest, bars[7]);
		    }
		if (f[i] % 8 != 0) 
		{
		    color_switch(h-n, maxh, thresh1, thresh2, thresh3);
		    if (virt == 0)
			for (q = 0; q < bw; q++)
			    mvprintw( (h - n), (i * bw) + q + i + rest, "%d",(f[i] % 8) );
		    else 
			for (q = 0; q < bw; q++)
			    mvaddwstr( (h - n), (i * bw) + q + i + rest, bars[(f[i] % 8) - 1]);
		}
	    }

//lower then last one

	    else if(f[i] < flastd[i])
	    {
		/* attron(COLOR_PAIR(2)); */

		for (n = f[i] / 8; n < flastd[i]/8 + 1; n++)
		{
		    color_switch(h-n, maxh, thresh1, thresh2, thresh3);
		    for (q = 0; q < bw; q++)
			mvaddstr( (h - n), (i*bw) + q + i + rest, " ");
		}


		if (f[i] % 8 != 0) {
		    color_switch(h-n, maxh, thresh1, thresh2, thresh3);
		    if (virt == 0)
			for (q = 0; q < bw; q++)
			    mvprintw((h - f[i] / 8), (i * bw) + q + i + rest, "%d",(f[i] % 8) );
		    else 
			for (q = 0; q < bw; q++)
			    mvaddwstr((h - f[i] / 8), (i * bw) + q + i + rest, bars[(f[i] % 8) - 1]);
		}
	    }
	    
	    flastd[i] = f[i]; //memmory for falloff func
	}

	refresh();
	return 0;
}


// general: cleanup
void cleanup_terminal_ncurses(void)
{
	echo();
	system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz  >/dev/null 2>&1");
	system("setterm -blank 10");
	endwin();
	system("clear");
}

