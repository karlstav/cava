#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "sdl_cava.h"
#include <SDL2/SDL.h>

#ifdef __linux__
#include "sdl_dashboard.h"
#endif

#include <stdbool.h>
#include <stdlib.h>

#include "../util.h"

#define MAX_GRADIENT_COLOR_DEFS 8

SDL_Window *gWindow = NULL;

SDL_Renderer *gRenderer = NULL;

static bool redraw_surface;

struct colors {
    uint16_t RGB[3];
};

struct colors *gradient_colors_sdl;

struct colors fg_color = {0};
struct colors bg_color = {0};

static void parse_color(char *color_string, struct colors *color) {
    if (color_string[0] == '#') {
        sscanf(++color_string, "%02hx%02hx%02hx", &color->RGB[0], &color->RGB[1], &color->RGB[2]);
    }
}

// index of the display under the mouse cursor, so the window opens where it was launched from
int sdl_cursor_display(void) {
    int mx, my;
    SDL_GetGlobalMouseState(&mx, &my);
    SDL_Point cursor = {mx, my};
    for (int i = 0; i < SDL_GetNumVideoDisplays(); i++) {
        SDL_Rect bounds;
        if (SDL_GetDisplayBounds(i, &bounds) == 0 && SDL_PointInRect(&cursor, &bounds))
            return i;
    }
    return 0;
}

void init_sdl_window(int width, int height, int x, int y, int full_screen) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    } else {
        int display = sdl_cursor_display();
        if (x == -1)
            x = SDL_WINDOWPOS_CENTERED_DISPLAY(display);

        if (y == -1)
            y = SDL_WINDOWPOS_CENTERED_DISPLAY(display);

        Uint32 sdl_flags = SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;

        if (sdl_load_full_screen(full_screen) == 1)
            sdl_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

        gWindow = SDL_CreateWindow("cava", x, y, width, height, sdl_flags);
        if (gWindow == NULL) {
            printf("Window could not be created! SDL_Error: %s\n", SDL_GetError());
        } else {
            gRenderer = SDL_CreateRenderer(gWindow, -1, SDL_RENDERER_ACCELERATED);
            if (gRenderer == NULL) {
                printf("Renderer could not be created! SDL Error: %s\n", SDL_GetError());
            }
        }
    }
}

void init_sdl_surface(int *w, int *h, char *const fg_color_string, char *const bg_color_string,
                      int gradient, int gradient_count, char **gradient_color_strings) {
    SDL_GetWindowSize(gWindow, w, h);

    parse_color(bg_color_string, &bg_color);
    parse_color(fg_color_string, &fg_color);
    redraw_surface = true;
    free(gradient_colors_sdl);
    gradient_colors_sdl = NULL;
    if (gradient && *h > 0) {
        struct colors gradient_color_defs[MAX_GRADIENT_COLOR_DEFS];
        for (int i = 0; i < gradient_count; i++) {
            parse_color(gradient_color_strings[i], &gradient_color_defs[i]);
        }
        int lines = *h;
        gradient_colors_sdl = malloc(lines * sizeof(*gradient_colors_sdl));
        if (!gradient_colors_sdl)
            return;
        for (int line = 0; line < lines; line++) {
            double position = line * (gradient_count - 1.0) / lines;
            int first = (int)position;
            for (int c = 0; c < 3; c++) {
                int from = gradient_color_defs[first].RGB[c];
                int to = gradient_color_defs[first + 1].RGB[c];
                gradient_colors_sdl[line].RGB[c] = from + (to - from) * (position - first);
            }
        }
        gradient_colors_sdl[lines - 1] = gradient_color_defs[gradient_count - 1];
    }
}

static void draw_spectrum(SDL_Rect area, int width, int height, int bars_count, int bar_width,
                          int bar_spacing, int remainder, const int bars[], const int base[],
                          enum orientation orientation, int gradient) {
    if (area.w <= 0 || area.h <= 0 || width <= 0 || height <= 0)
        return;

    SDL_RenderSetClipRect(gRenderer, &area);
    if (gradient && gradient_colors_sdl) {
        for (int row = 0; row < area.h; row++) {
            int line = row * height / area.h;
            int next_line = (row + 1) * height / area.h;
            struct colors color = gradient_colors_sdl[line];
            SDL_SetRenderDrawColor(gRenderer, color.RGB[0], color.RGB[1], color.RGB[2], 255);
            for (int bar = 0; bar < bars_count; bar++) {
                if (bars[bar] > line && (!base || base[bar] < next_line)) {
                    int x = bar * (bar_width + bar_spacing);
                    SDL_RenderDrawLine(
                        gRenderer, area.x + x * area.w / width, area.y + area.h - 1 - row,
                        area.x + (x + bar_width) * area.w / width, area.y + area.h - 1 - row);
                }
            }
        }
    } else {
        SDL_SetRenderDrawColor(gRenderer, fg_color.RGB[0], fg_color.RGB[1], fg_color.RGB[2], 255);
        bool vertical = orientation == ORIENT_LEFT || orientation == ORIENT_RIGHT;
        int axis = vertical ? area.h : area.w;
        int source_axis = vertical ? height : width;
        int value_axis = vertical ? area.w : area.h;
        int source_value_axis = vertical ? width : height;
        for (int bar = 0; bar < bars_count; bar++) {
            int offset = bar * (bar_width + bar_spacing) + remainder;
            int start = offset * axis / source_axis;
            int thickness = (offset + bar_width) * axis / source_axis - start;
            int base_value = orientation == ORIENT_BOTTOM && base ? base[bar] : 0;
            if (bars[bar] <= base_value)
                continue;
            int floor = base_value * value_axis / source_value_axis;
            int value = bars[bar] * value_axis / source_value_axis;
            if (value == floor)
                value++;
            if (thickness == 0)
                thickness = 1;
            SDL_Rect rect = vertical ? (SDL_Rect){area.x, area.y + start, value, thickness}
                                     : (SDL_Rect){area.x + start, area.y, thickness, value};
            if (orientation == ORIENT_RIGHT)
                rect.x += area.w - value;
            if (orientation == ORIENT_BOTTOM)
                rect.y += area.h - value;
            rect.h -= floor;
            SDL_RenderFillRect(gRenderer, &rect);
        }
    }
    SDL_RenderSetClipRect(gRenderer, NULL);
}

