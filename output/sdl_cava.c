#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS 1
#endif

#include "sdl_cava.h"
#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stdlib.h>

#include "../util.h"

#define MAX_GRADIENT_COLOR_DEFS 8

SDL_Window *gWindow = NULL;

SDL_Renderer *gRenderer = NULL;

SDL_Event e;

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
    SDL_SetRenderDrawColor(gRenderer, fg_color.RGB[0], fg_color.RGB[1], fg_color.RGB[2], 0xFF);
    SDL_RenderClear(gRenderer);

    parse_color(fg_color_string, &fg_color);
    SDL_SetRenderDrawColor(gRenderer, fg_color.RGB[0], fg_color.RGB[1], fg_color.RGB[2], 0xFF);
    if (gradient) {
        struct colors gradient_color_defs[MAX_GRADIENT_COLOR_DEFS];
        for (int i = 0; i < gradient_count; i++) {
            parse_color(gradient_color_strings[i], &gradient_color_defs[i]);
        }
        int lines = *h;
        gradient_colors_sdl = (struct colors *)malloc((lines * 2) * sizeof(struct colors));

        int individual_size = lines / (gradient_count - 1);

        float rest = lines / (float)(gradient_count - 1) - individual_size;

        float rest_total = 0;
        int gradient_lines = 0;

        for (int i = 0; i < gradient_count - 1; i++) {
            individual_size = lines / (gradient_count - 1);
            if (rest_total > 1.0) {
                individual_size++;
                rest_total = rest_total - 1.0;
            }
            for (int n = 0; n < individual_size; n++) {
                for (int c = 0; c < 3; c++) {
                    float next_color =
                        gradient_color_defs[i + 1].RGB[c] - gradient_color_defs[i].RGB[c];
                    next_color *= n / (float)individual_size;
                    gradient_colors_sdl[gradient_lines].RGB[c] =
                        gradient_color_defs[i].RGB[c] + next_color;
                }
                gradient_lines++;
            }
            rest_total = rest_total + rest;
        }
        gradient_colors_sdl[lines - 1] = gradient_color_defs[gradient_count - 1];
    }
}

int draw_sdl(int bars_count, int bar_width, int bar_spacing, int remainder, int height,
             const int bars[], int previous_frame[], const int base[], const int previous_base[],
             int frame_time, enum orientation orientation, int gradient) {

    bool update = false;
    int rc = 0;
    SDL_Rect fillRect;

    for (int bar = 0; bar < bars_count; bar++) {
        if (bars[bar] != previous_frame[bar] ||
            (base && previous_base && base[bar] != previous_base[bar])) {
            update = true;
            break;
        }
    }

    if (update) {
        SDL_SetRenderDrawColor(gRenderer, bg_color.RGB[0], bg_color.RGB[1], bg_color.RGB[2], 0xFF);
        SDL_RenderClear(gRenderer);
        if (gradient) {
            for (int line = 0; line < height; line++) {

                SDL_SetRenderDrawColor(gRenderer, gradient_colors_sdl[line].RGB[0],
                                       gradient_colors_sdl[line].RGB[1],
                                       gradient_colors_sdl[line].RGB[2], 0xFF);
                for (int bar = 0; bar < bars_count; bar++) {
                    if (bars[bar] > line && (!base || base[bar] <= line)) {
                        int x1 = bar * (bar_width + bar_spacing);
                        if (bar == 0)
                            x1 = 0;
                        int x2 = x1 + bar_width;
                        SDL_RenderDrawLine(gRenderer, x1, height - line, x2, height - line);
                    }
                }
            }
        } else {
            for (int bar = 0; bar < bars_count; bar++) {
                switch (orientation) {
                case ORIENT_LEFT:
                    fillRect.x = 0;
                    fillRect.y = bar * (bar_width + bar_spacing) + remainder;
                    fillRect.w = bars[bar];
                    fillRect.h = bar_width;
                    break;
                case ORIENT_RIGHT:
                    fillRect.x = height - bars[bar];
                    fillRect.y = bar * (bar_width + bar_spacing) + remainder;
                    fillRect.w = bars[bar];
                    fillRect.h = bar_width;
                    break;
                case ORIENT_TOP:
                    fillRect.x = bar * (bar_width + bar_spacing) + remainder;
                    fillRect.y = 0;
                    fillRect.w = bar_width;
                    fillRect.h = bars[bar];
                    break;
                default:
                    fillRect.x = bar * (bar_width + bar_spacing) + remainder;
                    fillRect.y = height - bars[bar];
                    fillRect.w = bar_width;
                    fillRect.h = bars[bar] - (base ? base[bar] : 0);
                    break;
                }

                SDL_SetRenderDrawColor(gRenderer, fg_color.RGB[0], fg_color.RGB[1], fg_color.RGB[2],
                                       0xFF);
                SDL_RenderFillRect(gRenderer, &fillRect);
            }
        }
        SDL_RenderPresent(gRenderer);
    }

    SDL_Delay(frame_time);

    while (SDL_PollEvent(&e)) {
        if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
            rc = -1;
            if (gradient) {
                free(gradient_colors_sdl);
            }
        }
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
    SDL_DestroyWindow(gWindow);
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
