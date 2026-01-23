// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>
// Description: Harmonic lock feedback (uses inputTexture + phase_xy).

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

float hash21(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 78.233);
    return fract(p.x * p.y);
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

    float total_amp = 0.0;
    vec2 phase_sum = vec2(0.0);

    float step0 = max(float(bc) / float(SAMPLES), 1.0);
    for (int s = 0; s < SAMPLES; s++) {
        int i = int(floor(float(s) * step0));
        i = clamp(i, 0, bc - 1);

        float a = clamp(bars[i], 0.0, 1.0);
        a = a * (1.0 + 0.8 * (1.0 - a));
        total_amp += a;
        phase_sum += phase_xy[i] * a;
    }

    float avg_amp = total_amp / float(SAMPLES);

    float hsum = 0.0;
    float hwsum = 0.0;
    int half_count = max(bc / 2, 1);
    float step1 = max(float(half_count - 1) / float(SAMPLES), 1.0);
    for (int s = 0; s < SAMPLES; s++) {
        int i = 1 + int(floor(float(s) * step1));
        if (i >= half_count) {
            continue;
        }
        int j = i * 2;
        if (j >= bc) {
            continue;
        }

        float ai = clamp(bars[i], 0.0, 1.0);
        float aj = clamp(bars[j], 0.0, 1.0);
        ai = ai * (1.0 + 0.8 * (1.0 - ai));
        aj = aj * (1.0 + 0.8 * (1.0 - aj));

        float w = sqrt(ai * aj);

        vec2 phi = safe_norm(phase_xy[i]);
        vec2 phj = safe_norm(phase_xy[j]);
        float align01 = 0.5 + 0.5 * dot(phi, phj);

        float coh = sqrt(clamp(length(phase_xy[i]), 0.0, 1.0) *
                         clamp(length(phase_xy[j]), 0.0, 1.0));

        float ok = smoothstep(0.55, 0.90, align01) * smoothstep(0.10, 0.60, coh);
        hsum += w * ok;
        hwsum += w;
    }

    float harmonicity = hsum / max(hwsum, 1e-6);

    float lock = smoothstep(0.18, 0.55, harmonicity) * smoothstep(0.03, 0.18, avg_amp);

    vec2 ph_flow = phase_sum / max(total_amp, 1e-6);
    float ph_len = length(ph_flow);
    vec2 ph_dir = (ph_len < 1e-6) ? vec2(0.0) : (ph_flow / ph_len);

    vec2 drift = mix(vec2(0.0042, 0.0024), vec2(0.0007, 0.0005), lock);
    drift += (1.0 - lock) * 0.0045 * ph_dir * smoothstep(0.0, 0.25, ph_len);

    float n0 = hash21(uv * vec2(120.0, 80.0) + shader_time * 0.3);
    vec2 jitter = vec2(n0 - 0.5, hash21(uv * vec2(90.0, 130.0) - shader_time * 0.25) - 0.5);
    drift += (1.0 - lock) * 0.0025 * jitter;

    vec2 fb_uv = clamp(uv + drift, vec2(0.0), vec2(1.0));

    vec3 fb = texture(inputTexture, fb_uv).rgb;
    fb += 0.22 * texture(inputTexture, clamp(fb_uv + vec2(px.x, 0.0), vec2(0.0), vec2(1.0))).rgb;
    fb += 0.22 * texture(inputTexture, clamp(fb_uv - vec2(px.x, 0.0), vec2(0.0), vec2(1.0))).rgb;
    fb += 0.18 * texture(inputTexture, clamp(fb_uv + vec2(0.0, px.y), vec2(0.0), vec2(1.0))).rgb;
    fb += 0.18 * texture(inputTexture, clamp(fb_uv - vec2(0.0, px.y), vec2(0.0), vec2(1.0))).rgb;
    fb /= (1.0 + 0.22 + 0.22 + 0.18 + 0.18);

    float decay = mix(0.940, 0.992, lock);
    fb = mix(bg_color, fb, decay);

    float aspect = u_resolution.x / max(u_resolution.y, 1.0);
    vec2 p = (uv - 0.5) * 2.0;
    p.x *= aspect;

    float r = length(p);
    float theta = atan(p.y, p.x);

    float nfold = mix(3.0, 16.0, lock);
    float spoke = abs(sin(theta * nfold));
    float aa_s = max(fwidth(spoke), 1e-5);
    float spokes = 1.0 - smoothstep(0.20 - 2.0 * aa_s, 0.20 + 2.0 * aa_s, spoke);

    float ring_r = 0.35 + 0.25 * smoothstep(0.0, 0.25, avg_amp);
    float ring_w = mix(0.020, 0.007, lock);
    float aa_r = max(fwidth(r), 1e-5);
    float ring = 1.0 - smoothstep(ring_r - ring_w - aa_r, ring_r + ring_w + aa_r, abs(r - ring_r));

    float idxf = fract((theta + 3.14159265) / (2.0 * 3.14159265)) * float(bc);
    int bi = int(floor(idxf));
    bi = clamp(bi, 0, bc - 1);

    float a_theta = clamp(bars[bi], 0.0, 1.0);
    a_theta = a_theta * (1.0 + 0.8 * (1.0 - a_theta));

    float coherence = clamp(length(phase_xy[bi]), 0.0, 1.0);

    float structure = 0.0;
    structure += 0.75 * spokes;
    structure += 0.95 * ring;
    structure *= (0.25 + 0.75 * smoothstep(0.0, 0.10, a_theta));

    float crisp = mix(0.6, 1.4, lock);
    structure = 1.0 - exp(-structure * crisp);

    float glow = exp(-r * 1.8);

    vec3 col = gradient_map(clamp(0.15 + 0.85 * (0.35 * a_theta + 0.65 * lock), 0.0, 1.0));
    col = mix(col, vec3(1.0), 0.10 * coherence * lock);

    float ink = structure * (0.35 + 0.85 * a_theta) * mix(0.35, 1.10, lock);
    ink *= mix(0.70, 1.10, smoothstep(0.0, 0.6, coherence));

    float shatter = 0.0;
    shatter += (1.0 - lock) * 0.30 * (0.5 + 0.5 * sin(shader_time * 2.2 + r * 9.0));
    shatter *= smoothstep(0.08, 0.24, avg_amp);

    float injection = 0.0;
    injection += ink;
    injection += 0.20 * glow * lock;
    injection += shatter;
    injection = 1.0 - exp(-injection * 1.8);

    vec3 out_col = fb + col * injection;

    out_col = out_col / (vec3(1.0) + out_col);
    out_col = clamp(out_col, vec3(0.0), vec3(1.0));
    fragColor = vec4(out_col, 1.0);
}
