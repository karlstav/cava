#include <curses.h>
#include <wchar.h>
#include <stdlib.h>

int gradient_size = 64;

struct colors {
	NCURSES_COLOR_T color;
	NCURSES_COLOR_T R;
	NCURSES_COLOR_T G;
	NCURSES_COLOR_T B;
};

#define COLOR_REDEFINITION -2

#define MAX_COLOR_REDEFINITION 256

//static struct colors the_color_redefinitions[MAX_COLOR_REDEFINITION];


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
/*
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
*/
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
		//remember_color_definition(color_number);
		init_color(color_number, COLORS_STRUCT_NORMALIZE(color));
		return_color_number = color_number;
	}
	return return_color_number;
}


void init_terminal_ncurses(char* const fg_color_string,
char* const bg_color_string, int predef_fg_color, int predef_bg_color, int gradient,
 char* const gradient_color_1, char* const gradient_color_2, int* width, int* height) {
	initscr();
	curs_set(0);
	timeout(0);
	noecho();
	start_color();
	use_default_colors();

    getmaxyx(stdscr, *height, *width);
	clear();

    NCURSES_COLOR_T color_pair_number = 1;

    NCURSES_COLOR_T bg_color_number;
    bg_color_number = change_color_definition(0, bg_color_string, predef_bg_color);

    if (!gradient) {

	    NCURSES_COLOR_T fg_color_number;
        fg_color_number = change_color_definition(1, fg_color_string, predef_fg_color);

        init_pair(color_pair_number, fg_color_number, bg_color_number);

    } else if (gradient) {
        
        short unsigned int rgb[3][3];
        char next_color[8];
        
        gradient_size = *height;

        if (gradient_size > COLORS) gradient_size = COLORS - 1;
	    
        if (gradient_size > COLOR_PAIRS) gradient_size = COLOR_PAIRS - 1;
        
        if (gradient_size > MAX_COLOR_REDEFINITION) gradient_size = MAX_COLOR_REDEFINITION - 1;

        sscanf(gradient_color_1 + 1, "%02hx%02hx%02hx", &rgb[0][0], &rgb[0][1], &rgb[0][2]);
        sscanf(gradient_color_2 + 1, "%02hx%02hx%02hx", &rgb[1][0], &rgb[1][1], &rgb[1][2]);            

        for (int n = 0; n < gradient_size; n++) {

            for(int i = 0; i < 3; i++) {
                rgb[2][i] = rgb[0][i] + (rgb[1][i] - rgb[0][i]) * n / (gradient_size * 0.85);
                if (rgb[2][i] > 255) rgb[2][i] = 0;
                if ( n > gradient_size * 0.85 ) rgb[2][i] = rgb[1][i];
                }
            
            sprintf(next_color,"#%02x%02x%02x",rgb[2][0], rgb[2][1], rgb[2][2]);

            change_color_definition(n + 1, next_color, n + 1);
            init_pair(color_pair_number++, n + 1, bg_color_number);

        }

    }

	if (bg_color_number != -1)
		bkgd(COLOR_PAIR(color_pair_number));
	attron(COLOR_PAIR(color_pair_number));
	refresh();

}

void change_colors(int cur_height, int tot_height) {
    tot_height /= gradient_size ;
    if (tot_height < 1) tot_height = 1;
    cur_height /= tot_height;
    if (cur_height > gradient_size  - 1) cur_height = gradient_size - 1;
    attron(COLOR_PAIR(cur_height + 1));
}

void get_terminal_dim_ncurses(int* width, int* height) {
	getmaxyx(stdscr, *height, *width);
    gradient_size = *height;
	clear(); // clearing in case of resieze
}

#define TERMINAL_RESIZED -1

int draw_terminal_ncurses(int is_tty, int terminal_height, int terminal_width,
int bars_count, int bar_width, int bar_spacing, int rest, const int f[200],
int flastd[200], int gradient) {

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
		if (f[bar] > flastd[bar]) { // higher then last frame
			if (is_tty) {
				for (int n = flastd[bar] / 8; n < f[bar] / 8; n++) {
                    if (gradient) change_colors(n, height); 
					for (int width = 0; width < bar_width; width++)
						mvprintw((height - n), CURRENT_COLUMN, "%d", 8);
                }
			} else {
				for (int n = flastd[bar] / 8; n < f[bar] / 8; n++) {                                 
                    if (gradient) change_colors(n, height); 
					for (int width = 0; width < bar_width; width++)
						mvaddwstr((height - n), CURRENT_COLUMN,
								bar_heights[LAST]);
                }
			}
            if (gradient) change_colors(f[bar] / 8, height); 
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
		} else if(f[bar] < flastd[bar]) { // lower then last frame
			for (int n = f[bar] / 8; n < flastd[bar]/8 + 1; n++)
				for (int width = 0; width < bar_width; width++)
					mvaddstr((height - n), CURRENT_COLUMN, " ");
			if (f[bar] % 8) {
                 if (gradient) change_colors(f[bar] / 8, height); 
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
	/*for(int i = 0; i < gradient_size; ++i) {
		if(the_color_redefinitions[i].color) {
			init_color(the_color_redefinitions[i].color,
				the_color_redefinitions[i].R,
				the_color_redefinitions[i].G,
				the_color_redefinitions[i].B);
		}
	}
  */
	endwin();
	system("clear");
    system("reset");
}

