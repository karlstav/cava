// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>
// Description: Void ring / halo.

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

float avg_low(int bc) {
    int n = max(1, bc / 12);
    float s = 0.0;
    for (int i = 0; i < 512; i++) {
        if (i >= n) {
            break;
        }
        s += clamp(bars[i], 0.0, 1.0);
    }
    return s / float(n);
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

    float r = length(p);
    float th = atan(p.y, p.x);

    float bass = avg_low(bc);
    bass = bass * (1.0 + 0.8 * (1.0 - bass));

    float ring_r = 0.58 + 0.03 * sin(shader_time * 0.7);
    float thickness = 0.10 + 0.10 * bass;

    float aa = max(fwidth(r), 1e-5);

    float inner = ring_r - 0.5 * thickness;
    float outer = ring_r + 0.5 * thickness;

    float ring = smoothstep(inner - aa, inner + aa, r) * (1.0 - smoothstep(outer - aa, outer + aa, r));

    float cells = clamp(8.0 + floor(float(bc) / 14.0), 8.0, 28.0);
    float a = (th + PI) / TAU;
    float cell = a * cells;
    float f = fract(cell) - 0.5;

    float spec = sample_bars(cell * float(bc) / cells, bc);
    spec = spec * (1.0 + 0.8 * (1.0 - spec));

    float sigma_ang = mix(0.26, 0.10, spec);
    float hole = exp(-(f * f) / (2.0 * sigma_ang * sigma_ang));

    float hole_depth = (0.25 + 0.75 * bass) * smoothstep(0.0, 0.02, bass);
    float carve = ring * hole * hole_depth;

    float edge = abs(f) / max(fwidth(cell), 1e-5);
    float rim = exp(-edge * 0.7) * ring;

    vec3 ring_col = gradient_map(clamp(0.20 + 0.80 * spec, 0.0, 1.0));
    ring_col *= 0.22 + 0.78 * rim;

    vec3 col = mix(bg_color, ring_col, ring * (1.0 - carve));
    col = mix(col, bg_color, carve);

    fragColor = vec4(col, 1.0);
}
