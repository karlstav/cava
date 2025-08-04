#include "input/pipewire.h"
#include "input/common.h"
#include <math.h>

#include <spa/param/audio/format-utils.h>
#include <spa/param/latency-utils.h>

#include <pipewire/pipewire.h>

struct pw_data {
    struct pw_main_loop *loop;
    struct spa_source *timer;
    struct pw_stream *stream;
    bool idle;

    struct spa_audio_info format;
    unsigned move : 1;
    struct audio_data *cava_audio;
};

static void on_process(void *userdata) {
    struct pw_data *data = userdata;
    struct pw_buffer *b;
    struct spa_buffer *buf;
    uint32_t n_samples;
    int16_t *samples;

    if (data->cava_audio->terminate == 1)
        pw_main_loop_quit(data->loop);

    if ((b = pw_stream_dequeue_buffer(data->stream)) == NULL) {
        pw_log_warn("out of buffers: %m");
        return;
    }

    buf = b->buffer;
    if ((samples = buf->datas[0].data) == NULL)
        return;

    n_samples = buf->datas[0].chunk->size / (data->cava_audio->format / 8);

    write_to_cava_input_buffers(n_samples, buf->datas[0].data, data->cava_audio);

    pw_stream_queue_buffer(data->stream, b);
}

static void on_timeout(void *userdata, uint64_t expirations) {
    struct pw_data *data = userdata;

    if (data->cava_audio->terminate)
        pw_main_loop_quit(data->loop);

    if (!data->idle) {
        if (expirations < 10) {
            reset_output_buffers(data->cava_audio);
        } else {
            struct timespec timeout, interval;
            data->idle = true;
            timeout.tv_sec = 0;
            timeout.tv_nsec = 500 * SPA_NSEC_PER_MSEC;
            interval.tv_sec = 0;
            interval.tv_nsec = 500 * SPA_NSEC_PER_MSEC;

            pw_loop_update_timer(pw_main_loop_get_loop(data->loop), data->timer, &timeout,
                                 &interval, false);
        }
    }
}

static void on_stream_state_changed(void *_data, [[maybe_unused]] enum pw_stream_state old,
                                    enum pw_stream_state state,
                                    [[maybe_unused]] const char *error) {
    struct pw_data *data = _data;

    data->idle = false;
    switch (state) {
    case PW_STREAM_STATE_PAUSED:
        struct timespec timeout, interval;

        timeout.tv_sec = 0;
        timeout.tv_nsec = 10 * SPA_NSEC_PER_MSEC;
        interval.tv_sec = 0;
        interval.tv_nsec = 10 * SPA_NSEC_PER_MSEC;

        pw_loop_update_timer(pw_main_loop_get_loop(data->loop), data->timer, &timeout, &interval,
                             false);
        break;
    case PW_STREAM_STATE_STREAMING:
        pw_loop_update_timer(pw_main_loop_get_loop(data->loop), data->timer, NULL, NULL, false);
        break;
    default:
        break;
    }
}

static void on_stream_param_changed(void *_data, uint32_t id, const struct spa_pod *param) {
    struct pw_data *data = _data;

    if (param == NULL || id != SPA_PARAM_Format)
        return;

    if (spa_format_parse(param, &data->format.media_type, &data->format.media_subtype) < 0)
        return;

    if (data->format.media_type != SPA_MEDIA_TYPE_audio ||
        data->format.media_subtype != SPA_MEDIA_SUBTYPE_raw)
        return;

    spa_format_audio_raw_parse(param, &data->format.info.raw);
}

