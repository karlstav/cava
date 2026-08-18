#version 330

// Emulate the "normal" style spectrum analyzer from Winamp 2:
// 16-level pixel grid, green-yellow-red gradient by Y, falling peak dots.

/*
[general]
bars = 76
bar_width = 8
bar_spacing = 2
higher_cutoff_freq = 22000

[output]
method = sdl_glsl
channels = mono
fragment_shader = winamp_spectrum.frag
continuous_rendering = 1
sdl_width = 1024
sdl_height = 256

[color]
background = '#000000'
gradient = 1
gradient_color_1 = '#00E800'
gradient_color_2 = '#00E800'
gradient_color_3 = '#FCF800'
gradient_color_4 = '#FCF800'
gradient_color_5 = '#FC9800'
gradient_color_6 = '#FC0000'

[smoothing]
noise_reduction = 20
*/

in vec2 fragCoord;
out vec4 fragColor;

uniform float bars[512];

uniform int bars_count;
uniform int bar_width;
uniform int bar_spacing;

uniform vec3 u_resolution;

uniform vec3 bg_color;
uniform vec3 fg_color;

uniform int gradient_count;
uniform vec3 gradient_colors[8];

uniform float shader_time;
uniform sampler2D inputTexture;

const float LEVELS = 16.0;
const float PEAK_FALL = 0.025;

vec3 normalize_C(float y, vec3 col_1, vec3 col_2, float y_min, float y_max)
{
    float yr = (y - y_min) / (y_max - y_min);
    return col_1 * (1.0 - yr) + col_2 * yr;
}

vec3 winamp_palette(float y)
{
    // Classic viscolor.txt analyzer bands (bottom to top): 7 green, 5 yellow, 2 orange, 2 red
    if (y < 7.0 / 16.0)
        return vec3(0.0, 0.910, 0.0);
    if (y < 12.0 / 16.0)
        return vec3(0.973, 0.973, 0.0);
    if (y < 14.0 / 16.0)
        return vec3(0.973, 0.596, 0.0);
    return vec3(0.973, 0.0, 0.0);
}

vec3 color_at(float y)
{
    if (gradient_count <= 0)
        return winamp_palette(y);
    if (gradient_count == 1)
        return gradient_colors[0];

    int color = int((gradient_count - 1) * y);
    if (color > gradient_count - 2)
        color = gradient_count - 2;

    float y_min = float(color) / (gradient_count - 1.0);
    float y_max = float(color + 1) / (gradient_count - 1.0);

    return normalize_C(y, gradient_colors[color], gradient_colors[color + 1], y_min, y_max);
}

void main()
{
    int bar = int(float(bars_count) * fragCoord.x);
    if (bar > bars_count - 1)
        bar = bars_count - 1;
    if (bar < 0)
        bar = 0;

    float bar_size = u_resolution.x / float(bars_count);
    float x = u_resolution.x * fragCoord.x;

    float y = min(bars[bar], 1.0);
    y = floor(y * LEVELS) / LEVELS;

    float prevPeak = 0.0;
    if (shader_time > 0.05)
    {
        vec2 peakUV = vec2((float(bar) + 0.5) / float(bars_count), 0.5);
        prevPeak = clamp(texture(inputTexture, peakUV).a, 0.0, 1.0);
    }
    float peak = max(y, prevPeak - PEAK_FALL);

    float fy = floor(fragCoord.y * LEVELS) / LEVELS;
    float peak_q = floor(peak * LEVELS) / LEVELS;
    float level = 1.0 / LEVELS;

    vec3 rgb = bg_color;

    if (x <= (float(bar) + 1.0) * bar_size - float(bar_spacing))
    {
        bool on_peak = peak_q > 0.0 && fragCoord.y <= peak_q && fragCoord.y > peak_q - level;
        if (on_peak)
        {
            rgb = color_at(peak_q - level * 0.5);
        }
        else if (fy < y)
        {
            rgb = color_at(fy + level * 0.5);
        }
    }

    fragColor = vec4(rgb, peak);
}
