int draw_terminal_printf(int height, int width, int bars) {
for (n = (height - 1); n >= 0; n--) {
	o = 0;
	move = rest / 2; //center adjustment
	for (i = 0; i < width; i++) {

		// output: check if we're already at the next bar
		if (i != 0 && i % bw == 0) {
			o++;
			if (o < bands)move++;
		}

		// output: draw and move to another one, check whether we're not too far
		if (o < bands) {
			if (f[o] - n < 0.125) { //blank
				if (matrix[i][n] != 0) { //change?
					if (move != 0)printf("\033[%dC", move);
					move = 0;
					printf(" ");
				} else move++; //no change, moving along
				matrix[i][n] = 0;
			} else if (f[o] - n > 1) { //color
				if (matrix[i][n] != 1) { //change?
					if (move != 0)printf("\033[%dC", move);
					move = 0;
					printf("\u2588");
				} else move++; //no change, moving along
				matrix[i][n] = 1;
			} else { //top color, finding fraction
				if (move != 0)printf("\033[%dC", move);
				move = 0;
				c = ((((f[o] - (float)n) - 0.125) / 0.875 * 7) + 1);
				if (0 < c && c < 8) {
					if (virt == 0)printf("%d", c);
					else printf("%lc", L'\u2580' + c);
				} else printf(" ");
				matrix[i][n] = 2;
			}
		}

	}

	printf("\n");

}

printf("\033[%dA", height);
}
