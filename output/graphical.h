#ifdef GLX
	#include <GL/gl.h>
	#include <GL/glx.h>
	GLXContext glcontext;
	extern int GLXmode;
#endif

int w, h, windowX, windowY;
unsigned char fs, borderFlag, transparentFlag, keepInBottom;
char *windowAlignment;
