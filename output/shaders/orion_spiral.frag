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

    float a = fract((theta + PI) / TAU);

    int bc = min(bars_count, 512);
    if (bc <= 0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float phase = fract(shader_time * 0.04);
    float a_sample = fract(a + phase);

    float cell = a_sample * float(bc);
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
    float df = 0.35 * (float(bc) * px) / (TAU * max(r, px));
    float gap_half = (1.0 - fill) * 0.5;
    float eps = 1.0 / (float(bc) * 2048.0);
    float gap_cap = max(gap_half - eps, 0.0);
    float df_cap = min(gap_cap, fill * 0.15);
    df = min(df, max(df_cap, 1e-6));

    float angular_alpha = 1.0 - smoothstep(fill * 0.5 - df, fill * 0.5 + df, angular);
    angular_alpha *= smoothstep(0.0, 0.01, angular_alpha);

    float y0 = clamp(bars[bar], 0.0, 1.0);
    float y1 = clamp(bars[bar_next], 0.0, 1.0);
    float y = mix(y0, y1, f);

    float amp = y * (1.0 + 0.8 * (1.0 - y));

    float min_len = 1.0 / u_resolution.y;
    float th_base = max(1.6 * min_len, 0.0015);
    float th = th_base + amp * (3.5 * min_len);

    float inner = 0.10;
    float outer = 0.48;
    float turns = 3.0;
    float pitch = (outer - inner) / max(turns, 1.0);

    float geom_phase = fract(shader_time * 0.02);
    float a_geom = fract(a + geom_phase);
    float a_turn = a_geom * turns;

    float r_base = inner + a_turn * pitch;
    float disp = amp * (0.85 * pitch);
    float r_center = r_base + disp;

    float pad = 2.0 / u_resolution.y;
    float min_r = max(inner - pad, 0.0);
    float max_r = outer + 0.85 * pitch + pad;
    if (r < min_r || r > max_r) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float dr = clamp(px, min_len, 2.5 * min_len);
    float d = abs(r - r_center);
    float radial_alpha = 1.0 - smoothstep(th - dr, th + dr, d);

    float alpha = angular_alpha * radial_alpha;
    alpha *= smoothstep(0.0, 0.0035, alpha);

    if (alpha < 1e-5) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    vec3 col = gradient_map(amp);
    fragColor = vec4(mix(bg_color, col, alpha), 1.0);
}
