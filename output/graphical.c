#include "output/graphical.h"

void calculate_win_pos(int *winX, int *winY, int winW, int winH, int scrW, int scrH, char *winPos) {
	if(!strcmp(winPos, "top")){
		(*winX) = (scrW - winW) / 2 + (*winX);
	}else if(!strcmp(winPos, "bottom")){
		(*winX) = (scrW - winW) / 2 + (*winX);
		(*winY) = (scrH - winH) + (-1*(*winY));
	}else if(!strcmp(winPos, "top_right")){
		(*winX) = (scrW - winW) + (-1*(*winX));
	}else if(!strcmp(winPos, "left")){
		(*winY) = (scrH - winH) / 2;
	}else if(!strcmp(winPos, "right")){
		(*winX) = (scrW - winW) + (-1*(*winX));
		(*winY) = (scrH - winH) / 2 + (*winY);
	}else if(!strcmp(winPos, "bottom_left")){
		(*winY) = (scrH - winH) + (-1*(*winY));
	}else if(!strcmp(winPos, "bottom_right")){
		(*winX) = (scrW - winW) + (-1*(*winX));
		(*winY) = (scrH - winH) + (-1*(*winY));
	}else if(!strcmp(winPos, "center")){
		(*winX) = (scrW - winW) / 2 + (*winX);
		(*winY) = (scrH - winH) / 2 + (*winY);
	}
	// Some error checking
	#ifdef DEBUG
		if(winX > scrW - winW) printf("Warning: Screen out of bounds (X axis)!");
		if(winY > scrH - winH) printf("Warning: Screen out of bounds (Y axis)!");
	#endif
}

#ifdef GL
int drawGLBars(int rest, int bw, int bs, int bars, int h, int shadow, int gradient, float colors[11], int *f) {
	for(int i = 0; i < bars; i++) {
		double point[4];
		point[0] = rest+(bw+bs)*i;
		point[1] = rest+(bw+bs)*i+bw;
		point[2] = f[i]+shadow > h-shadow ? h-shadow : f[i]+shadow;
		point[3] = shadow;
		
		glBegin(GL_QUADS);
			if(shadow) {
				// left side
				glColor4f(colors[8], colors[9], colors[10], colors[7]);
				glVertex2f(point[0], point[2]);
				glVertex2f(point[0], point[3]);
				glColor4f(0.0, 0.0, 0.0, 0.0);
				glVertex2f(point[0]-shadow/2, point[3]-shadow);
				glVertex2f(point[0]-shadow/2, point[2]+shadow/2);
				
				// right side
				glColor4f(colors[8], colors[9], colors[10], colors[7]);
				glVertex2f(point[1], point[2]);
				glVertex2f(point[1], point[3]);
				glColor4f(0.0, 0.0, 0.0, 0.0);
				glVertex2f(point[1]+shadow, point[3]-shadow);
				glVertex2f(point[1]+shadow, point[2]+shadow/2);
				
				// top side
				glColor4f(colors[8], colors[9], colors[10], colors[7]);
				glVertex2f(point[1], point[2]);
				glVertex2f(point[0], point[2]);
				glColor4f(0.0, 0.0, 0.0, 0.0);
				glVertex2f(point[0]-shadow/2, point[2]+shadow/2);
				glVertex2f(point[1]+shadow, point[2]+shadow/2);

				// bottom side
				glColor4f(colors[8], colors[9], colors[10], colors[7]);
				glVertex2f(point[1], point[3]);
				glVertex2f(point[0], point[3]);
				glColor4f(0.0, 0.0, 0.0, 0.0);
				glVertex2f(point[0]-shadow/2, point[3]-shadow);
				glVertex2f(point[1]+shadow, point[3]-shadow);
			}
			
			glColor4f(
			gradient ? (colors[0]+(colors[3]-colors[0])*f[i]/h) : colors[0], 
			gradient ? (colors[1]+(colors[4]-colors[1])*f[i]/h) : colors[1],
			gradient ? (colors[2]+(colors[5]-colors[2])*f[i]/h) : colors[2], colors[6]);
			glVertex2f(point[0], point[2]);
			glVertex2f(point[1], point[2]);
			
			glColor4f(colors[0], colors[1], colors[2], colors[6]);
			glVertex2f(point[1], point[3]);
			glVertex2f(point[0], point[3]);
		glEnd();
	}
	return 0;
}
#endif
