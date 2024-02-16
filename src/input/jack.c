#include <stdbool.h>
#include <time.h>

#include <jack/jack.h>

#include "cava/input/common.h"
#include "cava/input/jack.h"

// CAVA is hard-coded to a maximum of 2 channels, i.e. stereo.
#define MAX_CHANNELS 2

typedef jack_default_audio_sample_t sample_t;

struct jack_data {
    struct audio_data *audio; // CAVA ctx

    jack_client_t *client;           // JACK client
    jack_port_t *port[MAX_CHANNELS]; // input ports
    jack_nframes_t nframes;          // number of samples per port
    sample_t *buf;                   // samples buffer

    int graphorder; // JACK graph ordering signal (0=unchanged, 1=changed)
    int shutdown;   // JACK shutdown signal (0=keep, 1=shutdown)
};

static bool set_rate(struct jack_data *jack) {
    // Query sample rate from JACK server. If CAVA doesn't support the final value then it will
    // complain later.
    jack_nframes_t rate = jack_get_sample_rate(jack->client);

    if (rate <= 0) {
        fprintf(stderr, __FILE__ ": jack_get_sample_rate() failed.\n");
        return false;
    }

    jack->audio->rate = rate;

    return true;
}

static bool set_format(struct jack_data *jack) {
    // JACK returns 32bit float data.
    jack->audio->format = 32;
    jack->audio->IEEE_FLOAT = 1;
    return true;
}

static bool set_channels(struct jack_data *jack) {
    // Try to create terminal audio-typed input ports for the requested number of channels. These
    // ports can receive the audio data from other JACK clients.
    static const char *port_name[MAX_CHANNELS][MAX_CHANNELS] = {/* mono */ {"M", NULL},
                                                                /* stereo */ {"L", "R"}};
    static const char port_type[] = JACK_DEFAULT_AUDIO_TYPE;
    static const unsigned long flags = JackPortIsInput | JackPortIsTerminal;

    int channels;
    int chtype;
    int ch;

    // Limit the requested channels in case CAVA becomes surround-aware and MAX_CHANNELS hasn't
    // adapted yet.
    channels = jack->audio->channels > MAX_CHANNELS ? MAX_CHANNELS : jack->audio->channels;

    // Determines the row in the 'port_name' table, i.e. mono, stereo, etc...
    chtype = channels - 1;

    // Try to create a port for every requested channel. After the for-loop the variable 'ch' holds
    // the number of actual created ports.
    for (ch = 0; ch < channels; ++ch) {
        if ((jack->port[ch] = jack_port_register(jack->client, port_name[chtype][ch], port_type,
                                                 flags, 0)) == NULL)
            break;
    }

    // If not even one port was created, then there was a deeper problem.
    if (ch == 0) {
        fprintf(stderr, __FILE__ ": jack_port_register('%s') failed.\n", port_name[chtype][0]);
        return false;
    }

    // If less ports were created than channels requested, then the JACK server is probably not
    // configured for the requested channels, e.g. we requested stereo but the server only provides
    // mono. In this case we rename the created ports according to the resulting table row.
    if (ch < channels) {
        int chtype_new;

        channels = ch;
        chtype_new = channels - 1;

        for (ch = 0; ch < channels; ++ch) {
            int err;

            if ((err = jack_port_rename(jack->client, jack->port[ch], port_name[chtype_new][ch])) !=
                0) {
                fprintf(stderr, __FILE__ ": jack_port_rename('%s', '%s') failed: 0x%x\n",
                        port_name[chtype][ch], port_name[chtype_new][ch], err);
                return false;
            }
        }
    }

    jack->audio->channels = channels;

    return true;
}

static int on_buffer_size(jack_nframes_t nframes, void *arg) {
    // Buffersize changes should never happen in CAVA!
    struct jack_data *jack = arg;

    if ((jack->shutdown == 1) || (jack->audio->terminate == 1))
        return 0;

    if (jack->nframes != nframes) {
        fprintf(stderr, __FILE__ ": Unexpected change of JACK port buffersize! Aborting!\n");
        jack->shutdown = 1;
        return 1;
    }

    return 0;
}

static int on_graph_order(void *arg) {
    ((struct jack_data *)arg)->graphorder = 1;
    return 0;
}

