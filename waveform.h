#pragma once

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum waveform_channel { WAVEFORM_AVERAGE, WAVEFORM_LEFT, WAVEFORM_RIGHT };

// Mono history uses absolute audio-frame indices, including initial silence at negative indices.
// Storage compaction must not change the presentation cursor or trigger identity.
struct waveform_history {
    double *samples;
    int length, capacity, keep, window, rate, lookback, delay, frame, max_delay;
    int64_t first, end, last_time, last_trigger, trigger_time;
    double cursor, gain_cursor;
    int running, started, waiting;
    double *min, *max;
};

static inline int waveform_init(struct waveform_history *h, int rate, int duration_ms, int bars,
                                int block_frames, int framerate) {
    h->rate = rate;
    h->window = (int64_t)rate * duration_ms / 1000;

    if (h->window < 1)
        h->window = 1;

    h->lookback = rate / 20;
    h->frame = (rate + framerate - 1) / framerate;

    // Two visual frames, or one input block plus a frame, whichever is longer. The extra frame
    // keeps the cursor from reaching the end of the data just before the next block arrives.
    h->delay = block_frames + h->frame;
    if (h->delay < 2 * h->frame)
        h->delay = 2 * h->frame;

    // The delay can grow later to fit larger blocks, so the history is sized for that.
    // Right after a block arrives the cursor can trail the end by up to twice the delay.
    h->max_delay = h->delay > rate / 4 ? h->delay : rate / 4;
    h->keep = h->window + h->lookback + 2 * h->max_delay + rate / 10 + 100;
    h->capacity = 2 * h->keep;
    h->length = h->keep;
    h->first = -h->keep;
    h->cursor = -h->delay;
    h->gain_cursor = h->cursor;
    h->last_trigger = INT64_MIN;
    h->samples = calloc(h->capacity, sizeof(double));
    h->min = calloc(bars, sizeof(double));
    h->max = calloc(bars, sizeof(double));

    return h->samples && h->min && h->max;
}

static inline void waveform_free(struct waveform_history *h) {
    free(h->samples);
    free(h->min);
    free(h->max);
    memset(h, 0, sizeof(*h));
}

static inline void waveform_push(struct waveform_history *h, const double *in, int frames,
                                 int channels, enum waveform_channel mono) {
    if (frames <= 0)
        return;

    h->end += frames;
    if (frames > h->keep) {
        in += (int64_t)(frames - h->keep) * channels;
        frames = h->keep;
        h->length = 0; // The discarded prefix is a gap, not contiguous with the old history.
    }

    if (h->length + frames > h->capacity) {
        int retain = h->keep - frames;
        memmove(h->samples, h->samples + h->length - retain, retain * sizeof(double));
        h->length = retain;
    }

    for (int f = 0; f < frames; f++) {
        double sample = in[f * channels];
        if (channels == 2) {
            if (mono == WAVEFORM_AVERAGE)
                sample = (in[f * 2] + in[f * 2 + 1]) / 2;
            else if (mono == WAVEFORM_RIGHT)
                sample = in[f * 2 + 1];
        }

        h->samples[h->length++] = sample / 32768;
    }

    h->first = h->end - h->length;
}

static inline void waveform_reset_trigger(struct waveform_history *h) {
    h->last_trigger = INT64_MIN;
    h->waiting = 0;
    h->gain_cursor = h->cursor;
}

static inline void waveform_discontinuity(struct waveform_history *h, int bars) {
    memset(h->samples, 0, h->capacity * sizeof(double));
    memset(h->min, 0, bars * sizeof(double));
    memset(h->max, 0, bars * sizeof(double));
    h->length = h->keep;
    h->first = h->end - h->keep;
    h->cursor = h->end - h->delay;
    h->started = h->running = 0;
    waveform_reset_trigger(h);
}

// Some backends negotiate a larger quantum than the requested block size. Increase the
// buffering delay by holding briefly, never by moving the presentation cursor backwards.
static inline void waveform_buffer(struct waveform_history *h, int block_frames) {
    int need = block_frames + h->frame;
    if (need > h->max_delay)
        need = h->max_delay;

    if (need > h->delay) {
        h->delay = need;
        h->running = 0;
        waveform_reset_trigger(h);
    }
}

// Advance by elapsed monotonic time, not by how many samples arrived in this draw.
static inline void waveform_advance(struct waveform_history *h, int64_t now) {
    double elapsed = h->started ? (now - h->last_time) / 1000000000.0 : 0.0;
    h->last_time = now;
    if (!h->started) {
        if (!h->end)
            return;
        h->started = h->running = 1;
        h->cursor = h->end - h->delay;
        waveform_reset_trigger(h);
        return;
    }

    // A long render stall or dropped history gets a single bounded-latency resynchronization.
    int lag = h->delay + h->rate / 10;
    if (lag < 2 * h->delay)
        lag = 2 * h->delay;

    if (h->cursor - h->window - h->lookback - 100 < h->first || h->end - h->cursor > lag) {
        h->cursor = h->end - h->delay;
        h->running = 1;
        waveform_reset_trigger(h);
        return;
    }

    if (!h->running) {
        if (h->end - h->cursor >= h->delay)
            h->running = 1;
        return; // Refill without replaying the time spent waiting for input.
    }

    double next = h->cursor + fmax(0.0, elapsed) * h->rate;
    if (next > h->end) {
        h->cursor = h->end;
        h->running = 0;
    } else {
        h->cursor = next;
    }
}

