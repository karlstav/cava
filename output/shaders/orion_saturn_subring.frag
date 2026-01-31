// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>

// Orion Saturn subring

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

void main() {
    vec2 p = fragCoord - vec2(0.5);
    p.x *= u_resolution.x / u_resolution.y;

    float base_radius = 0.35;
    float max_len = 0.15;
    float pad = 2.0 / u_resolution.y;

    float max_r = base_radius + max_len + pad;

    float r2 = dot(p, p);

    if (r2 > max_r * max_r) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    int bc = min(bars_count, 512);
    if (bc <= 0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float r = sqrt(r2);

    float pi = radians(180.0);
    float tau = pi * 2.0;

    float theta = atan(p.y, p.x);
    float a = fract((theta + pi) / tau);

    float cell = a * float(bc);
    int bar = int(floor(cell));
    bar = clamp(bar, 0, bc - 1);
    int bar_next = bar + 1;
    if (bar_next >= bc) {
        bar_next = 0;
    }
    float f = fract(cell);

    float fill = float(bar_width) / max(float(bar_width + bar_spacing), 1.0);
    float angular = abs(f - 0.5);
    float px = max(length(dFdx(p)), length(dFdy(p)));
    float df = 0.35 * (float(bc) * px) / (tau * max(r, px));
    float gap_half = (1.0 - fill) * 0.5;
    float eps = 1.0 / (float(bc) * 2048.0);
    float gap_cap = max(gap_half - eps, 0.0);
    float df_cap = min(gap_cap, fill * 0.15);
    df = min(df, max(df_cap, 1e-6));
    float angular_alpha = 1.0 - smoothstep(fill * 0.5 - df, fill * 0.5 + df, angular);
    angular_alpha *= step(angular, fill * 0.5 + df);
    angular_alpha *= step(0.01, angular_alpha);

    float y0 = clamp(bars[bar], 0.0, 1.0);
    float y1 = clamp(bars[bar_next], 0.0, 1.0);
    float y = mix(y0, y1, f);
    float amp = y * (1.0 + 0.8 * (1.0 - y));

    float min_len = 1.0 / u_resolution.y;
    float max_len_cap = max(max_len - min_len, min_len);
    float len = min(max(amp * max_len, min_len), max_len_cap);
    float act = smoothstep(0.0, min_len / max_len, amp);

    float dr = clamp(px, min_len, 2.0 * min_len);
    float inner = smoothstep(base_radius - dr, base_radius + dr, r);
    float outer = 1.0 - smoothstep(base_radius + len - dr, base_radius + len + dr, r);
    float radial_alpha = inner * outer * act;
    float outer_cap = 1.0 - smoothstep(base_radius + max_len - dr, base_radius + max_len + dr, r);
    radial_alpha *= outer_cap;

    float ring_alpha = angular_alpha * radial_alpha;
    ring_alpha *= step(0.0035, ring_alpha);

    float core_energy = 0.0;
    int core_samples = 0;

    int core_limit = min(bc, 64);
    for (int i = 0; i < core_limit; i += 4) {
        core_energy += clamp(bars[i], 0.0, 1.0);
        core_samples++;
    }
    core_energy /= max(float(core_samples), 1.0);

    float core_amp = core_energy * (1.0 + 0.8 * (1.0 - core_energy));
    float core_radius = mix(0.05, 0.18, clamp(core_amp * 1.2, 0.0, 1.0));

    float core_act = smoothstep(0.0, 0.04, core_amp);
    float core_half_thickness = 0.007;

    float core = smoothstep(core_radius - core_half_thickness - dr,
                            core_radius - core_half_thickness + dr, r) -
                 smoothstep(core_radius + core_half_thickness - dr,
                            core_radius + core_half_thickness + dr, r);
    float core_alpha = clamp(core, 0.0, 1.0) * core_act;

    if (ring_alpha == 0.0 && core_alpha == 0.0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    vec3 col_ring = gradient_map(amp);
    vec3 col_core = gradient_map(core_amp);

    vec3 col = mix(bg_color, col_ring, ring_alpha);
    col = mix(col, col_core, core_alpha);
    fragColor = vec4(col, 1.0);
}
