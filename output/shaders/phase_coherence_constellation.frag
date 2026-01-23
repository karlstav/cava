// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2026 rezky_nightky <with.rezky@gmail.com>
// Description: Phase coherence constellation (uses phase_xy).

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

vec2 safe_norm(vec2 v) {
    float l = length(v);
    if (l < 1e-5) {
        return vec2(0.0);
    }
    return v / l;
}

float dist_to_segment(vec2 p, vec2 a, vec2 b) {
    vec2 ab = b - a;
    float t = dot(p - a, ab) / max(dot(ab, ab), 1e-6);
    t = clamp(t, 0.0, 1.0);
    return length((a + ab * t) - p);
}

void band_wrap(inout int i, int bc) {
    if (i < 0) {
        i += bc;
    }
    if (i >= bc) {
        i -= bc;
    }
}

void main() {
    int bc = min(bars_count, 512);
    if (bc <= 0) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float aspect = u_resolution.x / max(u_resolution.y, 1.0);

    vec2 p = fragCoord - vec2(0.5);
    p.x *= aspect;

    const float PI = 3.14159265358979323846;
    const float TAU = 6.28318530717958647692;

    float r0 = length(p);

    float base_r = 0.36;
    float span_r = 0.22;
    float ring_pad = 0.16;

    if (r0 < base_r - ring_pad || r0 > base_r + span_r + ring_pad) {
        fragColor = vec4(bg_color, 1.0);
        return;
    }

    float theta = atan(p.y, p.x);
    float a = fract((theta + PI) / TAU);
    float idx_f = a * float(bc);
    int i_center = int(floor(idx_f));

    vec3 sum = vec3(0.0);

    for (int di = -4; di <= 4; di++) {
        int i = i_center + di;
        band_wrap(i, bc);

        float amp_i = clamp(bars[i], 0.0, 1.0);
        amp_i = amp_i * (1.0 + 0.8 * (1.0 - amp_i));

        float coh_i = clamp(length(phase_xy[i]), 0.0, 1.0);

        float ang_i = ((float(i) + 0.5) / float(bc)) * TAU - PI;
        float rr_i = base_r + amp_i * span_r;
        vec2 pos_i = vec2(cos(ang_i) * aspect, sin(ang_i)) * rr_i;

        float tw = 0.85 + 0.15 * sin(shader_time * 1.2 + float(i) * 0.23);
        float star_sz = mix(0.0045, 0.017, amp_i) * (0.55 + 0.45 * coh_i) * tw;

        float d_star = length(p - pos_i);
        float aa_star = max(fwidth(d_star), 1e-5);
        float star_alpha = 1.0 - smoothstep(star_sz - aa_star, star_sz + aa_star, d_star);
        star_alpha *= (0.10 + 0.90 * coh_i);
        star_alpha *= smoothstep(0.0, 0.03, amp_i);

        vec3 col_i = gradient_map(clamp(0.25 + 0.75 * amp_i, 0.0, 1.0));
        sum += col_i * (1.05 * star_alpha);

        int j = i + 1;
        band_wrap(j, bc);

        float amp_j = clamp(bars[j], 0.0, 1.0);
        amp_j = amp_j * (1.0 + 0.8 * (1.0 - amp_j));

        float coh_j = clamp(length(phase_xy[j]), 0.0, 1.0);

        float ang_j = ((float(j) + 0.5) / float(bc)) * TAU - PI;
        float rr_j = base_r + amp_j * span_r;
        vec2 pos_j = vec2(cos(ang_j) * aspect, sin(ang_j)) * rr_j;

        vec2 ph_i = safe_norm(phase_xy[i]);
        vec2 ph_j = safe_norm(phase_xy[j]);
        float align = dot(ph_i, ph_j);
        float align01 = 0.5 + 0.5 * align;

        float coh_ij = sqrt(coh_i * coh_j);
        float link = smoothstep(0.55, 0.88, align01);
        link *= smoothstep(0.06, 0.55, coh_ij);
        link *= smoothstep(0.01, 0.06, 0.5 * (amp_i + amp_j));

        float d_line = dist_to_segment(p, pos_i, pos_j);
        float w = 0.0040 + 0.0065 * (0.35 + 0.65 * max(amp_i, amp_j));
        float aa_line = max(fwidth(d_line), 1e-5);
        float line_alpha = 1.0 - smoothstep(w - aa_line, w + aa_line, d_line);

        vec3 col_l = gradient_map(clamp(0.20 + 0.80 * (0.5 * (amp_i + amp_j)), 0.0, 1.0));
        sum += col_l * (1.35 * line_alpha * link);

        int j2 = i + 2;
        band_wrap(j2, bc);

        float amp_j2 = clamp(bars[j2], 0.0, 1.0);
        amp_j2 = amp_j2 * (1.0 + 0.8 * (1.0 - amp_j2));

        float coh_j2 = clamp(length(phase_xy[j2]), 0.0, 1.0);

        float ang_j2 = ((float(j2) + 0.5) / float(bc)) * TAU - PI;
        float rr_j2 = base_r + amp_j2 * span_r;
        vec2 pos_j2 = vec2(cos(ang_j2) * aspect, sin(ang_j2)) * rr_j2;

        vec2 ph_j2 = safe_norm(phase_xy[j2]);
        float align_n2 = dot(ph_i, ph_j2);
        float align_n201 = 0.5 + 0.5 * align_n2;

        float coh_i2 = sqrt(coh_i * coh_j2);
        float link_n2 = smoothstep(0.52, 0.85, align_n201);
        link_n2 *= smoothstep(0.06, 0.52, coh_i2);
        link_n2 *= smoothstep(0.01, 0.07, 0.5 * (amp_i + amp_j2));

        float d_n2 = dist_to_segment(p, pos_i, pos_j2);
        float w_n2 = 0.0034 + 0.0054 * (0.35 + 0.65 * max(amp_i, amp_j2));
        float aa_n2 = max(fwidth(d_n2), 1e-5);
        float a_n2 = 1.0 - smoothstep(w_n2 - aa_n2, w_n2 + aa_n2, d_n2);

        vec3 col_n2 = gradient_map(clamp(0.20 + 0.80 * (0.5 * (amp_i + amp_j2)), 0.0, 1.0));
        sum += col_n2 * (0.90 * a_n2 * link_n2);

        int k = i * 2;
        if (k < bc) {
            float amp_k = clamp(bars[k], 0.0, 1.0);
            amp_k = amp_k * (1.0 + 0.8 * (1.0 - amp_k));

            float coh_k = clamp(length(phase_xy[k]), 0.0, 1.0);

            float ang_k = ((float(k) + 0.5) / float(bc)) * TAU - PI;
            float rr_k = base_r + amp_k * span_r;
            vec2 pos_k = vec2(cos(ang_k) * aspect, sin(ang_k)) * rr_k;

            vec2 ph_k = safe_norm(phase_xy[k]);
            float align2 = dot(ph_i, ph_k);
            float align201 = 0.5 + 0.5 * align2;

            float coh_ik = sqrt(coh_i * coh_k);
            float link2 = smoothstep(0.60, 0.92, align201);
            link2 *= smoothstep(0.08, 0.60, coh_ik);
            link2 *= smoothstep(0.02, 0.10, 0.5 * (amp_i + amp_k));

            float d2 = dist_to_segment(p, pos_i, pos_k);
            float w2 = 0.0030 + 0.0048 * (0.35 + 0.65 * max(amp_i, amp_k));
            float aa2 = max(fwidth(d2), 1e-5);
            float a2 = 1.0 - smoothstep(w2 - aa2, w2 + aa2, d2);

            vec3 col2 = gradient_map(clamp(0.20 + 0.80 * (0.5 * (amp_i + amp_k)), 0.0, 1.0));
            sum += col2 * (0.70 * a2 * link2);
        }
    }

    vec3 out_col = bg_color + sum;
    out_col = out_col / (vec3(1.0) + out_col);

    fragColor = vec4(clamp(out_col, vec3(0.0), vec3(1.0)), 1.0);
}