int draw_sdl(int bars_count, int bar_width, int bar_spacing, int remainder, int value_extent,
             const int bars[], int previous_frame[], const int base[], const int previous_base[],
             enum orientation orientation, int gradient) {

    int rc = 0;
    int width, window_height;
    SDL_GetWindowSize(gWindow, &width, &window_height);
    SDL_Rect spectrum = {0, 0, width, window_height};

    bool update = redraw_surface;
    redraw_surface = false;
#ifdef __linux__
    update |= dashboard_update(gWindow);
    spectrum = dashboard_spectrum_rect();
#endif
    for (int bar = 0; spectrum.w > 0 && !update && bar < bars_count; bar++)
        update = bars[bar] != previous_frame[bar] ||
                 (base && previous_base && base[bar] != previous_base[bar]);

    if (update) {
        SDL_SetRenderDrawColor(gRenderer, bg_color.RGB[0], bg_color.RGB[1], bg_color.RGB[2], 255);
        SDL_RenderClear(gRenderer);
        bool horizontal_bars = orientation == ORIENT_LEFT || orientation == ORIENT_RIGHT;
        int spectrum_width = horizontal_bars ? value_extent : width;
        int spectrum_height = horizontal_bars ? window_height : value_extent;
        draw_spectrum(spectrum, spectrum_width, spectrum_height, bars_count, bar_width, bar_spacing,
                      remainder, bars, base, orientation, gradient);
#ifdef __linux__
        dashboard_draw(gRenderer);
#endif
        SDL_RenderPresent(gRenderer);
    }

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
#ifdef __linux__
        if (dashboard_handle_event(&e))
            continue;
#endif
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_EXPOSED)
            redraw_surface = true;
        if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
            rc = -1;
        if (e.type == SDL_KEYDOWN) {
            if (e.key.keysym.sym == SDLK_q || e.key.keysym.sym == SDLK_ESCAPE)
                rc = -2;
            if (e.key.keysym.sym == SDLK_F11 && !e.key.repeat) {
                sdl_toggle_full_screen(gWindow);
                rc = -1;
            }
        }
        if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT &&
            e.button.clicks == 2) {
            sdl_toggle_full_screen(gWindow);
            rc = -1;
        }
        if (e.type == SDL_QUIT)
            rc = -2;
    }

    return rc;
}

// general: cleanup
void cleanup_sdl(void) {
#ifdef __linux__
    dashboard_close();
#endif
    free(gradient_colors_sdl);
    gradient_colors_sdl = NULL;
    SDL_DestroyRenderer(gRenderer);
    gRenderer = NULL;
    SDL_DestroyWindow(gWindow);
    gWindow = NULL;
    SDL_Quit();
}

// general: remembered full screen state, stored as "0" or "1" in SDL's per-user pref dir
static FILE *open_full_screen_state(const char *mode) {
    char *dir = SDL_GetPrefPath("cava", "cava");
    if (dir == NULL)
        return NULL;
    char path[4096];
    snprintf(path, sizeof(path), "%sfullscreen", dir);
    SDL_free(dir);
    return fopen(path, mode);
}

int sdl_load_full_screen(int config_full_screen) {
    FILE *f = open_full_screen_state("r");
    if (f == NULL)
        return config_full_screen;
    int full_screen = config_full_screen;
    if (fscanf(f, "%d", &full_screen) != 1)
        full_screen = config_full_screen;
    fclose(f);
    return full_screen;
}

// general: toggle desktop full screen and remember it for the next start
void sdl_toggle_full_screen(SDL_Window *window) {
    int full_screen = !(SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP);
    SDL_SetWindowFullscreen(window, full_screen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
    FILE *f = open_full_screen_state("w");
    if (f != NULL) {
        fprintf(f, "%d\n", full_screen);
        fclose(f);
    }
}
