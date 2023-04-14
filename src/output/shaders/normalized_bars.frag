#version 330

in vec2 fragCoord;
out vec4 fragColor;

// bar values. defaults to left channels first (low to high), then right (high to low).
uniform float bars[512];

uniform int bars_count;    // number of bars (left + right) (configurable)

uniform vec3 u_resolution; // window resolution, not used here

//colors, configurable in cava config file
uniform vec3 bg_color; // background color(r,g,b) (0.0 - 1.0), not used here
uniform vec3 fg_color; // foreground color, not used here

float normalize_C(float x, float x_min, float x_max, float r_min, float r_max )
{
	float xr;
	xr = (r_max-r_min) * (x - x_min) / (x_max - x_min) + r_min;
	return xr;
}

void main()
{
    // find which bar to use based on where we are on the x axis
    int bar = int(bars_count * fragCoord.x);

    // create a normal along the y axis based on the bar height
    float x = normalize_C(fragCoord.y, 1.0, 0.0, 0.0, bars[bar]);

    // set color
    fragColor.r=fg_color.x*x;
    fragColor.g=fg_color.y*x;
    fragColor.b=fg_color.z*x;
    fragColor.a=1.0;

}
