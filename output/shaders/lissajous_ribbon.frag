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

float sd_segment(vec2 p, vec2 a, vec2 b) {
    vec2 pa = p - a;
    vec2 ba = b - a;
    float h = clamp(dot(pa, ba) / max(dot(ba, ba), 1e-6), 0.0, 1.0);
    return length(pa - ba * h);
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
    float i1 = floor(float(bc) * 0.10);
    float i2 = floor(float(bc) * 0.22);

    float i3 = floor(float(bc) * 0.33);
    float i4 = floor(float(bc) * 0.50);
    float i5 = floor(float(bc) * 0.66);

    float i6 = floor(float(bc) * 0.78);
    float i7 = floor(float(bc) * 0.90);
    float i8 = float(bc - 1);

    float bass = (fetch_bar(int(i0), bc) + fetch_bar(int(i1), bc) + fetch_bar(int(i2), bc)) / 3.0;
    float mid = (fetch_bar(int(i3), bc) + fetch_bar(int(i4), bc) + fetch_bar(int(i5), bc)) / 3.0;
    float treb = (fetch_bar(int(i6), bc) + fetch_bar(int(i7), bc) + fetch_bar(int(i8), bc)) / 3.0;

    bass = bass * (1.0 + 0.8 * (1.0 - bass));
    mid = mid * (1.0 + 0.8 * (1.0 - mid));
    treb = treb * (1.0 + 0.8 * (1.0 - treb));

    float ax = 2.0 + floor(3.0 * bass + 1.0);
    float by = 3.0 + floor(4.0 * treb);

    float phase = PI * (0.2 + 0.8 * mid);
    float speed = 0.55 + 0.65 * treb;
    float t0 = shader_time * speed;

    float rot = 0.25 * sin(shader_time * 0.35 + mid * 2.0);
    float cr = cos(rot);
    float sr = sin(rot);
    mat2 R = mat2(cr, -sr, sr, cr);

    float scale = 0.34 + 0.06 * bass;

    vec2 prev = vec2(sin(ax * 0.0 + t0), sin(by * 0.0 + t0 + phase));
    prev *= scale;
    prev = R * prev;

    float dmin = 1e6;
    float tmin = 0.0;

    const int N = 64;
    for (int i = 1; i < N; i++) {
        float u = float(i) / float(N - 1);
        float t = u * TAU;
        vec2 cur = vec2(sin(ax * t + t0), sin(by * t + t0 + phase));
        cur *= scale;
        cur = R * cur;

        float d = sd_segment(p, prev, cur);
        if (d < dmin) {
            dmin = d;
            tmin = u;
        }

        prev = cur;
    }

    float vign = 1.0 - smoothstep(0.62, 0.98, length(p));
    if (vign <= 0.0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float thick = 0.010 + 0.020 * bass + 0.012 * mid;
    float w = max(fwidth(dmin), 1.0 / u_resolution.y);

    float core = 1.0 - smoothstep(thick - w, thick + w, dmin);
    float glow = exp(-dmin * dmin / max(thick * thick * 14.0, 1e-6));

    float alpha = clamp(core + 0.45 * glow, 0.0, 1.0);
    alpha *= vign;
    alpha *= smoothstep(0.0, 0.0025, alpha);

    if (alpha < 1e-5) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    vec3 col = gradient_map(clamp(tmin, 0.0, 1.0));
    col = mix(col, vec3(1.0), 0.10 * treb);

    fragColor = vec4(mix(bg_color, col, alpha), 1.0);
}
