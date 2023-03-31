#version 330

in vec2 fragCoord;
out vec4 fragColor;

// bar values. defaults to left channels first (low to high), then right (high to low).
uniform float bars[512];

uniform int bars_count;    // number of bars (left + right) (configurable)
uniform int bar_width;    // bar width (configurable), not used here
uniform int bar_spacing;    // space bewteen bars (configurable)

uniform vec3 u_resolution; // window resolution

//colors, configurable in cava config file (r,g,b) (0.0 - 1.0)
uniform vec3 bg_color; // background color
uniform vec3 fg_color; // foreground color

uniform int gradient_count;
uniform vec3 gradient_colors[8]; // gradient colors

vec3 normalize_C(float y,vec3 col_1, vec3 col_2, float y_min, float y_max)
{
    float yr = (y - y_min) / (y_max - y_min);
    return col_1 * (1.0 - yr) + col_2 * yr;
}

void main()
{
    // find which bar to use based on where we are on the x axis
    float x = u_resolution.x * fragCoord.x;
    int bar = int(bars_count * fragCoord.x);
    float bar_size = u_resolution.x / bars_count;


    float y =  bars[bar];
    if (y > fragCoord.y)
    {

        if (x > (bar + 1) * (bar_size) - bar_spacing)
        {
            fragColor = vec4(bg_color,1.0);
        }
        else
        {
            if (gradient_count == 0)
            {
                fragColor = vec4(fg_color,1.0);
            }
            else
            {
                int color = int((gradient_count - 1) * fragCoord.y);
                float y_min = color / (gradient_count - 1.0);
                float y_max = (color + 1.0) / (gradient_count - 1.0);
                fragColor = vec4(normalize_C(fragCoord.y, gradient_colors[color], gradient_colors[color + 1], y_min, y_max), 1.0);
            }
        }
    }
    else
    {
        fragColor = vec4(bg_color,1.0);
    }
}