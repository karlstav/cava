#include "output/sdl_cava.h"

#include <SDL2/SDL.h>

#include <stdbool.h>
#include <stdlib.h>

#include "util.h"

SDL_Window *gWindow = NULL;

SDL_Renderer *gRenderer = NULL;

SDL_Event e;

struct colors {
    uint16_t R;
    uint16_t G;
    uint16_t B;
};

struct colors fg_color = {0};
struct colors bg_color = {0};

static void parse_color(char *color_string, struct colors *color) {
    if (color_string[0] == '#') {
        sscanf(++color_string, "%02hx%02hx%02hx", &color->R, &color->G, &color->B);
    }
}

void init_sdl_window(int width, int height, int x, int y) {
    if (x == -1)
        x = SDL_WINDOWPOS_UNDEFINED;

    if (y == -1)
        y = SDL_WINDOWPOS_UNDEFINED;

    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        printf("SDL could not initialize! SDL_Error: %s\n", SDL_GetError());
    } else {
        gWindow =
            SDL_CreateWindow("cava", x, y, width, height, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
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

void init_sdl_surface(int *w, int *h, char *const fg_color_string, char *const bg_color_string) {
    SDL_GetWindowSize(gWindow, w, h);

    parse_color(bg_color_string, &bg_color);
    SDL_SetRenderDrawColor(gRenderer, bg_color.R, bg_color.G, bg_color.B, 0xFF);
    SDL_RenderClear(gRenderer);

    parse_color(fg_color_string, &fg_color);
    SDL_SetRenderDrawColor(gRenderer, fg_color.R, fg_color.G, fg_color.B, 0xFF);
}

int draw_sdl(int bars_count, int bar_width, int bar_spacing, int remainder, int height,
             const int bars[], int previous_frame[], int frame_time) {

    bool update = false;
    int rc = 0;
    SDL_Rect fillRect;

    for (int bar = 0; bar < bars_count; bar++) {
        if (bars[bar] != previous_frame[bar]) {
            update = true;
            break;
        }
    }

    if (update) {
        SDL_SetRenderDrawColor(gRenderer, bg_color.R, bg_color.G, bg_color.B, 0xFF);
        SDL_RenderClear(gRenderer);
        for (int bar = 0; bar < bars_count; bar++) {
            fillRect.x = bar * (bar_width + bar_spacing) + remainder;
            fillRect.y = height - bars[bar];
            fillRect.w = bar_width;
            fillRect.h = bars[bar];
            SDL_SetRenderDrawColor(gRenderer, fg_color.R, fg_color.G, fg_color.B, 0xFF);
            SDL_RenderFillRect(gRenderer, &fillRect);
        }
        SDL_RenderPresent(gRenderer);
    }

    SDL_Delay(frame_time);

    SDL_PollEvent(&e);
    if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
        rc = -1;
    if (e.type == SDL_QUIT)
        rc = -2;

    return rc;
}

// general: cleanup
void cleanup_sdl(void) {
    SDL_DestroyWindow(gWindow);
    SDL_Quit();
}
