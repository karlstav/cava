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
int drawGLBars(GLVertex *glVertex, int rest, int bw, int bs, int bars, int h, int shadow, int gradient, float colors[11], int *f) {
	for(int i = 0; i < bars; i++) {
		double point[4];
		point[0] = rest+(bw+bs)*i;
		point[1] = rest+(bw+bs)*i+bw;
		point[2] = f[i]+shadow > h-shadow ? h-shadow : f[i]+shadow;
		point[3] = shadow;
		if(glVertex == NULL) {
			glVertex = (GLVertex*) malloc(sizeof(GLVertex)*bars*(shadow > 0 ? 20 : 4 )+1);
			if(glVertex == NULL) {
				fprintf(stderr, "Couldn't allocate memory for vertices!\n");
				return 1;
			}
			glVertexPointer(2, GL_FLOAT, sizeof(GLVertex), &glVertex[0].x);
			glColorPointer(4, GL_FLOAT, sizeof(GLVertex), &glVertex[0].r);
		}

		char shdw = shadow ? 1 : 0;	
		glVertex[shdw*16*bars+i*4].x = point[0];
		glVertex[shdw*16*bars+i*4].y = point[2];
		glVertex[shdw*16*bars+i*4+1].x = point[1];
		glVertex[shdw*16*bars+i*4+1].y = point[2];
		glVertex[shdw*16*bars+i*4+2].x = point[1];
		glVertex[shdw*16*bars+i*4+2].y = point[3];
		glVertex[shdw*16*bars+i*4+3].x = point[0];
		glVertex[shdw*16*bars+i*4+3].y = point[3];
		if(gradient){
			glVertex[shdw*16*bars+i*4].a = colors[6];
			glVertex[shdw*16*bars+i*4].r = (colors[0]+(colors[3]-colors[0])*f[i]/h);
			glVertex[shdw*16*bars+i*4].g = (colors[1]+(colors[4]-colors[1])*f[i]/h);
			glVertex[shdw*16*bars+i*4].b = (colors[2]+(colors[5]-colors[2])*f[i]/h);
			glVertex[shdw*16*bars+i*4+2].a = colors[6];
			glVertex[shdw*16*bars+i*4+2].r = colors[0];
			glVertex[shdw*16*bars+i*4+2].g = colors[1];
			glVertex[shdw*16*bars+i*4+2].b = colors[2];
			glVertex[shdw*16*bars+i*4+3].a = colors[6];
			glVertex[shdw*16*bars+i*4+3].r = colors[0];
			glVertex[shdw*16*bars+i*4+3].g = colors[1];
			glVertex[shdw*16*bars+i*4+3].b = colors[2];
			glVertex[shdw*16*bars+i*4+1].a = colors[6];
			glVertex[shdw*16*bars+i*4+1].r = (colors[0]+(colors[3]-colors[0])*f[i]/h);
			glVertex[shdw*16*bars+i*4+1].g = (colors[1]+(colors[4]-colors[1])*f[i]/h);
			glVertex[shdw*16*bars+i*4+1].b = (colors[2]+(colors[5]-colors[2])*f[i]/h);
		} else {
			for(int j = 0; j < 4; j++) {
				glVertex[shdw*16*bars+i*4+j].a = colors[6];
				glVertex[shdw*16*bars+i*4+j].r = colors[0];
				glVertex[shdw*16*bars+i*4+j].g = colors[1];
				glVertex[shdw*16*bars+i*4+j].b = colors[2];
			}
		}
		if(shadow){
			glVertex[i*16].x = point[1];
			glVertex[i*16].y = point[2];
			glVertex[i*16+1].x = point[1];
			glVertex[i*16+1].y = point[3];
			glVertex[i*16+2].x = point[1]+shadow;
			glVertex[i*16+2].y = point[3]-shadow;
			glVertex[i*16+3].x = point[1]+shadow;
			glVertex[i*16+3].y = point[2]+shadow/2;
			glVertex[i*16+4].x = point[0];
			glVertex[i*16+4].y = point[3];
			glVertex[i*16+5].x = point[1];
			glVertex[i*16+5].y = point[3];
			glVertex[i*16+6].x = point[1]+shadow;
			glVertex[i*16+6].y = point[3]-shadow;
			glVertex[i*16+7].x = point[0]-shadow/2;
			glVertex[i*16+7].y = point[3]-shadow;
			glVertex[i*16+8].x = point[0];
			glVertex[i*16+8].y = point[2];
			glVertex[i*16+9].x = point[0];
			glVertex[i*16+9].y = point[3];
			glVertex[i*16+10].x = point[0]-shadow/2;
			glVertex[i*16+10].y = point[3]-shadow;
			glVertex[i*16+11].x = point[0]-shadow/2;
			glVertex[i*16+11].y = point[2]+shadow/2;
			glVertex[i*16+12].x = point[1];
			glVertex[i*16+12].y = point[2];
			glVertex[i*16+13].x = point[0];
			glVertex[i*16+13].y = point[2];
			glVertex[i*16+14].x = point[0]-shadow/2;
			glVertex[i*16+14].y = point[2]+shadow/2;
			glVertex[i*16+15].x = point[1]+shadow;
			glVertex[i*16+15].y = point[2]+shadow/2;
			for(char j = 0; j < 4; j++){
				for(char k = 0; k < 2; k++) {
					glVertex[i*16+j*4+k].a = colors[7];
					glVertex[i*16+j*4+k].r = colors[8]; 
					glVertex[i*16+j*4+k].g = colors[9];
					glVertex[i*16+j*4+k].b = colors[10];
				}
				for(char k = 0; k < 2; k++) {
					glVertex[i*16+j*4+k+2].a = 0.0;
					glVertex[i*16+j*4+k+2].r = 0.0;
					glVertex[i*16+j*4+k+2].g = 0.0;
					glVertex[i*16+j*4+k+2].b = 0.0;
				}
			}
		}
	}
	return 0;
}
#endif
