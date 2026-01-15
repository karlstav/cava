// SPDX-License-Identifier: MIT
// Author: rezky_nightky <with.rezky@gmail.com>
// Static Orion (non-rotating)
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

uniform sampler2D inputTexture;

vec3 normalize_C(float y, vec3 col_1, vec3 col_2, float y_min, float y_max) {
    float yr = (y - y_min) / (y_max - y_min);
    return col_1 * (1.0 - yr) + col_2 * yr;
}

void main() {
    vec2 p = fragCoord - vec2(0.5);
    p.x *= u_resolution.x / u_resolution.y;

    float base_radius = 0.35;
    float max_len = 0.15;
    float pad = 2.0 / u_resolution.y;
    float min_r = max(base_radius - pad, 0.0);
    float max_r = base_radius + max_len + pad;

    float r2 = dot(p, p);
    if (r2 < min_r * min_r || r2 > max_r * max_r) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float r = sqrt(r2);

    float theta = atan(p.y, p.x);

    float pi = radians(180.0);
    float tau = pi * 2.0;

    float a = (theta + pi) / tau;
    a = fract(a);

    if (bars_count <= 0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float cell = a * float(bars_count);
    int bar = int(floor(cell));
    bar = clamp(bar, 0, bars_count - 1);
    float f = fract(cell);

    float fill = float(bar_width) / max(float(bar_width + bar_spacing), 1.0);
    float angular = abs(f - 0.5);
    float df = fwidth(cell);
    float angular_alpha = 1.0 - smoothstep(fill * 0.5 - df, fill * 0.5 + df, angular);

    float y = clamp(bars[bar], 0.0, 1.0);

    float amp = y * (1.0 + 0.8 * (1.0 - y));

    float min_len = 1.0 / u_resolution.y;
    float len = max(amp * max_len, min_len);
    float act = smoothstep(0.0, min_len / max_len, amp);

    float dr = fwidth(r);
    float inner = smoothstep(base_radius - dr, base_radius + dr, r);
    float outer = 1.0 - smoothstep(base_radius + len - dr, base_radius + len + dr, r);
    float radial_alpha = inner * outer * act;

    float alpha = angular_alpha * radial_alpha;

    vec3 col;
    if (gradient_count == 0) {
        col = fg_color;
    } else {
        if (gradient_count == 1) {
            col = gradient_colors[0];
        } else {
            int color = int(floor((gradient_count - 1) * amp));
            color = clamp(color, 0, gradient_count - 2);
            float y_min = float(color) / (gradient_count - 1.0);
            float y_max = float(color + 1) / (gradient_count - 1.0);
            col =
                normalize_C(amp, gradient_colors[color], gradient_colors[color + 1], y_min, y_max);
        }
    }

    fragColor = vec4(mix(bg_color, col, alpha), 1.0);
}
