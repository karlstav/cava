#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xmd.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include "output/graphical.h"

XVisualInfo cavaVInfo;
XSetWindowAttributes cavaAttr;
Display *cavaXDisplay;
Screen *cavaXScreen;
Window cavaXWindow;
GC cavaXGraphics;
Colormap cavaXColormap;
int cavaXScreenNumber;
XClassHint cavaXClassHint;
XColor xbgcol, xcol, xgrad[3];
XEvent cavaXEvent;
Atom wm_delete_window;
XWMHints cavaXWMHints;

/**
	The following struct is needed for
	passing window flags (aka. talking to the window manager)
**/
struct mwmHints {
    unsigned long flags;
    unsigned long functions;
    unsigned long decorations;
    long input_mode;
    unsigned long status;
};

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

// Some window manager definitions
#define _NET_WM_STATE_REMOVE 0;
#define _NET_WM_STATE_ADD 1;
#define _NET_WM_STATE_TOGGLE 2;

int init_window_x(char *color, char *bcolor, int col, int bgcol, int set_win_props, char **argv, int argc, int gradient, char *gradient_color_1, char *gradient_color_2)
{
	// connect to the X server
	cavaXDisplay = XOpenDisplay(NULL);
	if(cavaXDisplay == NULL) {
		fprintf(stderr, "cannot open X display\n");
		return 1;
	}
	// get the screen
	cavaXScreen = DefaultScreenOfDisplay(cavaXDisplay);
	// get the display number
	cavaXScreenNumber = DefaultScreen(cavaXDisplay);
	
	// calculate x and y
	if(!strcmp(windowAlignment, "top")){
		windowX = (cavaXScreen->width - w) / 2 + windowX;
	}else if(!strcmp(windowAlignment, "bottom")){
		windowX = (cavaXScreen->width - w) / 2 + windowX;
		windowY = (cavaXScreen->height - h) + (-1*windowY);
	}else if(!strcmp(windowAlignment, "top_left")){
		// Nothing to do here :P
	}else if(!strcmp(windowAlignment, "top_right")){
		windowX = (cavaXScreen->width - w) + (-1*windowX);
	}else if(!strcmp(windowAlignment, "left")){
		windowY = (cavaXScreen->height - h) / 2;
	}else if(!strcmp(windowAlignment, "right")){
		windowX = (cavaXScreen->width - w) + (-1*windowX);
		windowY = (cavaXScreen->height - h) / 2 + windowY;
	}else if(!strcmp(windowAlignment, "bottom_left")){
		windowY = (cavaXScreen->height - h) + (-1*windowY);
	}else if(!strcmp(windowAlignment, "bottom_right")){
		windowX = (cavaXScreen->width - w) + (-1*windowX);
		windowY = (cavaXScreen->height - h) + (-1*windowY);
	}else if(!strcmp(windowAlignment, "center")){
		windowX = (cavaXScreen->width - w) / 2 + windowX;
		windowY = (cavaXScreen->height - h) / 2 + windowY;
	}
	// Some error checking
	#ifdef DEBUG
		if(windowX > cavaXScreen->width - w) printf("Warning: Screen out of bounds (X axis)!");
		if(windowY > cavaXScreen->width - w) printf("Warning: Screen out of bounds (Y axis)!");
	#endif
	// create X window
	if(transparentFlag)
	{	
		// Set the window to 32 bit mode (if supported), and also set the screen to a blank pixel
		XMatchVisualInfo(cavaXDisplay, cavaXScreenNumber, 32, TrueColor, &cavaVInfo);
		cavaAttr.colormap = XCreateColormap(cavaXDisplay, DefaultRootWindow(cavaXDisplay), cavaVInfo.visual, AllocNone);
		cavaAttr.border_pixel = 0;
		cavaAttr.background_pixel = 0;
		// Now create a window using those options
		cavaXWindow = XCreateWindow(cavaXDisplay, DefaultRootWindow(cavaXDisplay), windowX, windowY, w, h, 0, cavaVInfo.depth, InputOutput, cavaVInfo.visual, CWColormap | CWBorderPixel | CWBackPixel, &cavaAttr);
	}
	else
		cavaXWindow = XCreateSimpleWindow(cavaXDisplay, RootWindow(cavaXDisplay, cavaXScreenNumber), windowX, windowY, w, h, 1, WhitePixel(cavaXDisplay, cavaXScreenNumber), BlackPixel(cavaXDisplay, cavaXScreenNumber));
	
	// add titlebar name
	XStoreName(cavaXDisplay, cavaXWindow, "CAVA");

	// fix for error while closing window
	wm_delete_window = XInternAtom (cavaXDisplay, "WM_DELETE_WINDOW", FALSE);
	XSetWMProtocols(cavaXDisplay, cavaXWindow, &wm_delete_window, 1);
	
	// set window properties
	if(set_win_props)
	{
		cavaXWMHints.flags = InputHint | StateHint;
		cavaXWMHints.initial_state = NormalState;
		cavaXClassHint.res_name = (char *)"Cava";
		XmbSetWMProperties(cavaXDisplay, cavaXWindow, NULL, NULL, argv, argc, NULL, &cavaXWMHints, &cavaXClassHint);
	}
	
	// add inputs
	XSelectInput(cavaXDisplay, cavaXWindow, StructureNotifyMask | ExposureMask | KeyPressMask | KeymapNotify);
	// set the current window as active (mapping windows)		
	XMapWindow(cavaXDisplay, cavaXWindow);
	// get graphics context
	cavaXGraphics = XCreateGC(cavaXDisplay, cavaXWindow, 0, 0);
	// get colormap
	if(transparentFlag) cavaXColormap = cavaAttr.colormap; 
	else cavaXColormap = DefaultColormap(cavaXDisplay, cavaXScreenNumber);
	// Get the color
	if(!strcmp(color, "default"))
	{
		/**
			A bit of description.
			Because it can get confusing, I know.
			This chunk of code grabs sums the desktop background and makes an average color,
			but of course we can't do the whole desktop because optimization.

			So we instead use precision, the higher the better is the color we get.
			However, if the precision is too low it takes a long time to load, so be careful.
			Just remember, SCREEN WIDTH / XPRECISION AND SCREEN HEIGHT / YPRECISION MUST BE
			A WHOLE NUMBER OTHERWISE THIS WHOLE FUNCTION BREAKS. There's a workaround below,
			but I don't think it could always work.
		**/

		// First we get the background
		XImage *background = XGetImage (cavaXDisplay, RootWindow (cavaXDisplay, cavaXScreenNumber), 0, 0, cavaXScreen->width, cavaXScreen->height, AllPlanes, XYPixmap);
		// Color sums
		unsigned long redSum = 0, greenSum = 0, blueSum = 0;
		// A color variable a.k.a foobar
		XColor tempColor;
		// Our precision variables (the higher they are the slower the calculation will get)
		int xPrecision = 20, yPrecision = 20;
		// Making sure that xPrecision is a whole number
		while(1){
			if(((double)cavaXScreen->width / (double)xPrecision) == ((int)cavaXScreen->width / (int)xPrecision)) break;
			else xPrecision++;
		}

		// Making sure that yPrecision is a whole number
		while(1){
			if(((double)cavaXScreen->height / (double)yPrecision) == ((int)cavaXScreen->height / (int)yPrecision)) break;
			else yPrecision++;
		}

		// Our screen width
		for(unsigned short i = 0; i < cavaXScreen->width; i+=(cavaXScreen->width / xPrecision))
		{
			// Our screen height
			for(unsigned short I = 0; I < cavaXScreen->height; I+=(cavaXScreen->height / yPrecision))
			{
				// This captures the pixel
				tempColor.pixel = XGetPixel (background, i, I);
				// This saves it
				XQueryColor(cavaXDisplay, cavaXColormap, &tempColor);
				// Because colors in Xlib are 48bit for some reason
				redSum += tempColor.red / 256;
				greenSum += tempColor.green / 256;
				blueSum += tempColor.blue / 256;
			}
		}

		// Now turn the sums into averages
		redSum /= xPrecision * yPrecision;
		greenSum /= xPrecision * yPrecision;
		blueSum /= xPrecision * yPrecision;

		// Nuke the leftovers
		XDestroyImage(background);

		// In case the background image is too bright or dark
		if(redSum < 0x10) redSum += 0x10;
		else if(redSum > 0xF0) redSum -= 0x10;
		if(greenSum < 0x10) greenSum += 0x10;
		else if(greenSum > 0xF0) greenSum -= 0x10;
		if(blueSum < 0x10) blueSum += 0x10;
		else if(blueSum > 0xF0) blueSum -= 0x10;
		// Again 48bit colors
		xcol.red = redSum * 256;
		xcol.green = greenSum * 256;
		xcol.blue = blueSum * 256;
		// Set the color channels
		xcol.flags = DoRed | DoGreen | DoBlue;
		// Save it
		XAllocColor(cavaXDisplay, cavaXColormap, &xcol);
	}
	else if(color[0] != '#')
	{
		color = (char *)malloc((char)8); // default hex color string
		if(color == NULL)
		{
			fprintf(stderr, "memory error\n");
			return 1;
		}
		switch(col)
		{
			case 0:
				strcpy(color, "#000000");
				break;
			case 1:
				strcpy(color, "#FF0000");
				break;
			case 2:
				strcpy(color, "#00FF00");
				break;
			case 3:
				strcpy(color, "#FFFF00");
				break;
			case 4:
				strcpy(color, "#0000FF");
				break;
			case 5:
				strcpy(color, "#FF00FF");
				break;
			case 6:
				strcpy(color, "#00FFFF");
				break;
			case 7:
				strcpy(color, "#FFFFFF");
				break;
		}
	}
	else
	{
		// because iniparser makes strings consts -_-
		char *temp = (char *)malloc((char)8);
		if(temp == NULL)
		{
			fprintf(stderr, "memory error\n");
			return 1;
		}
		strcpy(temp, color);
		color = temp;
	}
	
	if(bcolor[0] != '#')
	{
		bcolor = (char *)malloc((char)8); // default hex color string
		if(bcolor == NULL)
		{
			fprintf(stderr, "memory error\n");
			return 1;
		}
		switch(bgcol)
		{
			case 0:
				strcpy(bcolor, "#000000");
				break;
			case 1:
				strcpy(bcolor, "#FF0000");
				break;
			case 2:
				strcpy(bcolor, "#00FF00");
				break;
			case 3:
				strcpy(bcolor, "#FFFF00");
				break;
			case 4:
				strcpy(bcolor, "#0000FF");
				break;
			case 5:
				strcpy(bcolor, "#FF00FF");
				break;
			case 6:
				strcpy(bcolor, "#00FFFF");
				break;
			case 7:
				strcpy(bcolor, "#FFFFFF");
				break;
		}
	}
	else
	{
		// because iniparser makes strings consts -_-
		char *temp = (char *)malloc((char)8);
		if(temp == NULL)
		{
			fprintf(stderr, "memory error\n");
			return 1;
		}
		strcpy(temp, bcolor);
		bcolor = temp;
	}
	XParseColor(cavaXDisplay, cavaXColormap, bcolor, &xbgcol);
	XAllocColor(cavaXDisplay, cavaXColormap, &xbgcol);
	XParseColor(cavaXDisplay, cavaXColormap, color, &xcol);
	XAllocColor(cavaXDisplay, cavaXColormap, &xcol);
	if(strcmp(color, "default"))
	{
		XParseColor(cavaXDisplay, cavaXColormap, color, &xcol);
		XAllocColor(cavaXDisplay, cavaXColormap, &xcol);
	}
	if(gradient)
	{
		XParseColor(cavaXDisplay, cavaXColormap, gradient_color_1, &xgrad[2]);
		XAllocColor(cavaXDisplay, cavaXColormap, &xgrad[2]);
		XParseColor(cavaXDisplay, cavaXColormap, gradient_color_1, &xgrad[0]);
		XAllocColor(cavaXDisplay, cavaXColormap, &xgrad[0]);
		XParseColor(cavaXDisplay, cavaXColormap, gradient_color_2, &xgrad[1]);
		XAllocColor(cavaXDisplay, cavaXColormap, &xgrad[1]);
	}
	return 0;
}

