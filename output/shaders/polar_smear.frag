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

    float r = length(p);
    float theta = atan(p.y, p.x);

    int bc = min(bars_count, 512);
    if (bc <= 0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float a = fract((theta + PI) / TAU);

    float speed = 0.03;
    float trail_len = 0.12;

    float cell0 = fract(a + shader_time * speed) * float(bc);
    float y0 = smooth_bars(cell0, bc);

    float acc = 0.0;
    float wsum = 0.0;

    int taps = 10;
    for (int i = 1; i < 10; i++) {
        float t = float(i) / float(taps - 1);
        float w = exp(-t * t * 6.0);
        float cell = fract(a + shader_time * speed - t * trail_len) * float(bc);
        acc += smooth_bars(cell, bc) * w;
        wsum += w;
    }

    float y = max(y0, 0.85 * acc / max(wsum, 1e-6));
    float amp = y * (1.0 + 0.8 * (1.0 - y));

    float inner = 0.05;
    float span = 0.48;

    float pad = 3.0 / u_resolution.y;
    if (r < inner - pad || r > inner + span + pad) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float t = (r - inner) / max(span, 1e-6);

    float w = max(fwidth(t), 1.0 / u_resolution.y);
    float fill_alpha = 1.0 - smoothstep(amp - w, amp + w, t);

    float fill = float(bar_width) / max(float(bar_width + bar_spacing), 1.0);
    float cell_f = fract(cell0);
    float angular = abs(cell_f - 0.5);

    float df = max(fwidth(cell0), 1e-6);
    float angular_alpha = 1.0 - smoothstep(fill * 0.5 - df, fill * 0.5 + df, angular);
    angular_alpha = max(angular_alpha, 0.35);

    float alpha = fill_alpha * angular_alpha;
    alpha *= smoothstep(0.0, 0.0035, alpha);

    if (alpha < 1e-5) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    vec3 col = gradient_map(clamp(t, 0.0, 1.0));

    float head = smoothstep(0.55, 1.0, y0);
    col = mix(col, vec3(1.0), 0.12 * head);

    fragColor = vec4(mix(bg_color, col, alpha), 1.0);
}
