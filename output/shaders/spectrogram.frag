#version 330

in vec2 fragCoord;
out vec4 fragColor;

// bar values. defaults to left channels first (low to high), then right (high
// to low).
uniform float bars[512];

uniform int bars_count;  // number of bars (left + right) (configurable)
uniform int bar_width;   // bar width (configurable), not used here
uniform int bar_spacing; // space between bars (configurable)

uniform vec3 u_resolution; // window resolution

// colors, configurable in cava config file (r,g,b) (0.0 - 1.0)
uniform vec3 bg_color; // background color
uniform vec3 fg_color; // foreground color

uniform int gradient_count;
uniform sampler1D gradientTexture; // gradient colors

uniform sampler2D inputTexture; // Texture from the last render pass

void main() {
    // find which bar to use based on where we are on the y axis
    int bar = int(bars_count * fragCoord.y);
    float y = bars[bar];
    float band_size = 1.0 / float(bars_count);
    float current_band_min = bar * band_size;
    float current_band_max = (bar + 1) * band_size;

    int hist_length = 512;
    float win_size = 1.0 / hist_length;

    if (fragCoord.x > 1.0 - win_size) {

        if (fragCoord.y > current_band_min && fragCoord.y < current_band_max) {
            vec3 col = (gradient_count > 0) ? texture(gradientTexture, y).rgb : fg_color;
            fragColor = vec4(col * y, 1.0);
        }
    } else {
        vec2 offsetCoord = fragCoord;
        offsetCoord.x += float(win_size);
        fragColor = texture(inputTexture, offsetCoord);
    }
}
