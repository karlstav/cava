// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>
// Description: Spectral mass / gaussian glow.

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

float gaussian2(vec2 d, vec2 sigma) {
    vec2 s = max(sigma, vec2(1e-4));
    vec2 z = (d * d) / (2.0 * s * s);
    return exp(-(z.x + z.y));
}

void main() {
    int bc = min(bars_count, 512);
    if (bc <= 0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    vec2 uv = fragCoord;

    float sum = 0.0;
    float sum_i = 0.0;
    float sum_i2 = 0.0;

    for (int i = 0; i < 512; i++) {
        if (i >= bc) {
            break;
        }
        float a = clamp(bars[i], 0.0, 1.0);
        a = a * a;
        float x = float(i) + 0.5;
        sum += a;
        sum_i += a * x;
        sum_i2 += a * x * x;
    }

    float cx = 0.5 * float(bc);
    float sigma_x = 0.25;
    float energy = 0.0;

    if (sum > 1e-6) {
        cx = sum_i / sum;
        float v = max(sum_i2 / sum - cx * cx, 1.0);
        sigma_x = clamp(sqrt(v) / float(bc), 0.06, 0.40);
        energy = clamp(sum / float(bc), 0.0, 1.0);
    }

    float cx_n = cx / float(bc);

    float local = sample_bars(uv.x * float(bc), bc);
    local = local * (1.0 + 0.8 * (1.0 - local));

    float cy = 0.18 + 0.20 * energy;

    float wobble = 0.015 * sin(shader_time * 0.7 + uv.x * 6.0) + 0.010 * cos(shader_time * 0.5 + uv.y * 5.0);

    vec2 center = vec2(cx_n, cy + wobble);
    vec2 sigma = vec2(0.12 + 0.55 * sigma_x, 0.10 + 0.30 * (0.2 + energy));

    float blob = gaussian2(uv - center, sigma);

    float plume_h = 0.20 + 0.65 * local;
    float plume = exp(-pow(max(uv.y - 0.05, 0.0) / max(plume_h, 1e-3), 1.6));

    float plume_w = 0.030 + 0.090 * (0.35 + local);
    float plume_x = exp(-pow(abs(uv.x - (floor(uv.x * float(bc)) + 0.5) / float(bc)) / max(plume_w, 1e-3), 1.8));

    float density = 0.55 * blob + 0.45 * plume * plume_x;
    density *= smoothstep(0.0, 0.02, energy + 0.2 * local);

    float csel = clamp(0.25 * energy + 0.75 * local, 0.0, 1.0);
    vec3 col = gradient_map(csel);

    vec3 out_col = mix(bg_color, col, density);
    out_col = out_col / (vec3(1.0) + out_col);

    fragColor = vec4(out_col, 1.0);
}
