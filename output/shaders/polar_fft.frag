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

void main() {
    vec2 p = fragCoord - vec2(0.5);
    p.x *= u_resolution.x / u_resolution.y;

    const float PI = 3.14159265358979323846;
    const float TAU = 6.28318530717958647692;

    float r2 = dot(p, p);
    if (r2 < 1e-8) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float r = sqrt(r2);
    float theta = atan(p.y, p.x);

    int bc = min(bars_count, 512);
    if (bc <= 0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float a = fract((theta + PI) / TAU);
    float a_rot = fract(a + shader_time * 0.02);

    float cell = a_rot * float(bc);
    float f = fract(cell);

    float y = 0.0;
    float wsum = 0.0;
    for (int i = -2; i <= 2; i++) {
        float d = float(i);
        float w = 1.0 / (1.0 + d * d * 1.5);
        y += sample_bars(cell + d, bc) * w;
        wsum += w;
    }
    y /= max(wsum, 1e-6);

    float amp = y * (1.0 + 0.8 * (1.0 - y));

    float fill = float(bar_width) / max(float(bar_width + bar_spacing), 1.0);
    float angular = abs(f - 0.5);

    float px = max(length(dFdx(p)), length(dFdy(p)));
    float min_len = 1.0 / u_resolution.y;

    float df = max(fwidth(cell), 1e-6);

    float angular_alpha = 1.0 - smoothstep(fill * 0.5 - df, fill * 0.5 + df, angular);
    angular_alpha *= smoothstep(0.0, 0.01, angular_alpha);
    angular_alpha = max(angular_alpha, 0.12);

    float inner = 0.06;
    float span = 0.44;

    float pad = 4.0 * min_len;
    if (r < inner - pad || r > inner + span + pad) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float t = (r - inner) / max(span, 1e-6);

    float w = max(fwidth(t), min_len);
    float fill_alpha = 1.0 - smoothstep(amp - w, amp + w, t);

    float alpha = angular_alpha * fill_alpha;
    alpha *= smoothstep(0.0, 0.0035, alpha);

    if (alpha < 1e-5) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    vec3 col = gradient_map(clamp(t, 0.0, 1.0));
    fragColor = vec4(mix(bg_color, col, alpha), 1.0);
}
