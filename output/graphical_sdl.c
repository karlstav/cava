#include <SDL2/SDL.h>
#include "output/graphical.h"

SDL_Window *cavaSDLWindow;
SDL_Surface *cavaSDLWindowSurface;
SDL_Event cavaSDLEvent;
SDL_DisplayMode cavaSDLVInfo;
int gradCol[2];

void cleanup_graphical_sdl(void)
{
	SDL_FreeSurface(cavaSDLWindowSurface);
	SDL_DestroyWindow(cavaSDLWindow);
	SDL_Quit();
}

int init_window_sdl(int *col, int *bgcol, char *color, char *bcolor, int gradient, char *gradient_color_1, char *gradient_color_2)
{
	
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0)
	{
		fprintf(stderr, "unable to initilize SDL2: %s\n", SDL_GetError());
		return 1;
	}
	// calculating window x and y position
	if(SDL_GetCurrentDisplayMode(0, &cavaSDLVInfo)){
		fprintf(stderr, "Error opening display! %s\n", SDL_GetError());
		return 1;
	}
	if(!strcmp(windowAlignment, "top")){
		windowX = (cavaSDLVInfo.w - w) / 2 + windowX;
	}else if(!strcmp(windowAlignment, "bottom")){
		windowX = (cavaSDLVInfo.w - w) / 2 + windowX;
		windowY = (cavaSDLVInfo.h - h) + (-1*windowY);
	}else if(!strcmp(windowAlignment, "top_left")){
		// Nothing to do here :P
	}else if(!strcmp(windowAlignment, "top_right")){
		windowX = (cavaSDLVInfo.w - w) + (-1*windowX);
	}else if(!strcmp(windowAlignment, "left")){
		windowY = (cavaSDLVInfo.h - h) / 2;
	}else if(!strcmp(windowAlignment, "right")){
		windowX = (cavaSDLVInfo.w - w) + (-1*windowX);
		windowY = (cavaSDLVInfo.h - h) / 2 + windowY;
	}else if(!strcmp(windowAlignment, "bottom_left")){
		windowY = (cavaSDLVInfo.h - h) + (-1*windowY);
	}else if(!strcmp(windowAlignment, "bottom_right")){
		windowX = (cavaSDLVInfo.w - w) + (-1*windowX);
		windowY = (cavaSDLVInfo.h - h) + (-1*windowY);
	}else if(!strcmp(windowAlignment, "center")){
		windowX = (cavaSDLVInfo.w - w) / 2 + windowX;
		windowY = (cavaSDLVInfo.h - h) / 2 + windowY;
	}
	// creating a window
	Uint32 windowFlags = SDL_WINDOW_RESIZABLE;
	if(fs) windowFlags |= SDL_WINDOW_FULLSCREEN;
	if(!borderFlag) windowFlags |= SDL_WINDOW_BORDERLESS;
	cavaSDLWindow = SDL_CreateWindow("CAVA", windowX, windowY, w, h, windowFlags);
	if(!cavaSDLWindow)
	{
		fprintf(stderr, "SDL window cannot be created: %s\n", SDL_GetError());
		SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "ERROR", "cannot create SDL window", NULL);
		SDL_Quit();
		return 1;
	}
	if(color[0] != '#')
	{
		switch((*col))
		{
			case 0:
				(*col) = 0x000000;
				break;
			case 1:
				(*col) = 0xFF0000;
				break;
			case 2:
				(*col) = 0x00FF00;
				break;
			case 3:
				(*col) = 0xFFFF00;
				break;
			case 4:
				(*col) = 0x0000FF;
				break;
			case 5:
				(*col) = 0xFF00FF;
				break;
			case 6:
				(*col) = 0x00FFFF;
				break;
			case 7:
				(*col) = 0xFFFFFF;
				break;
		}
	}
	else
		sscanf(color+1, "%x", col);
	if(bcolor[0] != '#')
	{
		switch((*bgcol))
		{
			case 0:
				(*bgcol) = 0x000000;
				break;
			case 1:
				(*bgcol) = 0xFF0000;
				break;
			case 2:
				(*bgcol) = 0x00FF00;
				break;
			case 3:
				(*bgcol) = 0xFFFF00;
				break;
			case 4:
				(*bgcol) = 0x0000FF;
				break;
			case 5:
				(*bgcol) = 0xFF00FF;
				break;
			case 6:
				(*bgcol) = 0x00FFFF;
				break;
			case 7:
				(*bgcol) = 0xFFFFFF;
				break;
		}
	}
	else
		sscanf(bcolor, "%x", bgcol);

	if(gradient)
	{
		sscanf(gradient_color_1 + 1, "%x", &gradCol[0]);
		sscanf(gradient_color_2 + 1, "%x", &gradCol[1]);
	}

	return 0;
}

void apply_window_settings_sdl(int bgcol)
{
	// toggle fullscreen
	SDL_SetWindowFullscreen(cavaSDLWindow, SDL_WINDOW_FULLSCREEN & fs);

	cavaSDLWindowSurface = SDL_GetWindowSurface(cavaSDLWindow);
	// Appearently SDL uses multithreading so this avoids invalid access
	// If I had a job, here's what I would be fired for xD
	SDL_Delay(100);
	SDL_FillRect(cavaSDLWindowSurface, NULL, SDL_MapRGB(cavaSDLWindowSurface->format, bgcol / 0x10000 % 0x100, bgcol / 0x100 % 0x100, bgcol % 0x100));
	return;
}

