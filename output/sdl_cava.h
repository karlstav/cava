void init_sdl_window(int width, int height, int x, int y);
void init_sdl_surface(int *width, int *height, char *const fg_color_string,
                      char *const bg_color_string);
int draw_sdl(int bars_count, int bar_width, int bar_spacing, int remainder, int height,
             const int bars[], int previous_frame[], int frame_timer);
void cleanup_sdl(void);
