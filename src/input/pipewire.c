#include "input/pipewire.h"
#include "input/common.h"

#include <spa/param/audio/format-utils.h>

#include <pipewire/pipewire.h>

struct pw_data {
    struct pw_main_loop *loop;
    struct pw_stream *stream;

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
    .param_changed = on_stream_param_changed,
    .process = on_process,
};

static void do_quit(void *userdata, int signal_number) {
    struct pw_data *data = userdata;
    pw_main_loop_quit(data->loop);
    (void)signal_number;
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
    char **argv;

    pw_init(0, &argv);

    data.loop = pw_main_loop_new(NULL);

    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGINT, do_quit, &data);
    pw_loop_add_signal(pw_main_loop_get_loop(data.loop), SIGTERM, do_quit, &data);

    props = pw_properties_new(PW_KEY_MEDIA_TYPE, "Audio", PW_KEY_MEDIA_CATEGORY, "Capture",
                              PW_KEY_MEDIA_ROLE, "Music", NULL);

    pw_properties_set(props, PW_KEY_TARGET_OBJECT, data.cava_audio->source);
    pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, "true");

    data.stream = pw_stream_new_simple(pw_main_loop_get_loop(data.loop), "cava", props,
                                       &stream_events, &data);

    params[0] = spa_format_audio_raw_build(
        &b, SPA_PARAM_EnumFormat,
        &SPA_AUDIO_INFO_RAW_INIT(.format = SPA_AUDIO_FORMAT_S16, .rate = 44100));

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
