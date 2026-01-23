// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>
// Description: Ring collapse / topology warp.

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

    float sum2 = 0.0;
    float peak = 0.0;
    float low = 0.0;
    float high = 0.0;

    int n = 32;
    for (int i = 0; i < 32; i++) {
        float fi = float(i) / float(n - 1);
        int idx = int(floor(fi * float(bc - 1)));
        float v = fetch_bar(idx, bc);
        sum2 += v * v;
        peak = max(peak, v);

        if (fi < 0.25) {
            low += v;
        }
        if (fi > 0.75) {
            high += v;
        }
    }

    float rms = sqrt(sum2 / float(n));
    low /= float(n) * 0.25;
    high /= float(n) * 0.25;

    float transient = clamp(0.65 * (peak - rms) + 0.55 * (high - low), 0.0, 1.0);
    transient = transient * (1.0 + 0.8 * (1.0 - transient));

    rms = clamp(rms, 0.0, 1.0);
    rms = rms * (1.0 + 0.8 * (1.0 - rms));

    float base_speed = 0.07 + 0.07 * rms + 0.12 * transient;
    float phase = fract(shader_time * base_speed);

    float collapse = smoothstep(0.56, 0.78, phase);
    float reform = smoothstep(0.82, 1.0, phase);
    float collapse_amt = collapse * (1.0 - reform);

    float break_amt = smoothstep(0.30, 0.48, phase) * (1.0 - smoothstep(0.56, 0.68, phase));
    break_amt *= (0.15 + 0.85 * transient);

    float form_amt = smoothstep(0.00, 0.22, phase);
    float life = clamp(form_amt + 0.85 * (1.0 - collapse_amt), 0.0, 1.0);

    float r = length(p);
    float theta = atan(p.y, p.x);

    float base_radius = 0.34 + 0.10 * rms;
    float min_radius = 0.12;
    float radius = mix(base_radius, min_radius, collapse_amt);

    float crack = 0.5 + 0.5 * sin(theta * 11.0 + shader_time * (0.25 + 0.55 * transient)) *
                            sin(theta * 5.0 - shader_time * (0.18 + 0.48 * rms));

    float piece = (crack - 0.5);
    radius += break_amt * 0.04 * piece;

    float thickness = 0.010 + 0.020 * rms;
    thickness *= (0.45 + 0.55 * form_amt);

    float d = abs(r - radius);
    float aa = max(fwidth(d), 1.0 / u_resolution.y);

    float ring = 1.0 - smoothstep(thickness - aa, thickness + aa, d);

    float thr = 0.55 + 0.35 * break_amt;
    float keep = smoothstep(thr - aa * 3.0, thr + aa * 3.0, crack);

    float mask = mix(1.0, keep, break_amt);

    float vign = 1.0 - smoothstep(0.70, 0.98, r);
    float alpha = ring * mask * vign * life;

    alpha *= smoothstep(0.0, 0.003, alpha);

    if (alpha < 1e-5) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float cval = clamp(0.25 + 0.55 * rms + 0.35 * transient + 0.10 * crack, 0.0, 1.0);
    vec3 col = gradient_map(cval);

    float glow = exp(-d * d / max(thickness * thickness * 12.0, 1e-6));
    alpha = clamp(alpha + 0.25 * glow * vign, 0.0, 1.0);

    fragColor = vec4(mix(bg_color, col, alpha), 1.0);
}