int get_window_input_sdl(int *bs, int *bw, double *sens, int *mode, int modes, int *col, int *bgcol, int *w, int *h, int gradient)
{
	while(SDL_PollEvent(&cavaSDLEvent) != 0)
	{
		switch(cavaSDLEvent.type)
		{
			case SDL_KEYDOWN:
				switch(cavaSDLEvent.key.keysym.sym)
				{
					// should_reload = 1
					// resizeTerminal = 2
					// bail = -1
					case SDLK_a:
						(*bs)++;
						return 2;
					case SDLK_s:
						if((*bs) > 0) (*bs)--;
						return 2;
					case SDLK_ESCAPE:
						return -1;
					case SDLK_f: // fullscreen
						fs = !fs;
						return 2;
					case SDLK_UP: // key up
						(*sens) *= 1.05;
						break;
					case SDLK_DOWN: // key down
						(*sens) *= 0.95;
						break;
					case SDLK_LEFT: // key left
						(*bw)++;
						return 2;
					case SDLK_RIGHT: // key right
						if((*bw) > 1) (*bw)--;
						return 2;
					case SDLK_m:
						if((*mode) == modes){
							(*mode) = 1;
						} else {
							(*mode)++;
						}
						break;
					case SDLK_r: // reload config
						return 1;
					case SDLK_c: // change foreground color
						if(gradient) break;
						srand(time(NULL));
						(*col) = rand() % 0x100;
						(*col) = (*col) << 16;
						(*col) += rand();
						return 2;
					case SDLK_b: // change background color
						srand(time(NULL));
						(*bgcol) = rand() % 0x100;
						(*bgcol) = (*bgcol) << 16;
						(*bgcol) += rand();
						return 2;
					case SDLK_q:
						return -1;
				}
				break;
			case SDL_WINDOWEVENT:
				if(cavaSDLEvent.window.event == SDL_WINDOWEVENT_CLOSE) return -1;
				else if(cavaSDLEvent.window.event == SDL_WINDOWEVENT_RESIZED){
					// if the user resized the window
					(*w) = cavaSDLEvent.window.data1;
					(*h) = cavaSDLEvent.window.data2;
					return 2;
				}
				break;
		}
	}
	return 0;
}
 
void draw_graphical_sdl(int bars, int rest, int bw, int bs, int *f, int *flastd, int col, int bgcol, int gradient)
{
	for(int i = 0; i < bars; i++)
	{
		SDL_Rect current_bar;
		if(f[i] > flastd[i]){ 
			if(!gradient)
			{
				current_bar = (SDL_Rect) {rest + i*(bs+bw), h - f[i], bw, f[i] - flastd[i]};
				SDL_FillRect(cavaSDLWindowSurface, &current_bar, SDL_MapRGB(cavaSDLWindowSurface->format, col / 0x10000 % 0x100, col / 0x100 % 0x100, col % 0x100));
			}
			else
			{
				for(int I = flastd[i]; I < f[i]; I++)
				{
					Uint32 color = 0xFF;
					double step = (double)I/(double)h;
					color = color << 8;
					if(gradCol[0] / 0x10000 % 0x100 < gradCol[1] / 0x10000 % 0x100)
						color += step*(gradCol[1] / 0x10000 % 0x100 - gradCol[0] / 0x10000 % 0x100) + gradCol[0] / 0x10000 % 0x100;
					else
						color += -1*step*(gradCol[0] / 0x10000 % 0x100 - gradCol[1] / 0x10000 % 0x100) + gradCol[0] / 0x10000 % 0x100;
					color = color << 8;
					if(gradCol[0] / 0x100 % 0x100 < gradCol[1] / 0x100 % 0x100)
						color += step*(gradCol[1] / 0x100 % 0x100 - gradCol[0] / 0x100 % 0x100) + gradCol[0] / 0x100 % 0x100;
					else
						color += -1*step*(gradCol[0] / 0x100 % 0x100 - gradCol[1] / 0x100 % 0x100) + gradCol[0] / 0x100 % 0x100;
					color = color << 8;
					if(gradCol[0] % 0x100 < gradCol[1] % 0x100)
						color += step*(gradCol[1] % 0x100 - gradCol[0] % 0x100) + gradCol[0] % 0x100;
					else
						color += -1*step*(gradCol[0] % 0x100 - gradCol[1] % 0x100) + gradCol[0] % 0x100;
					current_bar = (SDL_Rect) {rest + i*(bs+bw), h - I, bw, 1};
					SDL_FillRect(cavaSDLWindowSurface, &current_bar, SDL_MapRGB(cavaSDLWindowSurface->format, color / 0x10000 % 0x100, color / 0x100 % 0x100, color % 0x100));
				}
			}
		} else if(f[i] < flastd[i]) {
			current_bar = (SDL_Rect) {rest + i*(bs+bw), h - flastd[i], bw, flastd[i] - f[i]};
			SDL_FillRect(cavaSDLWindowSurface, &current_bar, SDL_MapRGB(cavaSDLWindowSurface->format, bgcol / 0x10000 % 0x100, bgcol / 0x100 % 0x100, bgcol % 0x100));
		}
	}
	SDL_UpdateWindowSurface(cavaSDLWindow);
	return;
}
