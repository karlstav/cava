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

#ifdef GLX
int drawGLBars(GLVertex *glVertex, int rest, int bw, int bs, int bars, int h, int shadow, int gradient, float colors[11], int *f) {
	for(int i = 0; i < bars; i++) {
		double point[4];
		point[0] = rest+(bw+bs)*i;
		point[1] = rest+(bw+bs)*i+bw;
		point[2] = f[i]+shadow;
		point[3] = shadow;
		if(glVertex == NULL) {
			glVertex = (GLVertex*) malloc(sizeof(GLVertex)*bars*(shadow > 0 ? 12 : 4 )+1);
			if(glVertex == NULL) {
				fprintf(stderr, "Couldn't allocate memory for vertices!\n");
				return 1;
			}
			glVertexPointer(2, GL_FLOAT, sizeof(GLVertex), &glVertex[0].x);
			glColorPointer(4, GL_FLOAT, sizeof(GLVertex), &glVertex[0].r);
		}
			
		glVertex[i*4].x = point[0];
		glVertex[i*4].y = point[2];
		glVertex[i*4+1].x = point[1];
		glVertex[i*4+1].y = point[2];
		glVertex[i*4+2].x = point[1];
		glVertex[i*4+2].y = point[3];
		glVertex[i*4+3].x = point[0];
		glVertex[i*4+3].y = point[3];
		if(gradient){
			glVertex[i*4].a = colors[6];
			glVertex[i*4].r = (colors[0]+(colors[3]-colors[0])*f[i]/h);
			glVertex[i*4].g = (colors[1]+(colors[4]-colors[1])*f[i]/h);
			glVertex[i*4].b = (colors[2]+(colors[5]-colors[2])*f[i]/h);
			glVertex[i*4+2].a = colors[6];
			glVertex[i*4+2].r = colors[0];
			glVertex[i*4+2].g = colors[1];
			glVertex[i*4+2].b = colors[2];
			glVertex[i*4+3].a = colors[6];
			glVertex[i*4+3].r = colors[0];
			glVertex[i*4+3].g = colors[1];
			glVertex[i*4+3].b = colors[2];
			glVertex[i*4+1].a = colors[6];
			glVertex[i*4+1].r = (colors[0]+(colors[3]-colors[0])*f[i]/h);
			glVertex[i*4+1].g = (colors[1]+(colors[4]-colors[1])*f[i]/h);
			glVertex[i*4+1].b = (colors[2]+(colors[5]-colors[2])*f[i]/h);
		} else {
			for(int j = 0; j < 4; j++) {
				glVertex[i*4+j].a = colors[6];
				glVertex[i*4+j].r = colors[0];
				glVertex[i*4+j].g = colors[1];
				glVertex[i*4+j].b = colors[2];
			}
		}
		if(shadow){
			glVertex[bars*4+i*8].x = point[1];
			glVertex[bars*4+i*8].y = point[2];
			glVertex[bars*4+i*8+1].x = point[1];
			glVertex[bars*4+i*8+1].y = point[3];
			glVertex[bars*4+i*8+2].x = point[1]+shadow;
			glVertex[bars*4+i*8+2].y = point[3]-shadow;
			glVertex[bars*4+i*8+3].x = point[1]+shadow;
			glVertex[bars*4+i*8+3].y = point[2]-shadow;
			glVertex[bars*4+i*8+4].x = point[0];
			glVertex[bars*4+i*8+4].y = point[3];
			glVertex[bars*4+i*8+5].x = point[1];
			glVertex[bars*4+i*8+5].y = point[3];
			glVertex[bars*4+i*8+6].x = point[1]+shadow;
			glVertex[bars*4+i*8+6].y = point[3]-shadow;
			glVertex[bars*4+i*8+7].x = point[0]+shadow;
			glVertex[bars*4+i*8+7].y = point[3]-shadow;
			for(char j = 0; j < 2; j++){
				for(char k = 0; k < 2; k++) {
					glVertex[bars*4+i*8+j*4+k].a = colors[7];
					glVertex[bars*4+i*8+j*4+k].r = colors[8]; 
					glVertex[bars*4+i*8+j*4+k].g = colors[9];
					glVertex[bars*4+i*8+j*4+k].b = colors[10];
				}
				for(char k = 0; k < 2; k++) {
					glVertex[bars*4+i*8+j*4+k+2].a = 0.0;
					glVertex[bars*4+i*8+j*4+k+2].r = 0.0;
					glVertex[bars*4+i*8+j*4+k+2].g = 0.0;
					glVertex[bars*4+i*8+j*4+k+2].b = 0.0;
				}
			}
		}
	}
	return 0;
}
#endif
