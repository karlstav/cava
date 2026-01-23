// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>
// Description: Entropy-driven collapse feedback (uses inputTexture + phase_xy).

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

    const int SAMPLES = 32;

    float sum_amp = 0.0;
    float sum_log = 0.0;
    float sum_vis = 0.0;
    vec2 phase_sum = vec2(0.0);

    float step0 = max(float(bc) / float(SAMPLES), 1.0);
    for (int s = 0; s < SAMPLES; s++) {
        int i = int(floor(float(s) * step0));
        i = clamp(i, 0, bc - 1);

        float a = clamp(bars[i], 0.0, 1.0);
        a = a * (1.0 + 0.8 * (1.0 - a));

        float aa = max(a, 1e-6);
        sum_amp += aa;
        sum_log += aa * log(aa);
        sum_vis += a;
        phase_sum += phase_xy[i] * a;
    }

    float avg_amp = sum_vis / float(SAMPLES);

    float entropy = 0.0;
    if (sum_amp > 1e-6) {
        entropy = log(sum_amp) - (sum_log / sum_amp);
        entropy = entropy / max(log(float(SAMPLES)), 1e-6);
    }
    entropy = clamp(entropy, 0.0, 1.0);

    float collapse = smoothstep(0.55, 0.88, entropy);
    float stability = 1.0 - collapse;

    vec2 ph_flow = phase_sum / max(sum_vis, 1e-6);
    float ph_len = length(ph_flow);
    vec2 ph_dir = (ph_len < 1e-6) ? vec2(0.0) : (ph_flow / ph_len);

    vec2 drift = vec2(0.0018, 0.0009);
    drift += (0.0045 * (0.15 + 0.85 * collapse)) * vec2(sin(shader_time * 0.7 + uv.y * 7.0),
                                                        cos(shader_time * 0.6 + uv.x * 6.0));
    drift += (0.0038 * collapse) * ph_dir;

    vec2 fb_uv = clamp(uv + drift, vec2(0.0), vec2(1.0));

    vec3 fb = texture(inputTexture, fb_uv).rgb;
    fb += 0.24 * texture(inputTexture, clamp(fb_uv + vec2(px.x, 0.0), vec2(0.0), vec2(1.0))).rgb;
    fb += 0.24 * texture(inputTexture, clamp(fb_uv - vec2(px.x, 0.0), vec2(0.0), vec2(1.0))).rgb;
    fb += 0.18 * texture(inputTexture, clamp(fb_uv + vec2(0.0, px.y), vec2(0.0), vec2(1.0))).rgb;
    fb += 0.18 * texture(inputTexture, clamp(fb_uv - vec2(0.0, px.y), vec2(0.0), vec2(1.0))).rgb;
    fb /= (1.0 + 0.24 + 0.24 + 0.18 + 0.18);

    float decay = mix(0.900, 0.990, stability);
    fb = mix(bg_color, fb, decay);

    float aspect = u_resolution.x / max(u_resolution.y, 1.0);
    vec2 p = (uv - 0.5) * 2.0;
    p.x *= aspect;

    float r = length(p);
    float theta = atan(p.y, p.x);

    float idxf = fract((theta + 3.14159265) / (2.0 * 3.14159265)) * float(bc);
    int bi = int(floor(idxf));
    bi = clamp(bi, 0, bc - 1);

    float a_theta = clamp(bars[bi], 0.0, 1.0);
    a_theta = a_theta * (1.0 + 0.8 * (1.0 - a_theta));

    float nfold = mix(2.5, 18.0, stability * stability);
    float spoke = abs(sin(theta * nfold));
    float aa_s = max(fwidth(spoke), 1e-5);
    float spokes = 1.0 - smoothstep(0.16 - 2.0 * aa_s, 0.16 + 2.0 * aa_s, spoke);

    float ring_r = 0.28 + 0.38 * smoothstep(0.0, 0.22, avg_amp);
    float ring_w = mix(0.030, 0.008, stability);
    float aa_r = max(fwidth(r), 1e-5);
    float ring = 1.0 - smoothstep(ring_r - ring_w - aa_r, ring_r + ring_w + aa_r, abs(r - ring_r));

    float lattice = (0.75 * spokes + 0.95 * ring);
    lattice *= (0.22 + 0.78 * smoothstep(0.0, 0.12, a_theta));
    lattice *= (0.20 + 0.80 * stability);

    float core = exp(-r * 6.0);
    float core_breath = 0.6 + 0.4 * sin(shader_time * 0.9 + entropy * 6.0);
    float minimal = core * core_breath * (0.25 + 0.75 * smoothstep(0.02, 0.20, avg_amp));

    float ink = 0.0;
    ink += stability * lattice;
    ink += collapse * minimal;

    float smear = gaussian(p.x, 0.10 + 0.30 * collapse) * exp(-abs(p.y) * (2.0 + 4.0 * stability));
    ink += 0.12 * smear * (0.3 + 0.7 * collapse);

    ink = 1.0 - exp(-ink * 1.9);

    float col_amp = clamp(0.15 + 0.85 * (0.45 * stability + 0.55 * a_theta), 0.0, 1.0);
    vec3 col = gradient_map(col_amp);

    vec3 out_col = fb + col * ink;

    out_col = out_col / (vec3(1.0) + out_col);
    out_col = clamp(out_col, vec3(0.0), vec3(1.0));
    fragColor = vec4(out_col, 1.0);
}