static int on_process(jack_nframes_t nframes, void *arg) {
    // Interleave samples from separate ports and feed them to CAVA.
    struct jack_data *jack = arg;
    sample_t *buf[MAX_CHANNELS];
    unsigned char *buf_cava;

    if ((jack->shutdown == 1) || (jack->audio->terminate == 1))
        return 0;

    for (unsigned int i = 0; i < jack->audio->channels; ++i) {
        if ((buf[i] = jack_port_get_buffer(jack->port[i], nframes)) == NULL) {
            fprintf(stderr, __FILE__ ": jack_port_get_buffer('%s') failed.\n",
                    jack_port_name(jack->port[i]));
            jack->shutdown = 1;
            return 1;
        }
    }

    switch (jack->audio->channels) {
    case 1:
        // If mono then no interleaving needed, feed into CAVA directly.
        buf_cava = (unsigned char *)buf[0];
        break;
    case 2:
        // If stereo then unroll interleaving manually.
        for (jack_nframes_t i = 0; i < nframes; ++i) {
            jack->buf[2 * i + 0] = buf[0][i];
            jack->buf[2 * i + 1] = buf[1][i];
        }

        buf_cava = (unsigned char *)jack->buf;
        break;
    default:
        // Else to the loops.
        for (jack_nframes_t i = 0; i < nframes; ++i) {
            for (unsigned int j = 0; j < jack->audio->channels; ++j)
                jack->buf[jack->audio->channels * i + j] = buf[j][i];
        }

        buf_cava = (unsigned char *)jack->buf;
        break;
    }

    write_to_cava_input_buffers(nframes * jack->audio->channels, buf_cava, jack->audio);

    return 0;
}

static int on_sample_rate(jack_nframes_t nframes, void *arg) {
    // Sample rate changes are not supported in CAVA!
    struct jack_data *jack = arg;

    if ((jack->shutdown == 1) || (jack->audio->terminate == 1))
        return 0;

    if (jack->audio->rate != nframes) {
        fprintf(stderr, __FILE__ ": Unexpected change of JACK sample rate! Aborting!\n");
        jack->shutdown = 1;
        return 1;
    }

    return 0;
}

static void on_shutdown(void *arg) { ((struct jack_data *)arg)->shutdown = 1; }

static bool auto_connect(struct jack_data *jack) {
    // Get all physical terminal input-ports and mirror their connections to CAVA.
    static const char type_name_pattern[] = JACK_DEFAULT_AUDIO_TYPE;
    static const unsigned long flags = JackPortIsInput | JackPortIsPhysical | JackPortIsTerminal;

    unsigned int channels;

    const char **ports = NULL;

    bool success = false;

    if ((jack->shutdown == 1) || (jack->audio->terminate == 1))
        return true;

    if ((ports = jack_get_ports(jack->client, NULL, type_name_pattern, flags)) == NULL) {
        fprintf(stderr,
                __FILE__ ": jack_get_ports() failed: No physical terminal input-ports found!\n");
        goto cleanup;
    }

    // If CAVA is configured for mono, then we connect everything to the one mono port. If we have
    // more channels, then we limit the number of connection ports to the number of channels.
    for (channels = 0; ports[channels] != NULL; ++channels)
        ;

    if ((jack->audio->channels > 1) && (channels > jack->audio->channels))
        channels = jack->audio->channels;

    // Visit the physical terminal input-ports, get their connections and apply them to CAVA's
    // input-ports.
    for (unsigned int i = 0; i < channels; ++i) {
        const char **connections;
        jack_port_t *port;
        const char *port_name;

        if ((connections = jack_port_get_all_connections(
                 jack->client, jack_port_by_name(jack->client, ports[i]))) == NULL)
            continue;

        if (jack->audio->channels == 1)
            port = jack->port[0];
        else
            port = jack->port[i];

        port_name = jack_port_name(port);

        for (int j = 0; connections[j] != NULL; ++j) {
            if (jack_port_connected_to(port, connections[j]) == 0)
                jack_connect(jack->client, connections[j], port_name);
        }

        jack_free(connections);
    }

    success = true;

cleanup:
    if (!success)
        jack->shutdown = 1;

    jack_free(ports);

    return success;
}

