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

vec2 fetch_phase(int idx, int bc) {
    if (bc <= 0) {
        return vec2(0.0);
    }
    idx = idx % bc;
    if (idx < 0) {
        idx += bc;
    }
    return phase_xy[idx];
}

vec2 safe_normalize(vec2 v) {
    float n = length(v);
    if (n < 1e-4) {
        return vec2(0.0);
    }
    return v / n;
}

float phase_lock(vec2 a, vec2 b) {
    float na = length(a);
    float nb = length(b);
    if (na < 1e-4 || nb < 1e-4) {
        return 0.0;
    }
    return clamp(dot(a / na, b / nb), -1.0, 1.0);
}

void main() {
    if (bars_count <= 0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    vec2 uv = fragCoord;
    uv.y = 1.0 - uv.y;

    float bc = float(bars_count);
    float x = clamp(uv.x, 0.0, 1.0) * (bc - 1.0);
    int i = int(floor(x));
    int i1 = min(i + 1, bars_count - 1);
    float f = fract(x);

    float a0 = fetch_bar(i, bars_count);
    float a1 = fetch_bar(i1, bars_count);
    float amp = mix(a0, a1, f);

    vec2 p0 = fetch_phase(i, bars_count);
    vec2 p1 = fetch_phase(i1, bars_count);
    vec2 ph = safe_normalize(mix(p0, p1, f));

    // Neighbor phase alignment score (0..1)
    vec2 pL = fetch_phase(i - 1, bars_count);
    vec2 pR = fetch_phase(i + 1, bars_count);
    float lockL = 0.5 + 0.5 * phase_lock(ph, pL);
    float lockR = 0.5 + 0.5 * phase_lock(ph, pR);
    float lock_local = 0.5 * (lockL + lockR);

    // Harmonic phase alignment: compare with ~2x and ~3x bands
    int h2 = int(clamp(float(i) * 2.0, 0.0, bc - 1.0));
    int h3 = int(clamp(float(i) * 3.0, 0.0, bc - 1.0));
    float lock2 = 0.5 + 0.5 * phase_lock(ph, fetch_phase(h2, bars_count));
    float lock3 = 0.5 + 0.5 * phase_lock(ph, fetch_phase(h3, bars_count));
    float lock_harm = 0.5 * (lock2 + lock3);

    float coherence = clamp(0.55 * lock_local + 0.45 * lock_harm, 0.0, 1.0);

    // A stable animated scan (doesn't require coherence to animate)
    float t = shader_time;
    float scan = 0.5 + 0.5 * sin(6.28318530718 * (uv.x * 0.6 + 0.08 * t));

    // Field brightness: combine amplitude and coherence
    float field = amp * (0.35 + 0.65 * coherence);

    // Soft line based on bar height (y is already flipped)
    float w = max(1.5 / u_resolution.y, 0.0015);
    float line = smoothstep(uv.y - w, uv.y + w, field);

    // Add thin coherence highlight ribbon around the field height
    float ribbon = exp(-pow((uv.y - field) / (2.5 * w), 2.0));
    float highlight = ribbon * coherence * (0.35 + 0.65 * scan);

    vec3 base = gradient_map(clamp(field, 0.0, 1.0));
    vec3 col = mix(bg_color, base, line);
    col += highlight * (0.65 * base + 0.35 * fg_color);

    // Mild vignette
    vec2 p = fragCoord - vec2(0.5);
    float r2 = dot(p, p);
    float vig = smoothstep(0.85, 0.15, r2);
    col = mix(bg_color, col, vig);

    fragColor = vec4(clamp(col, 0.0, 1.0), 1.0);
}
