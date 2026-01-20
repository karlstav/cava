// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>

#version 330

in vec2 fragCoord;
out vec4 fragColor;

uniform float bars[512];

uniform vec2 phase_xy[512];

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

vec2 safe_norm(vec2 v) {
    float m2 = dot(v, v);
    if (m2 < 1e-10) {
        return vec2(0.0);
    }
    return v * inversesqrt(m2);
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

float gaussian(float x, float sigma) {
    float s = max(sigma, 1e-6);
    return exp(-(x * x) / (2.0 * s * s));
}

void main() {
    vec2 uv = fragCoord;
    vec2 px = 1.0 / max(u_resolution.xy, vec2(1.0));

    int bc = min(bars_count, 512);
    if (bc <= 0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float x = uv.x * float(bc);

    float y = 0.0;
    float wsum = 0.0;
    for (int i = -1; i <= 1; i++) {
        float d = float(i);
        float w = 1.0 / (1.0 + d * d * 1.2);
        y += sample_bars(x + d, bc) * w;
        wsum += w;
    }
    y /= max(wsum, 1e-6);

    float amp = clamp(y, 0.0, 1.0);
    amp = amp * (1.0 + 0.8 * (1.0 - amp));

    int bi = int(floor(x));
    bi = clamp(bi, 0, bc - 1);

    float coh = clamp(length(phase_xy[bi]), 0.0, 1.0);
    vec2 ph = safe_norm(phase_xy[bi]);

    float base_flow = 0.0010 + 0.0024 * amp;
    vec2 drift = vec2(-base_flow, 0.0);
    drift += 0.0032 * ph * (0.15 + 0.85 * coh) * (0.20 + 0.80 * amp);
    drift += 0.0011 * vec2(sin(shader_time * 0.37 + uv.y * 6.0),
                           cos(shader_time * 0.41 + uv.x * 7.0));

    vec2 fb_uv = clamp(uv + drift, vec2(0.0), vec2(1.0));

    vec3 fb = texture(inputTexture, fb_uv).rgb;
    fb += 0.25 * texture(inputTexture, clamp(fb_uv + vec2(px.x, 0.0), vec2(0.0), vec2(1.0))).rgb;
    fb += 0.25 * texture(inputTexture, clamp(fb_uv - vec2(px.x, 0.0), vec2(0.0), vec2(1.0))).rgb;
    fb += 0.20 * texture(inputTexture, clamp(fb_uv + vec2(0.0, px.y), vec2(0.0), vec2(1.0))).rgb;
    fb += 0.20 * texture(inputTexture, clamp(fb_uv - vec2(0.0, px.y), vec2(0.0), vec2(1.0))).rgb;
    fb /= (1.0 + 0.25 + 0.25 + 0.20 + 0.20);

    float decay = mix(0.962, 0.988, smoothstep(0.03, 0.35, amp));
    fb = mix(bg_color, fb, decay);

    float cx = (floor(x) + 0.5) / float(bc);
    float sigma_x = mix(0.010, 0.0045, coh);
    float gx = gaussian(uv.x - cx, sigma_x);

    float y_target = 0.06 + amp * 0.88;
    float sigma_y = mix(0.060, 0.020, amp);
    float gy = gaussian(uv.y - y_target, sigma_y);

    float tail = smoothstep(y_target + 0.14, y_target - 0.02, uv.y);
    float wash = tail * exp(-abs(uv.x - cx) / max(0.02, 0.02 + 0.06 * (1.0 - coh)));

    float injection = 0.0;
    injection += 0.80 * gx * gy;
    injection += 0.35 * wash * amp;
    injection *= smoothstep(0.0, 0.03, amp);
    injection *= mix(0.55, 1.20, coh);
    injection = 1.0 - exp(-injection * 1.4);

    vec3 col = gradient_map(clamp(0.15 + 0.85 * amp, 0.0, 1.0));
    col = mix(col, vec3(1.0), 0.08 * coh);

    vec3 out_col = fb + col * injection;

    out_col = out_col / (vec3(1.0) + out_col);
    out_col = clamp(out_col, vec3(0.0), vec3(1.0));
    fragColor = vec4(out_col, 1.0);
}