int apply_window_settings_x(void)
{
	// Gets the monitors resolution
	if(fs){
		w = DisplayWidth(cavaXDisplay, cavaXScreenNumber);
		h = DisplayHeight(cavaXDisplay, cavaXScreenNumber);
	}

	// Window manager options (atoms)
	Atom wmState = XInternAtom(cavaXDisplay, "_NET_WM_STATE", FALSE);
	Atom fullScreen = XInternAtom(cavaXDisplay, "_NET_WM_STATE_FULLSCREEN", FALSE);
	Atom mwmHintsProperty = XInternAtom(cavaXDisplay, "_MOTIF_WM_HINTS", FALSE);
	Atom wmStateBelow = XInternAtom(cavaXDisplay, "_NET_WM_STATE_BELOW", TRUE);
	//Atom xa = XInternAtom(cavaXDisplay, "_NET_WM_WINDOW_TYPE", FALSE); May be used in the future
	Atom prop;

	// Setting window options			
	struct mwmHints hints;
	hints.flags = (1L << 1);
	hints.decorations = borderFlag;		// setting the window border here
	XChangeProperty(cavaXDisplay, cavaXWindow, mwmHintsProperty, mwmHintsProperty, 32, PropModeReplace, (unsigned char *)&hints, 5);

	// use XEvents to toggle fullscreenxcolor xlib
	XEvent xev;
	xev.xclient.type=ClientMessage;
	xev.xclient.serial = 0;
	xev.xclient.send_event = TRUE;
	xev.xclient.window = cavaXWindow;
	xev.xclient.message_type = wmState;
	xev.xclient.format = 32;
	if(fs) {xev.xclient.data.l[0] = _NET_WM_STATE_ADD;}
	else {xev.xclient.data.l[0] = _NET_WM_STATE_REMOVE;}
	xev.xclient.data.l[1] = fullScreen;
	xev.xclient.data.l[2] = 0;
	XSendEvent(cavaXDisplay, DefaultRootWindow(cavaXDisplay), FALSE, SubstructureRedirectMask | SubstructureNotifyMask, &xev);

	// also use them to keep the window at the bottom of the stack
	/**
		This is how a XEvent should look like:
			
		window  = the respective client window
		message_type = _NET_WM_STATE
		format = 32
		data.l[0] = the action, as listed below
		data.l[1] = first property to alter
		data.l[2] = second property to alter
		data.l[3] = source indication (0-unk,1-normal app,2-pager)
		other data.l[] elements = 0
	**/
	if(keepInBottom){
		xev.xclient.type = ClientMessage;
		xev.xclient.window = cavaXWindow;
		xev.xclient.message_type = wmState;
		xev.xclient.format = 32;
		xev.xclient.data.l[0] = _NET_WM_STATE_ADD;
		xev.xclient.data.l[1] = wmStateBelow; // Keeps the window below duh
		xev.xclient.data.l[2] = 0;
		xev.xclient.data.l[3] = 0;
		xev.xclient.data.l[4] = 0;
		// Push the event
		XSendEvent(cavaXDisplay, DefaultRootWindow(cavaXDisplay), FALSE, SubstructureRedirectMask | SubstructureNotifyMask, &xev);
	}
  	
	// move the window in case it didn't by default
	XWindowAttributes xwa;
	XGetWindowAttributes(cavaXDisplay, cavaXWindow, &xwa);
	if(strcmp(windowAlignment, "none"))
		XMoveWindow(cavaXDisplay, cavaXWindow, windowX - xwa.x, windowY - xwa.y);
		
	// do the usual stuff :P
	if(!transparentFlag)
	{
		XSetForeground(cavaXDisplay, cavaXGraphics, xbgcol.pixel);
		XFillRectangle(cavaXDisplay, cavaXWindow, cavaXGraphics, 0, 0, w, h);
	}
	else
		XClearWindow(cavaXDisplay, cavaXWindow);
	
	// change window type (this makes sure that compoziting managers don't mess with it)
	//if(xa != NULL)
	//{
	//	prop = XInternAtom(cavaXDisplay, "_NET_WM_WINDOW_TYPE_DESKTOP", FALSE);
	//	XChangeProperty(cavaXDisplay, cavaXWindow, xa, XA_ATOM, 32, PropModeReplace, (unsigned char *) &prop, 1);
	//}
	// The code above breaks stuff, please don't use it.
	return 0;
}

