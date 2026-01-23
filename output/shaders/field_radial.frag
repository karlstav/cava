// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>
// Description: Radial field / wave patterns.

#version 330

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

vec3 normalize_C(float y, vec3 col_1, vec3 col_2, float y_min, float y_max) {
    const float EPS = 0.0001;
    float yr = (y - y_min) / max(y_max - y_min, EPS);
    yr = clamp(yr, 0.0, 1.0);
    return col_1 * (1.0 - yr) + col_2 * yr;
}

vec3 gradient_map(float amp) {
    if (gradient_count == 0) {
        return fg_color;
    }

    if (gradient_count == 1) {
        return gradient_colors[0];
    }

    int color = int(floor((gradient_count - 1) * amp));
    color = clamp(color, 0, gradient_count - 2);
    float y_min = float(color) / (gradient_count - 1.0);
    float y_max = float(color + 1) / (gradient_count - 1.0);
    return normalize_C(amp, gradient_colors[color], gradient_colors[color + 1], y_min, y_max);
}

float sample_bars(float x, int bc) {
    float bc_f = float(bc);
    float xw = mod(x, bc_f);
    if (xw < 0.0) {
        xw += bc_f;
    }

    float i = floor(xw);
    int i0 = int(i);
    int i1 = i0 + 1;
    if (i1 >= bc) {
        i1 = 0;
    }

    float f = fract(xw);
    float y0 = clamp(bars[i0], 0.0, 1.0);
    float y1 = clamp(bars[i1], 0.0, 1.0);
    return mix(y0, y1, f);
}

void main() {
    int bc = min(bars_count, 512);
    if (bc <= 0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    vec2 p = fragCoord - vec2(0.5);
    p.x *= u_resolution.x / max(u_resolution.y, 1.0);

    const float PI = 3.14159265358979323846;
    const float TAU = 6.28318530717958647692;

    float r0 = length(p);
    float theta0 = atan(p.y, p.x);
    float a0 = (theta0 + PI) / TAU;
    float idx0 = a0 * float(bc);

    float amp0 = sample_bars(idx0, bc);
    amp0 = amp0 * (1.0 + 0.8 * (1.0 - amp0));

    vec2 q = p;
    float t = shader_time;

    for (int it = 0; it < 3; it++) {
        float th = atan(q.y, q.x);
        float a = (th + PI) / TAU;
        float idx = a * float(bc);
        float amp = sample_bars(idx + float(it) * 3.7, bc);
        amp = amp * (1.0 + 0.8 * (1.0 - amp));

        vec2 swirl = vec2(-q.y, q.x);
        float w = 0.25 + 0.90 * amp;

        float n1 = sin(6.0 * th + 3.0 * t + 5.0 * q.x);
        float n2 = cos(7.0 * th - 2.2 * t + 6.0 * q.y);
        vec2 flow = vec2(n1, n2);

        float fade = smoothstep(1.2, 0.0, length(q));
        q += (0.16 * w) * swirl * fade;
        q += (0.05 + 0.08 * amp) * flow * fade;
    }

    float r = length(q);

    float f = sin(5.5 * q.x + 6.7 * q.y + 0.9 * t) + sin(-6.1 * q.x + 5.2 * q.y - 1.1 * t);
    float df = max(fwidth(f), 1e-5);
    float lines = 1.0 - smoothstep(0.0, 2.2 * df + 0.06, abs(f));

    float core = exp(-4.0 * r);
    float field = (0.20 + 0.80 * core) * (0.25 + 0.75 * lines);
    field *= smoothstep(1.25, 0.35, r0);

    vec3 col = gradient_map(clamp(0.15 + 0.85 * amp0, 0.0, 1.0));
    vec3 out_col = mix(bg_color, col, field);

    fragColor = vec4(out_col, 1.0);
}