// Last rising crossing of a low-passed signal, with hysteresis and filter warmup.
static inline int waveform_trigger(const double *in, int search) {
    const int warmup = 100;
    if (search <= warmup)
        return -1;

    double y = 0.0, peak = 0.0;
    for (int f = 0; f < search; f++) {
        y += 0.05 * (in[f] - y);
        if (fabs(y) > peak)
            peak = fabs(y);
    }

    y = 0.0;
    int armed = 0, trigger = -1, last_negative = -1;
    for (int f = 0; f < search; f++) {
        y += 0.05 * (in[f] - y);
        if (f < warmup)
            continue;

        if (y < -0.1 * peak && !armed) {
            armed = 1;
            last_negative = -1;
        }

        if (!armed)
            continue;

        if (y < 0.0) {
            if (in[f] < 0.0)
                last_negative = f;
        } else {
            trigger = (in[f] < 0.0 || last_negative < 0) ? f : last_negative + 1;
            armed = 0;
        }
    }

    return trigger;
}

// -1 holds the previous envelope. Old crossings cannot extend the 100 ms trigger-loss hold.
static inline int waveform_select(struct waveform_history *h, int64_t now, int sync) {
    int64_t end = (int64_t)floor(h->cursor);
    int offset = (int)(end - h->window - h->first);

    if (!sync)
        return offset;

    int from = offset - h->lookback - 100;
    if (from < 0)
        from = 0;

    int trigger = waveform_trigger(h->samples + from, offset - from + 1);
    if (trigger >= 0) {
        trigger += from;
        int64_t absolute = h->first + trigger;

        if (absolute > h->last_trigger) {
            h->last_trigger = absolute;
            h->trigger_time = now;
            h->waiting = 1;
            return trigger;
        }
    }

    if (!h->waiting) {
        h->trigger_time = now;
        h->waiting = 1;
    }

    return now - h->trigger_time < 100000000LL ? -1 : offset;
}

static inline double waveform_envelope(struct waveform_history *h, const double *window, int bars,
                                       double sens) {
    double peak = 0.0;
    for (int b = 0; b < bars; b++) {
        int start = (int64_t)b * h->window / bars;
        int end = (int64_t)(b + 1) * h->window / bars;
        if (end <= start)
            end = start + 1;

        int from = start > 0 ? start - 1 : 0;
        int to = end + 1 < h->window ? end + 1 : h->window;
        double min = window[from], max = window[from];
        for (int f = from + 1; f < to; f++) {
            if (window[f] < min)
                min = window[f];
            else if (window[f] > max)
                max = window[f];
        }

        min *= sens;
        max *= sens;
        peak = fmax(peak, fmax(fabs(min), fabs(max)));
        h->min[b] = fmax(-1.0, fmin(1.0, min));
        h->max[b] = fmax(-1.0, fmin(1.0, max));
    }

    return peak;
}

// snap the span [low, high] to whole pixels. all values are in pixels from the bottom and zero is
// the pixel border that is silence. the edges are rounded by their distance to zero so mirrored
// spans stay mirrored, with ties going away from zero. a span thinner than a pixel becomes the
// pixel it's centered in.
static inline void waveform_snap(double low, double high, double zero, int height, int *bottom,
                                 int *top) {
    double a, b;

    if (high - low < 1.0) {
        double distance = (low + high) / 2 - zero;
        double pixel = zero + (distance > 0 ? floor(distance) : -1 - floor(-distance));
        pixel = fmax(0.0, fmin(pixel, height - 1.0));
        a = pixel;
        b = fmin(pixel + 1.0, height);
    } else {
        a = fmax(0.0, fmin(zero + round(low - zero), height));
        b = fmax(0.0, fmin(zero + round(high - zero), height));
    }

    *bottom = a;
    *top = b;
}

// turn the envelope into [base, top] bar ranges, drawn as a line around the center. `total` is
// the full range of the output and `pixel` the size of the smallest step it can draw, both in
// output units. an even number of pixels is used so the center falls on a pixel border.
static inline void waveform_ranges(const struct waveform_history *h, int bars, double total,
                                   int pixel, float *top, int *base) {
    int half = total / pixel / 2;

    for (int n = 0; n < bars; n++) {
        int bottom, upper;
        waveform_snap(half + h->min[n] * half, half + h->max[n] * half, half, 2 * half, &bottom,
                      &upper);

        base[n] = bottom * pixel;
        top[n] = upper * pixel;
    }
}

// Preserve the old 60 FPS gain rates, expressed per second of presented audio.
static inline double waveform_gain(double peak, double seconds, int *initial) {
    if (peak > 1.0) {
        *initial = 0;
        return pow(0.98, seconds * 60.0);
    }

    if (peak > 0.0)
        return pow(1.001 * (*initial ? 1.1 : 1.0), seconds * 60.0);

    return 1.0;
}
