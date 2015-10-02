#include <locale.h>
#include <wchar.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

wchar_t barstring[8][100];
int ttybarstring[8];
char spacestring[100];


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

int init_terminal_noncurses(int col, int bgcol, int w, int h, int bw) {

	int n, i;
	//clearing barstrings
	for (n = 0; n < 8; n++) {

		ttybarstring[n] = 0;
		barstring[n][0] ='\0';
		spacestring[0] ='\0';
	}

	//creating barstrings for drawing
	for (n = 0; n < bw; n++) {

		wcscat(barstring[0],L"\u2588");
		wcscat(barstring[1],L"\u2581");
		wcscat(barstring[2],L"\u2582");
		wcscat(barstring[3],L"\u2583");
		wcscat(barstring[4],L"\u2584");
		wcscat(barstring[5],L"\u2585");
		wcscat(barstring[6],L"\u2586");
		wcscat(barstring[7],L"\u2587");
		strcat(spacestring, " ");

		for (i = 0; i < 8; i++) {
			ttybarstring[i] += (i + 1) * pow(10, n);
		}
	}

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


int draw_terminal_noncurses(int virt, int h, int w, int bars, int bw, int bs, int rest, int f[200], int flastd[200]) {
	int c, move, n, o;

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
		for (o = 0; o < bars; o++) {

			// output: draw and move to another one, check whether we're not too far

			if (f[o] != flastd[o]) { //change?

				if (move != 0) printf("\033[%dC", move);
				move = 0;

				c = f[o] - n * 8;

				if (c < 1) {
					if (n * 8 < flastd[o]) printf("%s", spacestring); //blank
					else move += bw;
				} else if (c > 7) {
					if (n > flastd[o] / 8 - 1) {
						if (virt == 0) printf("%d", ttybarstring[7]); //  block tty
						else  printf("%ls", barstring[0]); //block
					} else move += bw;
				} else {
					if (virt == 0) printf("%d", ttybarstring[c - 1]); // fractioned block tty
					else  printf("%ls", barstring[c] ); // fractioned block vt
				}


			} else move += bw; //no change, moving along

			move += bs;//move to next bar
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
	system("setfont  >/dev/null 2>&1");
	system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz  >/dev/null 2>&1");
	system("setterm -cursor on");
	system("setterm -blank 10");
	system("clear");

}