static const struct pw_stream_events stream_events = {
    PW_VERSION_STREAM_EVENTS,
    .state_changed = on_stream_state_changed,
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

unsigned int next_power_of_2(unsigned int n) {
    if (n == 0)
        return 1; // Handle zero case

    n--; // Decrement n to handle the case where n is already a power of two
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n++; // Increment to get the next power of two
    return n;
}

void *input_pipewire(void *audiodata) {
    struct pw_data data = {
        0,
    };

    data.cava_audio = (struct audio_data *)audiodata;
    const struct spa_pod *params[1];
    uint8_t buffer[data.cava_audio->input_buffer_size];
    struct pw_properties *props;
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
    uint32_t nom;
    nom = next_power_of_2((512 * data.cava_audio->rate / 48000));
    pw_init(0, 0);

    data.loop = pw_main_loop_new(NULL);
    if (data.loop == NULL) {
        data.cava_audio->terminate = 1;
        sprintf(data.cava_audio->error_message,
                __FILE__ ": Could not create main loop. Is your system running pipewire? Maybe try "
                         "pulse input method instead.");
        return 0;
    }

    data.timer = pw_loop_add_timer(pw_main_loop_get_loop(data.loop), on_timeout, &data);

    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture",
                              PW_KEY_MEDIA_ROLE, "Music", NULL);

    char *source = data.cava_audio->source;
    size_t sourcelength = strlen(source);

    if (8 <= sourcelength && !strcmp(source + sourcelength - 8, ".monitor")) {
        source[sourcelength - 8] = '\0';
        pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");
    }
    if (strcmp(source, "auto") == 0) {
        pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");
    } else if (strcmp(source, "auto_input") != 0) {
        pw_properties_set(props, PW_KEY_TARGET_OBJECT, source);
    }
    pw_properties_setf(props, PW_KEY_NODE_LATENCY, "%u/%u", nom, data.cava_audio->rate);

    if (data.cava_audio->active)
        pw_properties_set(props, PW_KEY_NODE_ALWAYS_PROCESS, "true");
    else
        pw_properties_set(props, PW_KEY_NODE_PASSIVE, "true");

    if (data.cava_audio->virtual)
        pw_properties_set(props, PW_KEY_NODE_VIRTUAL, "true");

    enum spa_audio_format audio_format = SPA_AUDIO_FORMAT_S16;

    switch (data.cava_audio->format) {
    case 8:
        audio_format = SPA_AUDIO_FORMAT_S8;
        break;
    case 16:
        audio_format = SPA_AUDIO_FORMAT_S16;
        break;
    case 24:
        audio_format = SPA_AUDIO_FORMAT_S24;
        break;
    case 32:
        audio_format = SPA_AUDIO_FORMAT_S32;
        break;
    };

    if (data.cava_audio->remix) {
        pw_properties_set(props, PW_KEY_STREAM_DONT_REMIX, "false");
        pw_properties_set(props, "channelmix.upmix", "true");

        if (data.cava_audio->channels < 2) {
            // N to 1 with all channels shown
            params[0] = spa_format_audio_raw_build(
                &b, SPA_PARAM_EnumFormat,
                &SPA_AUDIO_INFO_RAW_INIT(.format = audio_format, .rate = data.cava_audio->rate,
                                         .channels = data.cava_audio->channels, ));
        } else {
            // N to 2 with all channels shown
            params[0] = spa_format_audio_raw_build(
                &b, SPA_PARAM_EnumFormat,
                &SPA_AUDIO_INFO_RAW_INIT(.format = audio_format, .rate = data.cava_audio->rate,
                                         .channels = data.cava_audio->channels,
                                         .position = {SPA_AUDIO_CHANNEL_FL,
                                                      SPA_AUDIO_CHANNEL_FR}, ));
        }
    } else {
        // N to 2 with only FL and FR shown
        params[0] = spa_format_audio_raw_build(
            &b, SPA_PARAM_EnumFormat,
            &SPA_AUDIO_INFO_RAW_INIT(.format = audio_format, .rate = data.cava_audio->rate,
                                     .channels = data.cava_audio->channels, ));
    }

    data.stream = pw_stream_new_simple(pw_main_loop_get_loop(data.loop), "cava", props,
                                       &stream_events, &data);

    pw_stream_connect(data.stream, PW_DIRECTION_INPUT, PW_ID_ANY,
                      PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS |
                          PW_STREAM_FLAG_RT_PROCESS,
                      params, 1);

    pw_main_loop_run(data.loop);

    pw_stream_destroy(data.stream);
    pw_main_loop_destroy(data.loop);
    pw_deinit();
    return 0;
}
