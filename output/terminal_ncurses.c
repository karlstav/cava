#include <curses.h>
#include <wchar.h>
#include <stdlib.h>

struct colors {
	NCURSES_COLOR_T color;
	NCURSES_COLOR_T R;
	NCURSES_COLOR_T G;
	NCURSES_COLOR_T B;
};

#define COLOR_REDEFINITION -2

#define MAX_COLOR_REDEFINITION 2

static struct colors the_color_redefinitions[MAX_COLOR_REDEFINITION];


static void parse_color(char* color_string, struct colors* color) {
	if (color_string[0] == '#') {
		if (!can_change_color()) {
			cleanup_terminal_ncurses();
			fprintf(stderr, "Your terminal can not change color definitions, "
					"please use one of the predefined colors.\n");
			exit(EXIT_FAILURE);
		}
		color->color = COLOR_REDEFINITION;
		sscanf(++color_string, "%02hx%02hx%02hx", &color->R, &color->G, &color->B);
	}
}

static void remember_color_definition(NCURSES_COLOR_T color_number) {
	int index = color_number - 1; // array starts from zero and colors - not
	if(the_color_redefinitions[index].color == 0) {
		the_color_redefinitions[index].color = color_number;
		color_content(color_number,
			&the_color_redefinitions[index].R,
			&the_color_redefinitions[index].G,
			&the_color_redefinitions[index].B);
	}
}

// ncurses use color range [0, 1000], and we - [0, 255]
#define CURSES_COLOR_COEFFICIENT( X ) (( X ) * 1000.0 / 0xFF + 0.5)
#define COLORS_STRUCT_NORMALIZE( X ) CURSES_COLOR_COEFFICIENT( X.R ), \
	CURSES_COLOR_COEFFICIENT( X.G ), CURSES_COLOR_COEFFICIENT( X.B )

static NCURSES_COLOR_T change_color_definition(NCURSES_COLOR_T color_number,
char* const color_string, NCURSES_COLOR_T predef_color) {
	struct colors color = { 0 };
	parse_color(color_string, &color);
	NCURSES_COLOR_T return_color_number = predef_color;
	if (color.color == COLOR_REDEFINITION) {
		remember_color_definition(color_number);
		init_color(color_number, COLORS_STRUCT_NORMALIZE(color));
		return_color_number = color_number;
	}
	return return_color_number;
}

void init_terminal_ncurses(char* const fg_color_string,
char* const bg_color_string, int predef_fg_color, int predef_bg_color) {
	initscr();
	curs_set(0);
	timeout(0);
	noecho();
	start_color();
	use_default_colors();

	NCURSES_COLOR_T fg_color_number;
	fg_color_number = change_color_definition(1, fg_color_string, predef_fg_color);

	NCURSES_COLOR_T bg_color_number;
	bg_color_number = change_color_definition(2, bg_color_string, predef_bg_color);
	// do not forget to increase MAX_COLOR_REDEFINITION if you change more color
	// definitions

	NCURSES_COLOR_T color_pair_number = 1;
	init_pair(color_pair_number, fg_color_number, bg_color_number);

	if (bg_color_number != -1)
		bkgd(COLOR_PAIR(color_pair_number));
	attron(COLOR_PAIR(color_pair_number));
	refresh();
}

void get_terminal_dim_ncurses(int* width, int* height) {
	getmaxyx(stdscr, *height, *width);
	clear(); // clearing in case of resieze
}

#define TERMINAL_RESIZED -1

int draw_terminal_ncurses(int is_tty, int terminal_height, int terminal_width,
int bars_count, int bar_width, int bar_spacing, int rest, const int f[200],
int flastd[200]) {
	const wchar_t* bar_heights[] = {L"\u2581", L"\u2582", L"\u2583",
		L"\u2584", L"\u2585", L"\u2586", L"\u2587", L"\u2588"};
	#define LAST ((sizeof(bar_heights) / sizeof(bar_heights[0])) - 1)

	// output: check if terminal has been resized
	if (!is_tty) {
		if (LINES != terminal_height || COLS != terminal_width) {
			return TERMINAL_RESIZED;
		}
	}
	const int height = terminal_height - 1;
	#define CURRENT_COLUMN bar*bar_width + width + bar*bar_spacing + rest
	for (int bar = 0; bar < bars_count; bar++) {
		if (f[bar] > flastd[bar]) { // higher then last one
			if (is_tty) {
				for (int n = flastd[bar] / 8; n < f[bar] / 8; n++)
					for (int width = 0; width < bar_width; width++)
						mvprintw((height - n), CURRENT_COLUMN, "%d", 8);
			} else {
				for (int n = flastd[bar] / 8; n < f[bar] / 8; n++)
					for (int width = 0; width < bar_width; width++)
						mvaddwstr((height - n), CURRENT_COLUMN,
								bar_heights[LAST]);
			}
			if (f[bar] % 8) {
				if (is_tty) {
					for (int width = 0; width < bar_width; width++)
						mvprintw((height - f[bar] / 8), CURRENT_COLUMN, "%d",
								(f[bar] % 8));
				} else {
					for (int width = 0; width < bar_width; width++)
						mvaddwstr((height - f[bar] / 8), CURRENT_COLUMN,
								bar_heights[(f[bar] % 8) - 1]);
				}
			}
		} else if(f[bar] < flastd[bar]) { // lower then last one
			for (int n = f[bar] / 8; n < flastd[bar]/8 + 1; n++)
				for (int width = 0; width < bar_width; width++)
					mvaddstr((height - n), CURRENT_COLUMN, " ");
			if (f[bar] % 8) {
				if (is_tty) {
					for (int width = 0; width < bar_width; width++)
						mvprintw((height - f[bar] / 8), CURRENT_COLUMN, "%d",
								(f[bar] % 8));
				} else {
					for (int width = 0; width < bar_width; width++)
						mvaddwstr((height - f[bar] / 8), CURRENT_COLUMN,
								bar_heights[(f[bar] % 8) - 1]);
				}
			}
		}
		#undef CURRENT_COLUMN
		flastd[bar] = f[bar]; // memory for falloff func
	}
	refresh();
	return 0;
}

// general: cleanup
void cleanup_terminal_ncurses(void) {
	echo();
	system("setfont  >/dev/null 2>&1");
	system("setfont /usr/share/consolefonts/Lat2-Fixed16.psf.gz  >/dev/null 2>&1");
	system("setterm -blank 10");
	for(int i = 0; i < MAX_COLOR_REDEFINITION; ++i) {
		if(the_color_redefinitions[i].color) {
			init_color(the_color_redefinitions[i].color,
				the_color_redefinitions[i].R,
				the_color_redefinitions[i].G,
				the_color_redefinitions[i].B);
		}
	}
	endwin();
	system("clear");
}

