// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>
// Description: Kaleidoscopic tunnel effect.

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

float fetch_bar(int idx, int bc) {
    if (bc <= 0) {
        return 0.0;
    }
    idx = idx % bc;
    if (idx < 0) {
        idx += bc;
    }
    return clamp(bars[idx], 0.0, 1.0);
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

    float i0 = 0.0;
    float i1 = floor(float(bc) * 0.125);
    float i2 = floor(float(bc) * 0.25);

    float i3 = floor(float(bc) * 0.33);
    float i4 = floor(float(bc) * 0.50);
    float i5 = floor(float(bc) * 0.66);

    float i6 = floor(float(bc) * 0.75);
    float i7 = floor(float(bc) * 0.875);
    float i8 = float(bc - 1);

    float bass = (fetch_bar(int(i0), bc) + fetch_bar(int(i1), bc) + fetch_bar(int(i2), bc)) / 3.0;
    float mid = (fetch_bar(int(i3), bc) + fetch_bar(int(i4), bc) + fetch_bar(int(i5), bc)) / 3.0;
    float treb = (fetch_bar(int(i6), bc) + fetch_bar(int(i7), bc) + fetch_bar(int(i8), bc)) / 3.0;

    bass = bass * (1.0 + 0.8 * (1.0 - bass));
    mid = mid * (1.0 + 0.8 * (1.0 - mid));
    treb = treb * (1.0 + 0.8 * (1.0 - treb));

    float r = length(p);
    float theta = atan(p.y, p.x);

    float vign = 1.0 - smoothstep(0.70, 1.03, r);
    if (vign <= 0.0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float n = 6.0 + floor(6.0 * clamp(mid, 0.0, 1.0));
    float wedge = TAU / n;

    float a = mod(theta + shader_time * (0.10 + 0.25 * treb), wedge);
    a = abs(a - wedge * 0.5);

    float invr = 1.0 / max(r, 0.03);
    float z = invr * (0.22 + 0.45 * bass);

    float swirl = 0.18 * sin(z * 2.0 + shader_time * 0.6);
    float z2 = z + swirl + (a / max(wedge, 1e-6)) * (0.25 + 0.35 * mid);

    float ring_phase = z2 * (2.4 + 3.8 * bass) + shader_time * (0.7 + 0.9 * treb);
    float ring_cell = fract(ring_phase);
    float ring_d = abs(ring_cell - 0.5);
    float ring_w = max(fwidth(ring_phase), 1e-6);

    float ring = 1.0 - smoothstep(0.06 - ring_w, 0.06 + ring_w, ring_d);
    float ring_glow = 1.0 - smoothstep(0.22 - ring_w * 2.0, 0.22 + ring_w * 2.0, ring_d);

    float edge_d = a;
    float edge_w = max(fwidth(a), 1e-6);
    float edges = 1.0 - smoothstep(0.0, 0.010 + edge_w * 2.0, edge_d);

    float spokes = pow(clamp(edges, 0.0, 1.0), 0.45);

    float alpha = clamp(0.85 * ring + 0.55 * ring_glow + 0.45 * spokes, 0.0, 1.0);
    alpha *= vign;
    alpha *= smoothstep(0.0, 0.0025, alpha);

    if (alpha < 1e-5) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float cval = clamp(0.25 + 0.75 * ring_cell, 0.0, 1.0);
    vec3 col = gradient_map(cval);
    col = mix(col, vec3(1.0), 0.12 * treb);

    fragColor = vec4(mix(bg_color, col, alpha), 1.0);
}
