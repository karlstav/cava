#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <pulse/pulseaudio.h>
#define BUFSIZE 1024

pa_mainloop *m_pulseaudio_mainloop;

void cb(__attribute__((unused)) pa_context *pulseaudio_context, const pa_server_info *i, void *userdata){

	//getting default sink name
    struct audio_data *audio = (struct audio_data *)userdata;
	audio->source = malloc(sizeof(char) * 1024);

	strcpy(audio->source,i->default_sink_name);

	//appending .monitor suufix
	audio->source = strcat(audio->source, ".monitor");

	//quiting mainloop
    pa_context_disconnect(pulseaudio_context);
    pa_context_unref(pulseaudio_context);
	pa_mainloop_quit(m_pulseaudio_mainloop, 0);
    pa_mainloop_free(m_pulseaudio_mainloop);
}


void pulseaudio_context_state_callback(pa_context *pulseaudio_context,
                                                  void *userdata) {

	//make sure loop is ready	
	switch (pa_context_get_state(pulseaudio_context))
	{
	case PA_CONTEXT_UNCONNECTED:
	//printf("UNCONNECTED\n");
	break;
	case PA_CONTEXT_CONNECTING:
	//printf("CONNECTING\n");
	break;
	case PA_CONTEXT_AUTHORIZING:
	//printf("AUTHORIZING\n");
	break;
	case PA_CONTEXT_SETTING_NAME:
	//printf("SETTING_NAME\n");
	break;
	case PA_CONTEXT_READY://extract default sink name
	//printf("READY\n");
	pa_operation_unref(pa_context_get_server_info(
	pulseaudio_context, cb, userdata));
	break;
	case PA_CONTEXT_FAILED:
	//printf("FAILED\n");
	break;
	case PA_CONTEXT_TERMINATED:
	//printf("TERMINATED\n");
    pa_mainloop_quit(m_pulseaudio_mainloop, 0);
	break;	  
	}
}


void getPulseDefaultSink(void* data) {


    struct audio_data *audio = (struct audio_data *)data;
	pa_mainloop_api *mainloop_api;
	pa_context *pulseaudio_context;
	int ret;

	// Create a mainloop API and connection to the default server
	m_pulseaudio_mainloop = pa_mainloop_new();

	mainloop_api = pa_mainloop_get_api(m_pulseaudio_mainloop);
	pulseaudio_context = pa_context_new(mainloop_api, "cava device list");

	// This function connects to the pulse server
	pa_context_connect(pulseaudio_context, NULL, PA_CONTEXT_NOFLAGS,
			       NULL);


	//This function defines a callback so the server will tell us its state.
	pa_context_set_state_callback(pulseaudio_context,
			                  pulseaudio_context_state_callback,
			                  (void*)audio);

	//starting a mainloop to get default sink
	if (pa_mainloop_run(m_pulseaudio_mainloop, &ret) < 0)
	{
	printf("Could not open pulseaudio mainloop to "
			                  "find default device name: %d",
		   ret);
	}

}

int reconnect = 0;
pa_context *pulseaudio_track_context;
pa_threaded_mainloop *m_pulseaudio_track_mainloop;

void cb_track(__attribute__((unused)) pa_context *pulseaudio_context, pa_subscription_event_type_t t, __attribute__((unused)) uint32_t idx, __attribute__((unused)) void *userdata){
	if ((t & PA_SUBSCRIPTION_EVENT_FACILITY_MASK) == PA_SUBSCRIPTION_EVENT_SINK &&
		(t & PA_SUBSCRIPTION_EVENT_TYPE_MASK) == PA_SUBSCRIPTION_EVENT_NEW)
	{
		reconnect = 1;
	}
}

void pulseaudio_context_state_track_callback(pa_context *pulseaudio_context,
                                                  void *userdata) {
	//make sure loop is ready	
	switch (pa_context_get_state(pulseaudio_context))
	{
	case PA_CONTEXT_UNCONNECTED:
	//printf("UNCONNECTED\n");
	break;
	case PA_CONTEXT_CONNECTING:
	//printf("CONNECTING\n");
	break;
	case PA_CONTEXT_AUTHORIZING:
	//printf("AUTHORIZING\n");
	break;
	case PA_CONTEXT_SETTING_NAME:
	//printf("SETTING_NAME\n");
	break;
	case PA_CONTEXT_READY://extract default sink name
	//printf("READY\n");
	pa_context_set_subscribe_callback(
		pulseaudio_context, cb_track, userdata);
	pa_operation_unref(pa_context_subscribe(pulseaudio_context,
		PA_SUBSCRIPTION_MASK_SINK, NULL, NULL));
	break;
	case PA_CONTEXT_FAILED:
	//printf("FAILED\n");
	break;
	case PA_CONTEXT_TERMINATED:
	//printf("TERMINATED\n");
	pa_threaded_mainloop_stop(m_pulseaudio_track_mainloop);
	pa_threaded_mainloop_free(m_pulseaudio_track_mainloop);
	break;
	}
}

