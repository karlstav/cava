#ifndef SDL_DASHBOARD_H
#define SDL_DASHBOARD_H

#include <SDL2/SDL.h>
#include <stdbool.h>

bool dashboard_update(SDL_Window *window);
bool dashboard_handle_event(const SDL_Event *event);
SDL_Rect dashboard_spectrum_rect(void);
bool dashboard_has_overlay(void);
void dashboard_draw(SDL_Renderer *renderer);
void dashboard_close(void);

#endif
