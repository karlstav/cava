#ifndef H_GRAPHICAL
#define H_GRAPHICAL

#ifdef GLX
	#include <GL/gl.h>
	#include <GL/glx.h>
	GLXContext cavaGLXContext;
	GLXFBConfig* cavaFBConfig;
	extern int GLXmode;
	typedef struct {
		GLfloat x, y, r, g, b, a;
	} GLVertex;

	int drawGLBars(GLVertex *glVertex, int rest, int bw, int bs, int bars, int h, int shadow, int gradient, float colors[11], int *f);
#endif

int windowX, windowY;
unsigned char fs, borderFlag, transparentFlag, keepInBottom;
char *windowAlignment;

void calculate_win_pos(int *winX, int *winY, int winW, int winH, int scrW, int scrH, char *winPos);

#endif
