#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/XKBlib.h>
#include <X11/extensions/shape.h>
#include <X11/extensions/Xfixes.h>
#include "output/graphical.h"


Pixmap gradientBox = 0;
XColor xbgcol, xcol, xgrad[3];

XEvent cavaXEvent;
Colormap cavaXColormap;
Display *cavaXDisplay;
Screen *cavaXScreen;
Window cavaXWindow, cavaXRoot;
GC cavaXGraphics;

XVisualInfo cavaVInfo;
XSetWindowAttributes cavaAttr;
Atom wm_delete_window, wmState, fullScreen, mwmHintsProperty, wmStateBelow;
XClassHint cavaXClassHint;
XWMHints cavaXWMHints;
XEvent xev;


// mwmHints helps us comunicate with the window manager
struct mwmHints {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
};

int cavaXScreenNumber, shadow, shadow_color, GLXmode;
static unsigned int defaultColors[8] = {0x00000000,0x00FF0000,0x0000FF00,0x00FFFF00,0x000000FF,0x00FF00FF,0x0000FFFF,0x00FFFFFF};


// Some window manager definitions
enum {
    MWM_HINTS_FUNCTIONS = (1L << 0),
    MWM_HINTS_DECORATIONS =  (1L << 1),

    MWM_FUNC_ALL = (1L << 0),
    MWM_FUNC_RESIZE = (1L << 1),
    MWM_FUNC_MOVE = (1L << 2),
    MWM_FUNC_MINIMIZE = (1L << 3),
    MWM_FUNC_MAXIMIZE = (1L << 4),
    MWM_FUNC_CLOSE = (1L << 5)
};
#define _NET_WM_STATE_REMOVE 0
#define _NET_WM_STATE_ADD 1
#define _NET_WM_STATE_TOGGLE 2


#ifdef GLX
int XGLInit() {
	// we will use the existing VisualInfo for this, because I'm not messing around with FBConfigs
	cavaGLXContext = glXCreateContext(cavaXDisplay, &cavaVInfo, NULL, TRUE);
	glXMakeCurrent(cavaXDisplay, cavaXWindow, cavaGLXContext);
	if(transparentFlag) {
		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	}

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	return 0;
}
#endif

void calculateColors(char *color, char *bcolor, int bgcol, int col) {
	char tempColorStr[8];
	
	// Generate a sum of colors
	if(!strcmp(color, "default")) {
		unsigned long redSum = 0, greenSum = 0, blueSum = 0;
		XColor tempColor;
		int xPrecision = 20, yPrecision = 20;	
		while(1){ if(((double)cavaXScreen->width / (double)xPrecision) == ((int)cavaXScreen->width / (int)xPrecision)) break; else xPrecision++; }
		while(1){ if(((double)cavaXScreen->height / (double)yPrecision) == ((int)cavaXScreen->height / (int)yPrecision)) break; else yPrecision++; }
		
		// we need a source for that
		XImage *background = XGetImage(cavaXDisplay, cavaXRoot, 0, 0, cavaXScreen->width, cavaXScreen->height, AllPlanes, XYPixmap);
		for(unsigned short i = 0; i < cavaXScreen->width; i+=(cavaXScreen->width / xPrecision)) {
			for(unsigned short I = 0; I < cavaXScreen->height; I+=(cavaXScreen->height / yPrecision)) {
				// we validate each and EVERY pixel value,
				// because Xorg says so..... and is really slow, so we make compromises
				tempColor.pixel = XGetPixel(background, i, I);
				XQueryColor(cavaXDisplay, cavaXColormap, &tempColor);	
				
				redSum += tempColor.red/256;
				greenSum += tempColor.green/256;
				blueSum += tempColor.blue/256;
			}
		}
		redSum /= xPrecision*yPrecision;
		greenSum /= xPrecision*yPrecision;
		blueSum /= xPrecision*yPrecision;

		XDestroyImage(background);
		sprintf(tempColorStr, "#%02hhx%02hhx%02hhx", (unsigned char)redSum, (unsigned char)greenSum, (unsigned char)blueSum);
	} else if(color[0] != '#') 
		sprintf(tempColorStr, "#%02hhx%02hhx%02hhx", (unsigned char)(defaultColors[col]>>16)%256, (unsigned char)(defaultColors[col]>>8)%256, (unsigned char)defaultColors[col]);
	
	XParseColor(cavaXDisplay, cavaXColormap, color[0]=='#' ? color : tempColorStr, &xcol);
	XAllocColor(cavaXDisplay, cavaXColormap, &xcol);

	
	if(bcolor[0] != '#')
		sprintf(tempColorStr, "#%02hhx%02hhx%02hhx", (unsigned char)defaultColors[bgcol]>>16, (unsigned char)defaultColors[bgcol]>>8, (unsigned char)defaultColors[bgcol]);
	
	XParseColor(cavaXDisplay, cavaXColormap, bcolor[0]=='#' ? bcolor : tempColorStr, &xbgcol);
	XAllocColor(cavaXDisplay, cavaXColormap, &xbgcol);
}

