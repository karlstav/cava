#ifdef GLX
	#include <GL/gl.h>
	#include <GL/glx.h>
	GLXContext cavaGLXContext;
	GLXFBConfig* cavaFBConfig;
	extern int GLXmode;
#endif

int windowX, windowY;
unsigned char fs, borderFlag, transparentFlag, keepInBottom;
char *windowAlignment;

void calculate_win_pos(int *winX, int *winY, int winW, int winH, int scrW, int scrH, char *winPos);