void *input_jack(void *data) {
    static const char client_name[] = "cava";
    static const jack_options_t options = JackNullOption | JackServerName;
    static const struct timespec rqtp = {.tv_sec = 0, .tv_nsec = 1000000}; // 1ms nsleep

    struct audio_data *audio = data;
    char *server_name;
    jack_status_t status;
    int err;

    struct jack_data jack = {0};

    bool is_jack_activated = false;
    bool success = false;

    jack.audio = audio;

    // JACK server selection by source or implicit default.
    if (strlen(audio->source) == 0)
        server_name = NULL;
    else
        server_name = audio->source;

    if ((jack.client = jack_client_open(client_name, options, &status, server_name)) == NULL) {
        fprintf(stderr, __FILE__ ": Could not open JACK source '%s': 0x%x\n", server_name, status);
        goto cleanup;
    }

    if (!set_rate(&jack) || !set_format(&jack) || !set_channels(&jack))
        goto cleanup;

    // Parameters finalized. Signal main thread.
    signal_threadparams(audio);

    // JACK returns samples per channel. Adjust its buffersize to fit within CAVA.
    // Must be a power of 2.
    jack.nframes = 1 << 31;

    while (jack.nframes > audio->input_buffer_size / audio->channels)
        jack.nframes >>= 1;

    if ((err = jack_set_buffer_size(jack.client, jack.nframes)) != 0) {
        fprintf(stderr, __FILE__ ": jack_set_buffer_size() failed: 0x%x\n", err);
        goto cleanup;
    }

    // Work buffer for interleaving if not mono.
    if ((audio->channels > 1) &&
        ((jack.buf = malloc(jack.nframes * audio->channels * sizeof(sample_t))) == NULL)) {
        fprintf(stderr, __FILE__ ": malloc() failed: %s\n", strerror(errno));
        goto cleanup;
    }

    // Set JACK callbacks before JACK activation.
    if ((err = jack_set_buffer_size_callback(jack.client, on_buffer_size, &jack)) != 0) {
        fprintf(stderr, __FILE__ ": jack_set_buffer_size_callback() failed: 0x%x\n", err);
        goto cleanup;
    }

    if (audio->autoconnect > 0) {
        if (audio->autoconnect == 1)
            jack.graphorder = 1;
        else {
            if ((err = jack_set_graph_order_callback(jack.client, on_graph_order, &jack)) != 0) {
                fprintf(stderr, __FILE__ ": jack_set_graph_order_callback() failed: 0x%x\n", err);
                goto cleanup;
            }
        }
    }

    if ((err = jack_set_process_callback(jack.client, on_process, &jack)) != 0) {
        fprintf(stderr, __FILE__ ": jack_set_process_callback() failed: 0x%x\n", err);
        goto cleanup;
    }

    if ((err = jack_set_sample_rate_callback(jack.client, on_sample_rate, &jack)) != 0) {
        fprintf(stderr, __FILE__ ": jack_set_sample_rate_callback() failed: 0x%x\n", err);
        goto cleanup;
    }

    jack_on_shutdown(jack.client, on_shutdown, &jack);

    if ((err = jack_activate(jack.client)) != 0) {
        fprintf(stderr, __FILE__ ": jack_activate() failed: 0x%x\n", err);
        goto cleanup;
    }

    is_jack_activated = true;

    while (audio->terminate != 1) {
        if (jack.shutdown == 1)
            signal_terminate(audio);
        else if (jack.graphorder == 1) {
            if (!auto_connect(&jack))
                goto cleanup;

            jack.graphorder = 0;
        }

        nanosleep(&rqtp, NULL);
    }

    success = true;

cleanup:
    if (is_jack_activated && ((err = jack_deactivate(jack.client)) != 0)) {
        fprintf(stderr, __FILE__ ": jack_deactivate() failed: 0x%x\n", err);
        success = false;
    }

    free(jack.buf);

    for (int i = 0; i < MAX_CHANNELS; ++i) {
        if ((jack.port[i] != NULL) &&
            ((err = jack_port_unregister(jack.client, jack.port[i])) != 0)) {
            fprintf(stderr, __FILE__ ": jack_port_unregister('%s') failed: 0x%x\n",
                    jack_port_name(jack.port[i]), err);
            success = false;
        }
    }

    if ((jack.client != NULL) && ((err = jack_client_close(jack.client)) != 0)) {
        fprintf(stderr, __FILE__ ": jack_client_close() failed: 0x%x\n", err);
        success = false;
    }

    signal_threadparams(audio);
    signal_terminate(audio);

    if (!success)
        exit(EXIT_FAILURE);

    return NULL;
}
