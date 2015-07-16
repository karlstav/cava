#include <locale.h>
#include <wchar.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>
 
int setecho(int fd, int onoff) {

	struct termios t;
 
	if (tcgetattr(fd, &t) == -1)
		return -1;
 
	if (onoff == 0)
		t.c_lflag &= ~(ECHO | ECHOE | ECHOK | ECHONL);
	else
		t.c_lflag |= (ECHO | ECHOE | ECHOK | ECHONL);
 
	if (tcsetattr(fd, TCSANOW, &t) == -1)
		return -1;

	return 0;
}

int init_terminal_noncurses(int col, int bgcol, int w, int h) {

	int n, i;

	col += 30;
	bgcol += 40;

	system("setterm -cursor off");
	system("setterm -blank 0");
	

	// output: reset console
	printf("\033[0m\n");
	system("clear");

	printf("\033[%dm", col); //setting color
	

	printf("\033[1m"); //setting "bright" color mode, looks cooler... I think

	if (bgcol != 0)
		printf("\033[%dm", bgcol);
	{
		for (n = (h); n >= 0; n--) {
			for (i = 0; i < w; i++) {

				printf(" "); //setting backround color

			}
			printf("\n");
		}
		printf("\033[%dA", h); //moving cursor back up
	}
		
	setecho(STDIN_FILENO, 0);


	return 0;
}


void get_terminal_dim_noncurses(int *w, int *h) {

	struct winsize dim;

	ioctl(STDOUT_FILENO, TIOCGWINSZ, &dim);

        *h = (int)dim.ws_row;
	*w = (int)dim.ws_col;

	system("clear"); //clearing in case of resieze
}


int draw_terminal_noncurses(int virt, int h, int w, int bands, int bw, int rest, int f[200], int flastd[200]) {
	int c, move, i, n, o;
	struct winsize dim;


	// output: check if terminal has been resized
	if (virt != 0) {

		ioctl(STDOUT_FILENO, TIOCGWINSZ, &dim);

		if ( (int)dim.ws_row != h || (int)dim.ws_col != w) {
			return -1;					
		} 
	}

	for (n = h - 2; n >= 0; n--) {

		move = rest; //center adjustment
		for (o = 0; o < bands; o++) {

			// output: draw and move to another one, check whether we're not too far

			if (f[o] != flastd[o]) { //change?
				if (move != 0)printf("\033[%dC", move);
				move = 0;
				c = f[o] - n * 8;

				for (i = 0; i < bw; i++) {
					if (c < 1) {
						printf(" "); //blank
					} else if (c > 7) {
						if (virt == 0) printf("%d", 8); //  block tty
						else printf("\u2588"); //block
					} else { 
						if (virt == 0) printf("%d", c); // fractioned block tty
						else printf("%lc", L'\u2580' + c); // fractioned block vt
					}
				}
				

			} else move += bw; //no change, moving along
		
			move++;//move to next bar
		}

		printf("\n");

	}

	printf("\033[%dA", h);
	return 0;
}

// general: cleanup
void cleanup_terminal_noncurses(void)
{
	setecho(STDIN_FILENO, 1);
	printf("\033[0m\n");
	system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz  >/dev/null 2>&1");
	system("setterm -cursor on");
	system("setterm -blank 10");
	system("clear");

}
