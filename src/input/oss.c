#include <stdbool.h>
#include <stddef.h>

#include <sys/ioctl.h>
#include <sys/soundcard.h>

#include "cava/input/common.h"
#include "cava/input/oss.h"

static bool set_format(int fd, struct audio_data *audio) {
    // CAVA favors signed and little endian (sle) formats. It might actually not work correctly if
    // we feed it an unsigned or big endian format. Therefore we prefer to select one of the
    // supported sle formats, if any exist.

    // The favored sle formats. 16bit has priority, followed by "bigger is better".
    static const int fmts_sle[] = {AFMT_S16_LE, AFMT_S32_LE, AFMT_S24_LE, AFMT_S8};

    // Bitmasks of formats categorized by bitlength.
    static const int fmts_8bits = AFMT_U8 | AFMT_S8;
    static const int fmts_16bits = AFMT_S16_LE | AFMT_S16_BE | AFMT_U16_LE | AFMT_U16_BE;
    static const int fmts_24bits = AFMT_S24_LE | AFMT_S24_BE | AFMT_U24_LE | AFMT_U24_BE;
    static const int fmts_32bits = AFMT_S32_LE | AFMT_S32_BE | AFMT_U32_LE | AFMT_U32_BE;

    int fmts;
    int fmt;

    // Get all supported formats from the audio device.
    if (ioctl(fd, SNDCTL_DSP_GETFMTS, &fmts) == -1) {
        fprintf(stderr, __FILE__ ": ioctl(SNDCTL_DSP_GETFMTS) failed: %s\n", strerror(errno));
        return false;
    }

    // Determine the sle format for the requested bitlength.
    switch (audio->format) {
    case 8:
        fmt = AFMT_S8;
        break;
    case 16:
        fmt = AFMT_S16_LE;
        break;
    case 24:
        fmt = AFMT_S24_LE;
        break;
    case 32:
        fmt = AFMT_S32_LE;
        break;
    default:
        fprintf(stderr, __FILE__ ": Invalid format: %d\n", audio->format);
        return false;
    }

    // If the requested format is not available then test for the other sle formats.
    if (!(fmts & fmt)) {
        for (size_t i = 0; i < sizeof(fmts_sle) / sizeof(fmts_sle[0]); ++i) {
            if (fmts & fmts_sle[i]) {
                fmt = fmts_sle[i];
                break;
            }
        }
    }

    // Set the format of the device. If not supported then OSS will adjust to a supported format.
    if (ioctl(fd, SNDCTL_DSP_SETFMT, &fmt) == -1) {
        fprintf(stderr, __FILE__ ": ioctl(SNDCTL_DSP_SETFMT) failed: %s\n", strerror(errno));
        return false;
    }

    // Determine the actual bitlength of the returned format.
    if (fmts_8bits & fmt)
        audio->format = 8;
    else if (fmts_16bits & fmt)
        audio->format = 16;
    else if (fmts_24bits & fmt)
        audio->format = 24;
    else if (fmts_32bits & fmt)
        audio->format = 32;
    else {
        fprintf(stderr, __FILE__ ": No support for 8, 16, 24 or 32 bits in OSS source '%s'.\n",
                audio->source);
        return false;
    }

    return true;
}

static bool set_channels(int fd, struct audio_data *audio) {
    // Try to set the requested channels, OSS will adjust to a supported value. If CAVA doesn't
    // support the final value then it will complain later.
    int channels = audio->channels;

    if (ioctl(fd, SNDCTL_DSP_CHANNELS, &channels) == -1) {
        fprintf(stderr, __FILE__ ": ioctl(SNDCTL_DSP_CHANNELS) failed: %s\n", strerror(errno));
        return false;
    }

    audio->channels = channels;

    return true;
}

static bool set_rate(int fd, struct audio_data *audio) {
    // Try to set the requested rate, OSS will adjust to a supported value. If CAVA doesn't support
    // the final value then it will complain later.
    int rate = audio->rate;

    if (ioctl(fd, SNDCTL_DSP_SPEED, &rate) == -1) {
        fprintf(stderr, __FILE__ ": ioctl(SNDCTL_DSP_SPEED) failed: %s\n", strerror(errno));
        return false;
    }

    audio->rate = rate;

    return true;
}

void *input_oss(void *data) {
    static const int flags = O_RDONLY;

    struct audio_data *audio = data;
    int bytes;
    size_t buf_size;

    int fd = -1;
    void *buf = NULL;

    bool success = false;

    if ((fd = open(audio->source, flags, 0)) == -1) {
        fprintf(stderr, __FILE__ ": Could not open OSS source '%s': %s\n", audio->source,
                strerror(errno));
        goto cleanup;
    }

    // For OSS it's adviced to determine format, channels and rate in this order.
    if (!set_format(fd, audio) || !set_channels(fd, audio) || !set_rate(fd, audio))
        goto cleanup;

    // Parameters finalized. Signal main thread.
    signal_threadparams(audio);

    // OSS uses 32 bits for 24bit.
    if (audio->format == 24)
        bytes = 4; // = 32 / 8
    else
        bytes = audio->format / 8;

    buf_size = audio->input_buffer_size * bytes;

    if ((buf = malloc(buf_size)) == NULL) {
        fprintf(stderr, __FILE__ ": malloc() failed: %s\n", strerror(errno));
        goto cleanup;
    }

    while (audio->terminate != 1) {
        ssize_t rd;

        if ((rd = read(fd, buf, buf_size)) < 0) {
            fprintf(stderr, __FILE__ ": read() failed: %s\n", strerror(errno));
            goto cleanup;
        } else if (rd == 0)
            signal_terminate(audio);
        else
            write_to_cava_input_buffers(rd / bytes, buf, audio);
    }

    success = true;

cleanup:
    free(buf);

    if ((fd >= 0) && (close(fd) == -1)) {
        fprintf(stderr, __FILE__ ": close() failed: %s\n", strerror(errno));
        success = false;
    }

    signal_threadparams(audio);
    signal_terminate(audio);

    if (!success)
        exit(EXIT_FAILURE);

    return NULL;
}
