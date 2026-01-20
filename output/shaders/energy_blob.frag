// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>

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

    float bass = fetch_bar(0, bc);
    bass = bass * (1.0 + 0.8 * (1.0 - bass));

    float spin = shader_time * 0.12;

    float field = 0.0;

    float nucleus = exp(-dot(p, p) * (18.0 + 30.0 * bass));
    field += 1.2 * nucleus;

    int points = 28;
    float points_f = float(points);

    for (int i = 0; i < 28; i++) {
        float fi = float(i);
        int bi = int(floor(fi / points_f * float(bc)));
        float a = fetch_bar(bi, bc);
        a = a * (1.0 + 0.8 * (1.0 - a));

        float ang = TAU * (fi / points_f) + spin;
        float wob = 0.08 * sin(ang * 1.7 + shader_time * 0.35);
        float rad = 0.22 + 0.16 * a + wob;

        vec2 c = vec2(cos(ang), sin(ang)) * rad;
        vec2 d = p - c;

        float k = 26.0 + 18.0 * a;
        float blob = exp(-dot(d, d) * k);
        field += (0.45 + 0.85 * a) * blob;
    }

    float vign = 1.0 - smoothstep(0.55, 0.92, length(p));
    field *= vign;

    float w = max(fwidth(field), 0.01);

    float thr = 0.62;
    float core = smoothstep(thr - w, thr + w, field);
    float glow = smoothstep(0.10, 0.55, field) * (1.0 - core);

    float alpha = clamp(core + 0.35 * glow, 0.0, 1.0);
    alpha *= smoothstep(0.0, 0.0025, alpha);

    if (alpha < 1e-5) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float cval = clamp(field, 0.0, 1.0);
    vec3 col = gradient_map(cval);

    fragColor = vec4(mix(bg_color, col, alpha), 1.0);
}
