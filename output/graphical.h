#if defined(GLX)
	#define GL
#endif

#ifndef H_GRAPHICAL
#define H_GRAPHICAL

#ifdef GLX
	#include <GL/glx.h>
	GLXContext cavaGLXContext;
	GLXFBConfig* cavaFBConfig;
	extern int GLXmode;
#endif

#ifdef GL
	#include <GL/gl.h>
	int drawGLBars(int rest, int bw, int bs, int bars, int h, int shadow, int gradient, float colors[11], int *f);
#endif

int windowX, windowY;
unsigned char fs, borderFlag, transparentFlag, keepInBottom, interactable;
char *windowAlignment;

void calculate_win_pos(int *winX, int *winY, int winW, int winH, int scrW, int scrH, char *winPos);

#endif
