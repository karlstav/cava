// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>
// Description: Spectral interference waves.

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

float smooth_bars(float x, int bc) {
    float y = 0.0;
    float wsum = 0.0;
    for (int i = -2; i <= 2; i++) {
        float d = float(i);
        float w = 1.0 / (1.0 + d * d * 1.5);
        y += sample_bars(x + d, bc) * w;
        wsum += w;
    }
    return y / max(wsum, 1e-6);
}

void main() {
    vec2 p = fragCoord - vec2(0.5);
    p.x *= u_resolution.x / u_resolution.y;

    const float PI = 3.14159265358979323846;
    const float TAU = 6.28318530717958647692;

    int bc = min(bars_count, 512);
    if (bc <= 0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float r = length(p);
    float theta = atan(p.y, p.x);
    float a = fract((theta + PI) / TAU);

    float cell = a * float(bc);
    float a0 = smooth_bars(cell, bc);
    float a1 = smooth_bars(cell + 3.2, bc);
    float a2 = smooth_bars(cell - 4.7, bc);

    a0 = a0 * (1.0 + 0.8 * (1.0 - a0));
    a1 = a1 * (1.0 + 0.8 * (1.0 - a1));
    a2 = a2 * (1.0 + 0.8 * (1.0 - a2));

    float amp = clamp(0.25 + 0.75 * max(a0, max(a1, a2)), 0.0, 1.0);

    float k1 = 18.0 + 26.0 * (0.15 + 0.85 * a0);
    float k2 = 16.0 + 24.0 * (0.15 + 0.85 * a1);

    float t = shader_time;

    float phi1 = k1 * r + t * (1.30 + 0.90 * a0) + 1.4 * sin(theta * 2.0);
    float phi2 = k2 * r - t * (1.05 + 0.85 * a1) + 1.2 * cos(theta * 3.0);

    float beat = 0.5 + 0.5 * cos(phi1 - phi2);
    float carrier = 0.5 + 0.5 * cos(phi1 + phi2);

    float inter = 0.55 * beat + 0.45 * carrier;
    inter = pow(clamp(inter, 0.0, 1.0), 1.25);

    float focus = 1.0 - smoothstep(0.58, 0.98, r);
    float hole = smoothstep(0.02, 0.06, r);

    float v = inter * amp * focus * hole;

    float w = max(fwidth(v), 1.0 / u_resolution.y);
    float alpha = smoothstep(0.10 - w, 0.48 + w, v);
    alpha *= smoothstep(0.0, 0.003, alpha);

    if (alpha < 1e-5) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    vec3 col = gradient_map(clamp(0.10 + 0.90 * v, 0.0, 1.0));

    float g = smoothstep(0.15, 0.85, v);
    col = mix(col, vec3(1.0), 0.08 * g);

    fragColor = vec4(mix(bg_color, col, alpha), 1.0);
}