void trackPulseSinkChanges(void* data) {
    struct audio_data *audio = (struct audio_data *)data;
	pa_mainloop_api *mainloop_api;
	pa_context *pulseaudio_context;

	// Create a mainloop API and connection to the default server
	m_pulseaudio_track_mainloop = pa_threaded_mainloop_new();

	mainloop_api = pa_threaded_mainloop_get_api(m_pulseaudio_track_mainloop);
	pulseaudio_context = pa_context_new(mainloop_api, "cava device track");

	// This function connects to the pulse server
	pa_context_connect(pulseaudio_context, NULL, PA_CONTEXT_NOFLAGS,
			       NULL);

	//This function defines a callback so the server will tell us its state.
	pa_context_set_state_callback(pulseaudio_context,
			                  pulseaudio_context_state_track_callback,
			                  (void*)audio);

	//starting a mainloop to get default sink
	if (pa_threaded_mainloop_start(m_pulseaudio_track_mainloop) < 0)
	{
		printf("Could not open pulseaudio mainloop for tracking sinks.");
		exit(EXIT_FAILURE);
	}
}

void* input_pulse(void* data)
{

        struct audio_data *audio = (struct audio_data *)data;
        int i, n;
	int16_t buf[BUFSIZE / 2];

	/* The sample type to use */
	static const pa_sample_spec ss = {
		.format = PA_SAMPLE_S16LE,
		.rate =  44100,
		.channels = 2
		};
	static const pa_buffer_attr pb = {
	.maxlength = BUFSIZE * 2,
	.fragsize = BUFSIZE
	};

	pa_simple *s = NULL;
	int error;

	reconnect = 0;

	if (!(s = pa_simple_new(NULL, "cava", PA_STREAM_RECORD, audio->source, "audio for cava", &ss, NULL, &pb, &error))) {
		fprintf(stderr, __FILE__": Could not open pulseaudio source: %s, %s. To find a list of your pulseaudio sources run 'pacmd list-sources'\n",audio->source, pa_strerror(error));
		//exit(EXIT_FAILURE);
		reconnect = 1;
	}

	trackPulseSinkChanges((void*)&audio);

	n = 0;
               
	while (1) {
		if (reconnect > 0) {
			if (s) {
				pa_simple_free(s);
				s = NULL;
			}

			if (!(s = pa_simple_new(NULL, "cava", PA_STREAM_RECORD, audio->source, "audio for cava", &ss, NULL, &pb, &error))) {
				sleep(1);
				continue;
			}

			reconnect = 0;
		}
        	/* Record some data ... */
        	if (pa_simple_read(s, buf, sizeof(buf), &error) < 0) {
            		fprintf(stderr, __FILE__": pa_simple_read() failed: %s\n", pa_strerror(error));
        	exit(EXIT_FAILURE);
		}

		 //sorting out channels

	        for (i = 0; i < BUFSIZE / 2; i += 2) {

                                if (audio->channels == 1) audio->audio_out_l[n] = (buf[i] + buf[i + 1]) / 2;

                                //stereo storing channels in buffer
                                if (audio->channels == 2) {
                                        audio->audio_out_l[n] = buf[i];
                                        audio->audio_out_r[n] = buf[i + 1];
                                        }

                                n++;
                                if (n == 2048 - 1)n = 0;
                        }

		if (audio->terminate == 1) {            		
			pa_simple_free(s);
			break;
		    }
        }

	pa_threaded_mainloop_stop(m_pulseaudio_track_mainloop);
	pa_threaded_mainloop_free(m_pulseaudio_track_mainloop);
	pa_context_disconnect(pulseaudio_track_context);
	pa_context_unref(pulseaudio_track_context);

	return 0;
}
