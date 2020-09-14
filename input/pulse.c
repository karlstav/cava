#include "input/pulse.h"
#include "input/common.h"

#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#include <pulse/simple.h>

pa_mainloop *m_pulseaudio_mainloop;

void cb(__attribute__((unused)) pa_context *pulseaudio_context, const pa_server_info *i,
        void *userdata) {

    // getting default sink name
    struct audio_data *audio = (struct audio_data *)userdata;
    free(audio->source);
    audio->source = malloc(sizeof(char) * 1024);

    strcpy(audio->source, i->default_sink_name);

    // appending .monitor suufix
    audio->source = strcat(audio->source, ".monitor");

    // quiting mainloop
    pa_context_disconnect(pulseaudio_context);
    pa_context_unref(pulseaudio_context);
    pa_mainloop_quit(m_pulseaudio_mainloop, 0);
    pa_mainloop_free(m_pulseaudio_mainloop);
}

void pulseaudio_context_state_callback(pa_context *pulseaudio_context, void *userdata) {

    // make sure loop is ready
    switch (pa_context_get_state(pulseaudio_context)) {
    case PA_CONTEXT_UNCONNECTED:
        // printf("UNCONNECTED\n");
        break;
    case PA_CONTEXT_CONNECTING:
        // printf("CONNECTING\n");
        break;
    case PA_CONTEXT_AUTHORIZING:
        // printf("AUTHORIZING\n");
        break;
    case PA_CONTEXT_SETTING_NAME:
        // printf("SETTING_NAME\n");
        break;
    case PA_CONTEXT_READY: // extract default sink name
        // printf("READY\n");
        pa_operation_unref(pa_context_get_server_info(pulseaudio_context, cb, userdata));
        break;
    case PA_CONTEXT_FAILED:
        printf("failed to connect to pulseaudio server\n");
        exit(EXIT_FAILURE);
        break;
    case PA_CONTEXT_TERMINATED:
        // printf("TERMINATED\n");
        pa_mainloop_quit(m_pulseaudio_mainloop, 0);
        break;
    }
}

void getPulseDefaultSink(void *data) {

    struct audio_data *audio = (struct audio_data *)data;
    pa_mainloop_api *mainloop_api;
    pa_context *pulseaudio_context;
    int ret;

    // Create a mainloop API and connection to the default server
    m_pulseaudio_mainloop = pa_mainloop_new();

    mainloop_api = pa_mainloop_get_api(m_pulseaudio_mainloop);
    pulseaudio_context = pa_context_new(mainloop_api, "cava device list");

    // This function connects to the pulse server
    pa_context_connect(pulseaudio_context, NULL, PA_CONTEXT_NOFLAGS, NULL);

    //        printf("connecting to server\n");

    // This function defines a callback so the server will tell us its state.
    pa_context_set_state_callback(pulseaudio_context, pulseaudio_context_state_callback,
                                  (void *)audio);

    // starting a mainloop to get default sink

    // starting with one nonblokng iteration in case pulseaudio is not able to run
    if (!(ret = pa_mainloop_iterate(m_pulseaudio_mainloop, 0, &ret))) {
        printf("Could not open pulseaudio mainloop to "
               "find default device name: %d\n"
               "check if pulseaudio is running\n",
               ret);

        exit(EXIT_FAILURE);
    }

    pa_mainloop_run(m_pulseaudio_mainloop, &ret);
}

void *input_pulse(void *data) {

    struct audio_data *audio = (struct audio_data *)data;
    uint16_t frames = audio->FFTtreblebufferSize;
    int channels = 2;
    int16_t buf[frames * channels];

    /* The sample type to use */
    static const pa_sample_spec ss = {.format = PA_SAMPLE_S16LE, .rate = 44100, .channels = 2};

    audio->format = 16;

    const int frag_size = frames * channels * audio->format / 8 *
                          2; // we double this because of cpu performance issues with pulseaudio

    pa_buffer_attr pb = {.maxlength = (uint32_t)-1, // BUFSIZE * 2,
                         .fragsize = frag_size};

    pa_simple *s = NULL;
    int error;

    if (!(s = pa_simple_new(NULL, "cava", PA_STREAM_RECORD, audio->source, "audio for cava", &ss,
                            NULL, &pb, &error))) {
        sprintf(audio->error_message, __FILE__ ": Could not open pulseaudio source: %s, %s. \
		To find a list of your pulseaudio sources run 'pacmd list-sources'\n",
                audio->source, pa_strerror(error));

        audio->terminate = 1;
    }

    while (!audio->terminate) {
        if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
            sprintf(audio->error_message, __FILE__ ": pa_simple_read() failed: %s\n",
                    pa_strerror(error));
            audio->terminate = 1;
        }

        pthread_mutex_lock(&lock);
        write_to_fftw_input_buffers(frames, buf, data);
        pthread_mutex_unlock(&lock);
    }

    pa_simple_free(s);
    pthread_exit(NULL);
    return 0;
}