int init_window_x(char *color, char *bcolor, int col, int bgcol, int set_win_props, char **argv, int argc, int gradient, char *gradient_color_1, char *gradient_color_2, unsigned int shdw, unsigned int shdw_col, int w, int h)
{
	// Pass the shadow values
	shadow = shdw;
	shadow_color = shdw_col;
	
	// connect to the X server
	cavaXDisplay = XOpenDisplay(NULL);
	if(cavaXDisplay == NULL) {
		fprintf(stderr, "cannot open X display\n");
		return 1;
	}
	
	cavaXScreen = DefaultScreenOfDisplay(cavaXDisplay);
	cavaXScreenNumber = DefaultScreen(cavaXDisplay);
	cavaXRoot = RootWindow(cavaXDisplay, cavaXScreenNumber);
	
	calculate_win_pos(&windowX, &windowY, w, h, cavaXScreen->width, cavaXScreen->height, windowAlignment);
	
	// 32 bit color means alpha channel support
	XMatchVisualInfo(cavaXDisplay, cavaXScreenNumber, transparentFlag ? 32 : 24, TrueColor, &cavaVInfo);
		cavaAttr.colormap = XCreateColormap(cavaXDisplay, DefaultRootWindow(cavaXDisplay), cavaVInfo.visual, AllocNone);
		cavaXColormap = cavaAttr.colormap; 
		calculateColors(color, bcolor, bgcol, col);
		cavaAttr.background_pixel = transparentFlag ? 0 : xbgcol.pixel;
		cavaAttr.border_pixel = xcol.pixel;
	
	cavaXWindow = XCreateWindow(cavaXDisplay, cavaXRoot, windowX, windowY, w, h, 0, cavaVInfo.depth, InputOutput, cavaVInfo.visual, CWEventMask | CWColormap | CWBorderPixel | CWBackPixel, &cavaAttr);	
	XStoreName(cavaXDisplay, cavaXWindow, "CAVA");

	// The "X" button is handled by the window manager and not Xorg, so we set up a Atom
	wm_delete_window = XInternAtom (cavaXDisplay, "WM_DELETE_WINDOW", FALSE);
	XSetWMProtocols(cavaXDisplay, cavaXWindow, &wm_delete_window, 1);
	
	if(set_win_props) {
		cavaXWMHints.flags = InputHint | StateHint;
		cavaXWMHints.initial_state = NormalState;
		cavaXClassHint.res_name = (char *)"Cava";
		XmbSetWMProperties(cavaXDisplay, cavaXWindow, NULL, NULL, argv, argc, NULL, &cavaXWMHints, &cavaXClassHint);
	}

	XSelectInput(cavaXDisplay, cavaXWindow, VisibilityChangeMask | StructureNotifyMask | ExposureMask | KeyPressMask | KeymapNotify);
	
	#ifdef GLX
		if(GLXmode) if(XGLInit()) return 1;
	#endif
	
	XMapWindow(cavaXDisplay, cavaXWindow);
	cavaXGraphics = XCreateGC(cavaXDisplay, cavaXWindow, 0, 0);
	
	if(gradient) {
		XParseColor(cavaXDisplay, cavaXColormap, gradient_color_1, &xgrad[2]);
		XAllocColor(cavaXDisplay, cavaXColormap, &xgrad[2]);
		XParseColor(cavaXDisplay, cavaXColormap, gradient_color_1, &xgrad[0]);
		XAllocColor(cavaXDisplay, cavaXColormap, &xgrad[0]);
		XParseColor(cavaXDisplay, cavaXColormap, gradient_color_2, &xgrad[1]);
		XAllocColor(cavaXDisplay, cavaXColormap, &xgrad[1]);
	}
	
	// Set up atoms
	wmState = XInternAtom(cavaXDisplay, "_NET_WM_STATE", FALSE);
	fullScreen = XInternAtom(cavaXDisplay, "_NET_WM_STATE_FULLSCREEN", FALSE);
	mwmHintsProperty = XInternAtom(cavaXDisplay, "_MOTIF_WM_HINTS", FALSE);
	wmStateBelow = XInternAtom(cavaXDisplay, "_NET_WM_STATE_BELOW", TRUE);

	if(keepInBottom){
	/**
		window  = the respective client window
		message_type = _NET_WM_STATE
		format = 32
		data.l[0] = the action, as listed below
		data.l[1] = first property to alter
		data.l[2] = second property to alter
		data.l[3] = source indication (0-unk,1-normal app,2-pager)
		other data.l[] elements = 0
	**/
		xev.xclient.type = ClientMessage;
		xev.xclient.window = cavaXWindow;
		xev.xclient.message_type = wmState;
		xev.xclient.format = 32;
		xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
		xev.xclient.data.l[1] = wmStateBelow; // Keeps the window below duh
		xev.xclient.data.l[2] = 0;
		xev.xclient.data.l[3] = 0;
		xev.xclient.data.l[4] = 0;
		XSendEvent(cavaXDisplay, cavaXRoot, FALSE, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
	}

	// Setting window options			
	struct mwmHints hints;
	hints.flags = (1L << 1);
	hints.decorations = borderFlag;
	XChangeProperty(cavaXDisplay, cavaXWindow, mwmHintsProperty, mwmHintsProperty, 32, PropModeReplace, (unsigned char *)&hints, 5);
	
	// move the window in case it didn't by default
	XWindowAttributes xwa;
	XGetWindowAttributes(cavaXDisplay, cavaXWindow, &xwa);
	if(strcmp(windowAlignment, "none"))
		XMoveWindow(cavaXDisplay, cavaXWindow, windowX, windowY);
	
	return 0;
}

int apply_window_settings_x(int *w, int *h)
{
	// Gets the monitors resolution
	if(fs){
		(*w) = DisplayWidth(cavaXDisplay, cavaXScreenNumber);
		(*h) = DisplayHeight(cavaXDisplay, cavaXScreenNumber);
	}

	//Atom xa = XInternAtom(cavaXDisplay, "_NET_WM_WINDOW_TYPE", FALSE); May be used in the future
	//Atom prop;

	// change window type (this makes sure that compoziting managers don't mess with it)
	//if(xa != NULL)
	//{
	//	prop = XInternAtom(cavaXDisplay, "_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
	//	XChangeProperty(cavaXDisplay, cavaXWindow, xa, XA_ATOM, 32, PropModeReplace, (unsigned char *) &prop, 1);
	//}
	// The code above breaks stuff, please don't use it.	
	

	// tell the window manager to switch to a fullscreen state
	xev.xclient.type=ClientMessage;
	xev.xclient.serial = 0;
	xev.xclient.send_event = TRUE;
	xev.xclient.window = cavaXWindow;
	xev.xclient.message_type = wmState;
	xev.xclient.format = 32;
	xev.xclient.data.l[0] = fs ? _NET_WM_STATE_ADD : _NET_WM_STATE_REMOVE;
	xev.xclient.data.l[1] = fullScreen;
	xev.xclient.data.l[2] = 0;
	XSendEvent(cavaXDisplay, cavaXRoot, FALSE, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
	
	// do the usual stuff :P
	if(GLXmode){	
		#ifdef GLX
		glViewport(0, 0, (double)*w, (double)*h);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		
		glOrtho(0, (double)*w, 0, (double)*h, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		#endif
	}else{
		XSetBackground(cavaXDisplay, cavaXGraphics, xbgcol.pixel);
		XClearWindow(cavaXDisplay, cavaXWindow);
	}

	if(!interactable){	
		XRectangle rect;
		XserverRegion region = XFixesCreateRegion(cavaXDisplay, &rect, 1);
		XFixesSetWindowShapeRegion(cavaXDisplay, cavaXWindow, ShapeInput, 0, 0, region);
		XFixesDestroyRegion(cavaXDisplay, region);
	}

	return 0;
}

int get_window_input_x(int *should_reload, int *bs, double *sens, int *bw, int *w, int *h, char *color, char *bcolor, int gradient) {
	while(!*should_reload && XPending(cavaXDisplay)) {
		XNextEvent(cavaXDisplay, &cavaXEvent);
		
		switch(cavaXEvent.type) {
			case KeyPress:
			{
				KeySym key_symbol;
				key_symbol = XkbKeycodeToKeysym(cavaXDisplay, cavaXEvent.xkey.keycode, 0, cavaXEvent.xkey.state & ShiftMask ? 1 : 0);
				switch(key_symbol) {
					// should_reload = 1
					// resizeTerminal = 2
					// bail = -1
					case XK_a:
						(*bs)++;
						return 2;
					case XK_s:
						if((*bs) > 0) (*bs)--;
						return 2;
					case XK_f: // fullscreen
						fs = !fs;
						return 2;
					case XK_Up:
						(*sens) *= 1.05;
						break;
					case XK_Down:
						(*sens) *= 0.95;
						break;
					case XK_Left:
						(*bw)++;
						return 2;
					case XK_Right:
						if ((*bw) > 1) (*bw)--;
						return 2;
					case XK_r: //reload config
						(*should_reload) = 1;
						return 1;
					case XK_q:
						return -1;
					case XK_Escape:
						return -1;
					case XK_b:
						if(transparentFlag) break;
						if(bcolor[0] == '#' && strlen(bcolor) == 7) free(bcolor);
						srand(time(NULL));
						bcolor = (char *) malloc(8*sizeof(char));
						sprintf(bcolor, "#%hhx%hhx%hhx", (rand() % 0x100), (rand() % 0x100), (rand() % 0x100));
						XParseColor(cavaXDisplay, cavaXColormap, bcolor, &xbgcol);
						XAllocColor(cavaXDisplay, cavaXColormap, &xbgcol);
						return 2;
					case XK_c:
						if(gradient) break;
						if(color[0] == '#' && strlen(color) == 7) free(color);
						srand(time(NULL));
						color = (char *) malloc(8*sizeof(char));
						sprintf(color, "#%hhx%hhx%hhx", (rand() % 0x100), (rand() % 0x100), (rand() % 0x100));
						XParseColor(cavaXDisplay, cavaXColormap, color, &xcol);
						XAllocColor(cavaXDisplay, cavaXColormap, &xcol);
						return 2;
				}
				break;
			}
			case ConfigureNotify:
			{
				// This is needed to track the window size
				XConfigureEvent trackedCavaXWindow;
				trackedCavaXWindow = cavaXEvent.xconfigure;
				if((*w) != trackedCavaXWindow.width || (*h) != trackedCavaXWindow.height)
				{
					(*w) = trackedCavaXWindow.width;
					(*h) = trackedCavaXWindow.height;
					return 2;
				}
				break;
			}
			case Expose:
				return 2;
			case VisibilityNotify:
				if(cavaXEvent.xvisibility.state == VisibilityUnobscured) return 2;
				break;
			case ClientMessage:
				if((Atom)cavaXEvent.xclient.data.l[0] == wm_delete_window)
					return -1;
				break;
		}
	}
	return 0;
}

int render_gradient_x(int window_height, int bar_width, double foreground_opacity) {
	if(gradientBox != 0) XFreePixmap(cavaXDisplay, gradientBox);

	gradientBox = XCreatePixmap(cavaXDisplay, cavaXWindow, bar_width, window_height, 32);
	// TODO: Error checks

	for(int I = 0; I < window_height; I++) {
		double step = (double)I / (float)window_height;

		// gradients break compatibility with non ARGB displays.
		// if you could fix this without allocating bilions of colors, please do so

		xgrad[2].pixel ^= xgrad[2].pixel; 	
		if(xgrad[0].red != 0 || xgrad[1].red != 0) {
			if(xgrad[0].red < xgrad[1].red) 
				xgrad[2].pixel |= (unsigned long)(xgrad[0].red + ((xgrad[1].red - xgrad[0].red) * step)) / 256 << 16;
			else xgrad[2].pixel |= (unsigned long)(xgrad[0].red - ((xgrad[0].red - xgrad[1].red) * step)) / 256 << 16;
		}
		
		if(xgrad[0].green != 0 || xgrad[1].green != 0) {
			if(xgrad[0].green < xgrad[1].green) 
				xgrad[2].pixel |= (unsigned long)(xgrad[0].green + ((xgrad[1].green - xgrad[0].green) * step)) / 256 << 8;
			else xgrad[2].pixel |= (unsigned long)(xgrad[0].green - ((xgrad[0].green - xgrad[1].green) * step)) / 256 << 8;
		}
		
		if(xgrad[0].blue != 0 || xgrad[1].blue != 0) {
			if(xgrad[0].blue < xgrad[1].blue) 
				xgrad[2].pixel |= (unsigned long)(xgrad[0].blue + ((xgrad[1].blue - xgrad[0].blue) * step)) / 256;
			else xgrad[2].pixel |= (unsigned long)(xgrad[0].blue - ((xgrad[0].blue - xgrad[1].blue) * step)) / 256;
		}
		
		xgrad[2].pixel |= (unsigned int)((unsigned char)(0xFF * foreground_opacity) << 24);	// set window opacity

		XSetForeground(cavaXDisplay, cavaXGraphics, xgrad[2].pixel);
		XFillRectangle(cavaXDisplay, gradientBox, cavaXGraphics, 0, window_height - I, bar_width, 1);
	}
	return 0;
}

void draw_graphical_x(int window_height, int bars_count, int bar_width, int bar_spacing, int rest, int gradient, int f[200], int flastd[200], double foreground_opacity)
{
	if(GLXmode) {
		#ifdef GLX
		glClearColor(xbgcol.red/65535.0, xbgcol.green/65535.0, xbgcol.blue/65535.0, (!transparentFlag)*1.0); // TODO BG transparency
		glClear(GL_COLOR_BUFFER_BIT);
		#endif
	} else {
		if(gradient&&!gradientBox&&transparentFlag) render_gradient_x(window_height, bar_width, foreground_opacity); 
	}
	
	if(GLXmode) {
		#ifdef GLX
		float glColors[11] = {gradient ? xgrad[0].red/65535.0 : xcol.red/65535.0, gradient ? xgrad[0].green/65535.0 : xcol.green/65535.0, gradient ? xgrad[0].blue/65535.0 : xcol.blue/65535.0,
							xgrad[1].red/65535.0, xgrad[1].green/65535.0, xgrad[1].blue/65535.0, foreground_opacity, ((unsigned int)shadow_color>>24)%256/255.0, ((unsigned int)shadow_color>>16)%256/255.0, ((unsigned int)shadow_color>>8)%256/255.0, (unsigned int)shadow_color%256/255.0};
		if(drawGLBars(rest, bar_width, bar_spacing, bars_count, window_height, shadow, gradient, glColors, f)) exit(EXIT_FAILURE);
		#endif
	} else {	
		// draw bars on the X11 window
		for(int i = 0; i < bars_count; i++) {
			// this fixes a rendering bug
			if(f[i] > window_height) f[i] = window_height;
				
			if(f[i] > flastd[i]) {
				if(gradient&&transparentFlag)
					XCopyArea(cavaXDisplay, gradientBox, cavaXWindow, cavaXGraphics, 0, window_height - f[i], bar_width, f[i] - flastd[i], rest + i*(bar_spacing+bar_width), window_height - f[i]);	
				else {
					XSetForeground(cavaXDisplay, cavaXGraphics, xcol.pixel);
					XFillRectangle(cavaXDisplay, cavaXWindow, cavaXGraphics, rest + i*(bar_spacing+bar_width), window_height - f[i], bar_width, f[i] - flastd[i]);
				}
			}
			else if (f[i] < flastd[i])
				XClearArea(cavaXDisplay, cavaXWindow, rest + i*(bar_spacing+bar_width), window_height - flastd[i], bar_width, flastd[i] - f[i], FALSE);
		}
	}
	
	#ifdef GLX
	if(GLXmode) {
		glXSwapBuffers(cavaXDisplay, cavaXWindow);
		glXWaitGL();
	}
	#endif
	
	XSync(cavaXDisplay, 0);
	return;
}

void adjust_x(void){
	if(gradientBox != 0) { XFreePixmap(cavaXDisplay, gradientBox); gradientBox = 0; };
}

void cleanup_graphical_x(void)
{
	// make sure that all events are dead by this point
	XSync(cavaXDisplay, 0);
	
	adjust_x();	
	 
	#ifdef GLX
		glXMakeCurrent(cavaXDisplay, 0, 0);
		if(GLXmode) glXDestroyContext(cavaXDisplay, cavaGLXContext);
	#endif
	XFreeGC(cavaXDisplay, cavaXGraphics);
	XDestroyWindow(cavaXDisplay, cavaXWindow);
	XFreeColormap(cavaXDisplay, cavaXColormap);
	XCloseDisplay(cavaXDisplay);
	return;
}