int get_window_input_x(int *should_reload, int *bs, double *sens, int *bw, int *modes, int *mode, int *w, int *h, char *color, char *bcolor, int gradient)
{
	while(!*should_reload && (XEventsQueued(cavaXDisplay, QueuedAfterFlush) > 0))
	{
		XNextEvent(cavaXDisplay, &cavaXEvent);
		
		switch(cavaXEvent.type)
		{
			case KeyPress:
			{
				KeySym key_symbol;
				key_symbol = XKeycodeToKeysym(cavaXDisplay, cavaXEvent.xkey.keycode, 0);
				switch(key_symbol)
				{
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
					case XK_m:
						if ((*mode) == (*modes)) {
						(*mode) = 1;
						} else {
						(*mode)++;
						}
						break;
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
			case ClientMessage:
				if((Atom)cavaXEvent.xclient.data.l[0] == wm_delete_window)
					return -1;
				break;
		}
	}
	return 0;
}

void draw_graphical_x(int window_height, int bars_count, int bar_width, int bar_spacing, int rest, int gradient, int f[200], int flastd[200])
{
	// draw bars on the X11 window
	for(int i = 0; i < bars_count; i++)
	{	
		// this fixes a rendering bug
		if(f[i] > h) f[i] = window_height;
		
		
		if(f[i] > flastd[i])
		{
			if(!gradient)
			{
				XSetForeground(cavaXDisplay, cavaXGraphics, xcol.pixel);
				XFillRectangle(cavaXDisplay, cavaXWindow, cavaXGraphics, rest + i*(bar_spacing+bar_width), window_height - f[i], bar_width, f[i] - flastd[i]);
			}
			else
			{
				for(int I = flastd[i]; I < f[i]; I++)
				{
					double step = (double)I / (float)window_height;
					
					// major optimization over allocating colors (like seriously, it killed a 6th gen i7)
					// although this may or may NOT work on some systems, like if you're running in non-ARGB color mode
					xgrad[2].pixel ^= xgrad[2].pixel; // YOU REALLY WANT TO RESET THIS TO NULL
				
		   			if(xgrad[0].red != 0 || xgrad[1].red != 0)
		   			{
						if(xgrad[0].red < xgrad[1].red) 
							xgrad[2].pixel |= (unsigned long)(xgrad[0].red + ((xgrad[1].red - xgrad[0].red) * step)) / 256 << 16;
						else xgrad[2].pixel |= (unsigned long)(xgrad[0].red - ((xgrad[0].red - xgrad[1].red) * step)) / 256 << 16;
					}
					
		   			if(xgrad[0].green != 0 || xgrad[1].green != 0)
		   			{
						if(xgrad[0].green < xgrad[1].green) 
							xgrad[2].pixel |= (unsigned long)(xgrad[0].green + ((xgrad[1].green - xgrad[0].green) * step)) / 256 << 8;
						else xgrad[2].pixel |= (unsigned long)(xgrad[0].green - ((xgrad[0].green - xgrad[1].green) * step)) / 256 << 8;
					}
					
					if(xgrad[0].blue != 0 || xgrad[1].blue != 0)
		   			{
						if(xgrad[0].blue < xgrad[1].blue) 
							xgrad[2].pixel |= (unsigned long)(xgrad[0].blue + ((xgrad[1].blue - xgrad[0].blue) * step)) / 256;
						else xgrad[2].pixel |= (unsigned long)(xgrad[0].blue - ((xgrad[0].blue - xgrad[1].blue) * step)) / 256;
					}
					
        			xgrad[2].pixel |= 0xFF000000;	// set window opacity

					XSetForeground(cavaXDisplay, cavaXGraphics, xgrad[2].pixel);
					XDrawLine(cavaXDisplay, cavaXWindow, cavaXGraphics, rest + i*(bar_spacing+bar_width), window_height - I, rest + i*(bar_spacing+bar_width) + bar_width - 1, window_height - I);
				}
			}
		}
		else if (f[i] < flastd[i]){
			if(transparentFlag)
			{
				XClearArea(cavaXDisplay, cavaXWindow, rest + i*(bar_spacing+bar_width), window_height - flastd[i], bar_width, flastd[i] - f[i], FALSE);
			}
			else
			{
				XSetForeground(cavaXDisplay, cavaXGraphics, xbgcol.pixel);
				XFillRectangle(cavaXDisplay, cavaXWindow, cavaXGraphics, rest + i*(bar_spacing+bar_width), window_height - flastd[i], bar_width, flastd[i] - f[i]);
			}
		}
	}

	XSync(cavaXDisplay, 1);
	return;
}

void cleanup_graphical_x(void)
{
	XDestroyWindow(cavaXDisplay, cavaXWindow);
	XCloseDisplay(cavaXDisplay);
	return;
}
