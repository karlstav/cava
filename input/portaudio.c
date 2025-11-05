#include "portaudio.h"

#include <portaudio.h>

#define SAMPLE_SILENCE -32767
typedef short SAMPLE;

typedef struct {
    int frameIndex; /* Index into sample array. */
    int maxFrameIndex;
    SAMPLE *recordedSamples;
} paTestData;

static struct audio_data *audio;
int16_t silence_buffer[8092] = {SAMPLE_SILENCE};

static int recordCallback(const void *inputBuffer, void *outputBuffer,
                          unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo,
                          PaStreamCallbackFlags statusFlags, void *userData) {
    paTestData *data = (paTestData *)userData;
    unsigned char *rptr = (unsigned char *)inputBuffer;
    unsigned char *silence_ptr = (unsigned char *)silence_buffer;
    long framesToCalc;
    int finished;
    unsigned long framesLeft = data->maxFrameIndex - data->frameIndex;
    (void)outputBuffer; // Prevent unused variable warnings.
    (void)timeInfo;
    (void)statusFlags;
    (void)userData;

    if (framesLeft < framesPerBuffer) {
        framesToCalc = framesLeft;
        finished = paComplete;
    } else {
        framesToCalc = framesPerBuffer;
        finished = paContinue;
    }

    if (inputBuffer == NULL)
        write_to_cava_input_buffers(framesToCalc * audio->channels, silence_ptr, audio);
    else
        write_to_cava_input_buffers(framesToCalc * audio->channels, rptr, audio);

    data->frameIndex += framesToCalc;
    if (finished == paComplete) {
        data->frameIndex = 0;
        finished = paContinue;
    }

    if (audio->terminate == 1)
        finished = paComplete;

    return finished;
}

void portaudio_simple_free(paTestData data) {
    Pa_Terminate();
    free(data.recordedSamples);
}

void *input_portaudio(void *audiodata) {
    audio = (struct audio_data *)audiodata;

    PaStreamParameters inputParameters;
    PaStream *stream;
    PaError err = paNoError;
    paTestData data;

    // start portaudio
    err = Pa_Initialize();
    if (err != paNoError) {
        fprintf(stderr, "Error: unable to initilize portaudio - %s\n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }

    // get portaudio device
    int deviceNum = -1, numOfDevices = Pa_GetDeviceCount();
    if (!strcmp(audio->source, "list")) {
        if (numOfDevices < 0) {
            fprintf(stderr, "Error: portaudio was unable to find a audio device! Code: 0x%x\n",
                    numOfDevices);
            exit(EXIT_FAILURE);
        }
        for (int i = 0; i < numOfDevices; i++) {
            const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
            printf("Device #%d: %s\n"
                   "\tInput Channels: %d\n"
                   "\tOutput Channels: %d\n"
                   "\tDefault SampleRate: %lf\n",
                   i + 1, deviceInfo->name, deviceInfo->maxInputChannels,
                   deviceInfo->maxOutputChannels, deviceInfo->defaultSampleRate);
        }
        printf("See cava readme for more information on how to capture audio.\n");
        exit(EXIT_SUCCESS);
    } else if (!strcmp(audio->source, "auto")) {
        deviceNum = Pa_GetDefaultInputDevice();

        if (deviceNum == paNoDevice) {
            fprintf(stderr, "Error: no portaudio input device found\n");
            exit(EXIT_FAILURE);
        }
    } else if (sscanf(audio->source, "%d", &deviceNum)) {
        if (deviceNum > numOfDevices) {
            fprintf(stderr, "Error: Invalid input device!\n");
            exit(EXIT_FAILURE);
        }
        deviceNum--;
    } else {
        for (int i = 0; i < numOfDevices; i++) {
            const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
            if (!strcmp(audio->source, deviceInfo->name)) {
                deviceNum = i;
                break;
            }
        }
        if (deviceNum == -1) {
            fprintf(stderr, "Error: No such device '%s'!\n", audio->source);
            exit(EXIT_FAILURE);
        }
    }
    inputParameters.device = deviceNum;
    const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(deviceNum);
    if (deviceInfo->maxInputChannels == 0) {
        fprintf(stderr, "Error: selected device has no input channels!\n Use \"list\" as source to "
                        "get a list of available sources.\n");
        exit(EXIT_FAILURE);
    }

    // set parameters
    inputParameters.channelCount = deviceInfo->maxInputChannels;
    audio->channels = deviceInfo->maxInputChannels;
    if (audio->channels > 2)
        audio->channels = 2;

    data.maxFrameIndex = audio->input_buffer_size * 1024 / audio->channels;
    data.recordedSamples = (SAMPLE *)malloc(2 * data.maxFrameIndex * sizeof(SAMPLE));
    if (data.recordedSamples == NULL) {
        fprintf(stderr, "Error: failure in memory allocation!\n");
        exit(EXIT_FAILURE);
    } else
        memset(data.recordedSamples, 0x00, 2 * data.maxFrameIndex);

    double sampleRate = deviceInfo->defaultSampleRate;
    audio->rate = sampleRate;

    PaSampleFormat sampleFormats[] = {paInt16, paInt24, paInt32, paFloat32,
                                      paInt8,  paUInt8, paInt16};
    int sampleBits[] = {
        16, 24, 32, 32, 8, 8,
    };

    for (int i = 0; i < 7; i++) {
        inputParameters.sampleFormat = sampleFormats[i];
        PaError err = Pa_IsFormatSupported(&inputParameters, NULL, sampleRate);
        if (err == paFormatIsSupported) {
            audio->format = sampleBits[i];
            if (i == 3)
                audio->IEEE_FLOAT = 1;
            break;
        }
    }

    inputParameters.suggestedLatency =
        Pa_GetDeviceInfo(inputParameters.device)->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = NULL;

    // set it to work
    err =
        Pa_OpenStream(&stream, &inputParameters, NULL, sampleRate,
                      audio->input_buffer_size / audio->channels, paClipOff, recordCallback, &data);
    if (err != paNoError) {
        fprintf(stderr,
                "Error: failure in opening stream (device: %d), (error: %s). Use \"list\" as souce "
                "to get a list of "
                "available sources.\n",
                deviceNum + 1, Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }

    audio->threadparams = 0;

    // main loop
    while (1) {
        // start recording
        data.frameIndex = 0;
        err = Pa_StartStream(stream);
        if (err != paNoError) {
            fprintf(stderr, "Error: failure in starting stream (%s)\n", Pa_GetErrorText(err));
            exit(EXIT_FAILURE);
        }

        //  record
        while ((err = Pa_IsStreamActive(stream)) == 1) {
            Pa_Sleep(1000);
            if (audio->terminate == 1)
                break;
        }
        // check for errors
        if (err < 0) {
            fprintf(stderr, "Error: failure in recording audio (%s)\n", Pa_GetErrorText(err));
            exit(EXIT_FAILURE);
        }

        // check if it bailed
        if (audio->terminate == 1)
            break;
    }
    // close stream
    if ((err = Pa_CloseStream(stream)) != paNoError) {
        fprintf(stderr, "Error: failure in closing stream (%s)\n", Pa_GetErrorText(err));
        exit(EXIT_FAILURE);
    }

    portaudio_simple_free(data);
    return 0;
}
