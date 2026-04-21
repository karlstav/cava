#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>

#include <AudioUnit/AudioUnit.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>

#include "input/common.h"
#include "input/coreaudio.h"

struct coreaudio_state {
    AudioUnit audio_unit;
    AudioStreamBasicDescription format;
    unsigned char *buffer;
    UInt32 buffer_size;
    struct audio_data *audio;
};

static int get_channel_count(AudioDeviceID device, AudioObjectPropertyScope scope) {
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamConfiguration,
        scope,
        kAudioObjectPropertyElementMain,
    };

    UInt32 property_size = 0;
    OSStatus status = AudioObjectGetPropertyDataSize(device, &address, 0, NULL, &property_size);
    if (status != noErr || property_size == 0) {
        return 0;
    }

    AudioBufferList *buffer_list = (AudioBufferList *)malloc(property_size);
    if (buffer_list == NULL) {
        return 0;
    }

    status = AudioObjectGetPropertyData(device, &address, 0, NULL, &property_size, buffer_list);
    if (status != noErr) {
        free(buffer_list);
        return 0;
    }

    int channels = 0;
    for (UInt32 i = 0; i < buffer_list->mNumberBuffers; i++) {
        channels += (int)buffer_list->mBuffers[i].mNumberChannels;
    }

    free(buffer_list);
    return channels;
}

static bool copy_device_name(AudioDeviceID device, char *name, size_t name_size) {
    AudioObjectPropertyAddress address = {
        kAudioObjectPropertyName,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };

    CFStringRef cf_name = NULL;
    UInt32 property_size = sizeof(cf_name);
    OSStatus status =
        AudioObjectGetPropertyData(device, &address, 0, NULL, &property_size, &cf_name);
    if (status != noErr || cf_name == NULL) {
        return false;
    }

    Boolean ok = CFStringGetCString(cf_name, name, (CFIndex)name_size, kCFStringEncodingUTF8);
    CFRelease(cf_name);
    return ok == true;
}

static bool get_devices(AudioDeviceID **devices_out, UInt32 *count_out, bool include_input,
                        bool include_output) {
    AudioObjectPropertyAddress address = {
        kAudioHardwarePropertyDevices,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };

    UInt32 property_size = 0;
    OSStatus status =
        AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &address, 0, NULL, &property_size);
    if (status != noErr || property_size == 0) {
        return false;
    }

    UInt32 all_count = property_size / sizeof(AudioDeviceID);
    AudioDeviceID *all_devices = (AudioDeviceID *)malloc(property_size);
    if (all_devices == NULL) {
        return false;
    }

    status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &property_size,
                                        all_devices);
    if (status != noErr) {
        free(all_devices);
        return false;
    }

    AudioDeviceID *devices = (AudioDeviceID *)malloc(all_count * sizeof(AudioDeviceID));
    if (devices == NULL) {
        free(all_devices);
        return false;
    }

    UInt32 count = 0;
    for (UInt32 i = 0; i < all_count; i++) {
        int in_channels = get_channel_count(all_devices[i], kAudioDevicePropertyScopeInput);
        int out_channels = get_channel_count(all_devices[i], kAudioDevicePropertyScopeOutput);
        if ((include_input && in_channels > 0) || (include_output && out_channels > 0)) {
            devices[count++] = all_devices[i];
        }
    }

    free(all_devices);

    *devices_out = devices;
    *count_out = count;
    return true;
}

static void list_input_devices(void) {
    AudioDeviceID *devices = NULL;
    UInt32 count = 0;

    if (!get_devices(&devices, &count, true, true) || count == 0) {
        fprintf(stderr, "Error: no Core Audio devices found\n");
        exit(EXIT_FAILURE);
    }

    for (UInt32 i = 0; i < count; i++) {
        char name[256] = {0};
        if (!copy_device_name(devices[i], name, sizeof(name))) {
            snprintf(name, sizeof(name), "<unavailable>");
        }

        printf("Device #%u: %s\n", i + 1, name);
        printf("\tCoreAudioID: %u\n", (unsigned int)devices[i]);
        printf("\tInput Channels: %d\n",
               get_channel_count(devices[i], kAudioDevicePropertyScopeInput));
        printf("\tOutput Channels: %d\n",
               get_channel_count(devices[i], kAudioDevicePropertyScopeOutput));
    }

    free(devices);
}

static bool get_default_device(UInt32 selector, AudioDeviceID *device_out) {
    AudioObjectPropertyAddress address = {
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };
    UInt32 size = sizeof(AudioDeviceID);
    OSStatus status =
        AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL, &size, device_out);
    return status == noErr && *device_out != kAudioObjectUnknown;
}

static bool select_input_device(const char *source, AudioDeviceID *device_out) {
    if (strcmp(source, "auto") == 0 || strcmp(source, "auto_output") == 0 ||
        strcmp(source, "default_output") == 0) {
        return get_default_device(kAudioHardwarePropertyDefaultOutputDevice, device_out);
    }

    if (strcmp(source, "auto_input") == 0 || strcmp(source, "default_input") == 0) {
        AudioObjectPropertyAddress address = {
            kAudioHardwarePropertyDefaultInputDevice,
            kAudioObjectPropertyScopeGlobal,
            kAudioObjectPropertyElementMain,
        };
        UInt32 size = sizeof(AudioDeviceID);
        OSStatus status = AudioObjectGetPropertyData(kAudioObjectSystemObject, &address, 0, NULL,
                                                     &size, device_out);
        return status == noErr && *device_out != kAudioObjectUnknown;
    }

    AudioDeviceID *devices = NULL;
    UInt32 count = 0;
    if (!get_devices(&devices, &count, true, true) || count == 0) {
        return false;
    }

    char *endptr = NULL;
    long index = strtol(source, &endptr, 10);
    if (endptr != source && *endptr == '\0') {
        if (index >= 1 && (UInt32)index <= count) {
            *device_out = devices[index - 1];
            free(devices);
            return true;
        }
        free(devices);
        return false;
    }

    for (UInt32 i = 0; i < count; i++) {
        char name[256] = {0};
        if (copy_device_name(devices[i], name, sizeof(name)) && strcmp(name, source) == 0) {
            *device_out = devices[i];
            free(devices);
            return true;
        }
    }

    free(devices);
    return false;
}

static bool device_has_output_channels(AudioDeviceID device) {
    return get_channel_count(device, kAudioDevicePropertyScopeOutput) > 0;
}

static OSStatus input_callback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags,
                               const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber,
                               UInt32 inNumberFrames, AudioBufferList *ioData) {
    (void)inBusNumber;
    (void)ioData;

    struct coreaudio_state *state = (struct coreaudio_state *)inRefCon;

    if (state->audio->terminate == 1) {
        return noErr;
    }

    UInt32 bytes_needed = inNumberFrames * state->format.mBytesPerFrame;
    if (bytes_needed > state->buffer_size) {
        unsigned char *new_buffer = (unsigned char *)realloc(state->buffer, bytes_needed);
        if (new_buffer == NULL) {
            return kAudioUnitErr_NoConnection;
        }
        state->buffer = new_buffer;
        state->buffer_size = bytes_needed;
    }

    AudioBufferList input;
    input.mNumberBuffers = 1;
    input.mBuffers[0].mNumberChannels = state->format.mChannelsPerFrame;
    input.mBuffers[0].mDataByteSize = bytes_needed;
    input.mBuffers[0].mData = state->buffer;

    OSStatus status =
        AudioUnitRender(state->audio_unit, ioActionFlags, inTimeStamp, 1, inNumberFrames, &input);
    if (status != noErr) {
        return status;
    }

    write_to_cava_input_buffers((int16_t)(inNumberFrames * state->audio->channels), state->buffer,
                                state->audio);

    return noErr;
}

static bool configure_stream_format(struct coreaudio_state *state) {
    struct audio_data *audio = state->audio;

    audio->channels = audio->channels < 1 ? 1 : audio->channels;
    if (audio->channels > 2) {
        audio->channels = 2;
    }

    if (audio->format != 8 && audio->format != 16 && audio->format != 24 && audio->format != 32) {
        audio->format = 16;
    }

    state->format.mSampleRate = (Float64)audio->rate;
    state->format.mFormatID = kAudioFormatLinearPCM;
    state->format.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    state->format.mBitsPerChannel = (UInt32)audio->format;
    state->format.mChannelsPerFrame = (UInt32)audio->channels;
    state->format.mFramesPerPacket = 1;
    state->format.mBytesPerFrame =
        (state->format.mBitsPerChannel / 8) * state->format.mChannelsPerFrame;
    state->format.mBytesPerPacket = state->format.mBytesPerFrame;

    OSStatus status =
        AudioUnitSetProperty(state->audio_unit, kAudioUnitProperty_StreamFormat,
                             kAudioUnitScope_Output, 1, &state->format, sizeof(state->format));
    if (status == noErr) {
        audio->IEEE_FLOAT = 0;
        return true;
    }

    state->format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    state->format.mBitsPerChannel = 32;
    state->format.mBytesPerFrame = 4 * state->format.mChannelsPerFrame;
    state->format.mBytesPerPacket = state->format.mBytesPerFrame;

    status = AudioUnitSetProperty(state->audio_unit, kAudioUnitProperty_StreamFormat,
                                  kAudioUnitScope_Output, 1, &state->format, sizeof(state->format));
    if (status != noErr) {
        return false;
    }

    audio->format = 32;
    audio->IEEE_FLOAT = 1;
    return true;
}

void *input_coreaudio(void *audiodata) {
    struct audio_data *audio = (struct audio_data *)audiodata;
    struct coreaudio_state state;
    memset(&state, 0, sizeof(state));
    state.audio = audio;

    if (strcmp(audio->source, "list") == 0) {
        list_input_devices();
        printf("Use 'source = auto' for the default output device, 'auto_input' for the default "
               "input device, an index from this list, or an exact device name.\n");
#ifdef COREAUDIO_TAP
        printf("Use 'source = tap' (or 'tap_mono') to capture system output directly via CoreAudio "
               "taps (macOS 14.2+).\n");
#endif
        exit(EXIT_SUCCESS);
    }

    AudioDeviceID device = kAudioObjectUnknown;
    if (!select_input_device(audio->source, &device)) {
        fprintf(stderr,
                "Error: could not find Core Audio device '%s'. Use source='list' to inspect "
                "available devices.\n",
                audio->source);
        exit(EXIT_FAILURE);
    }

    int device_input_channels = get_channel_count(device, kAudioDevicePropertyScopeInput);
    if (device_input_channels == 0) {
        if (device_has_output_channels(device)) {
            fprintf(stderr,
                    "Error: selected Core Audio device has output channels but no capture stream. "
                    "On macOS, output mix capture still requires a loopback-capable device.\n");
        } else {
            fprintf(stderr, "Error: selected Core Audio device has no input channels.\n");
        }
        exit(EXIT_FAILURE);
    }

    // Use the device's actual channel count, clamped to stereo.
    // This prevents a mono device from being incorrectly programmed as stereo,
    // which would produce a stereo view with only the left channel populated.
    if (device_input_channels < (int)audio->channels) {
        audio->channels = (unsigned int)device_input_channels;
    }
    if (audio->channels > 2) {
        audio->channels = 2;
    }

    AudioObjectPropertyAddress nominal_rate_address = {
        kAudioDevicePropertyNominalSampleRate,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };
    UInt32 nominal_size = sizeof(Float64);
    Float64 nominal_rate = 0;
    if (AudioObjectGetPropertyData(device, &nominal_rate_address, 0, NULL, &nominal_size,
                                   &nominal_rate) == noErr &&
        nominal_rate > 0) {
        audio->rate = (unsigned int)nominal_rate;
    }

    AudioComponentDescription desc;
    memset(&desc, 0, sizeof(desc));
    desc.componentType = kAudioUnitType_Output;
    desc.componentSubType = kAudioUnitSubType_HALOutput;
    desc.componentManufacturer = kAudioUnitManufacturer_Apple;

    AudioComponent component = AudioComponentFindNext(NULL, &desc);
    if (component == NULL) {
        fprintf(stderr, "Error: could not create Core Audio component\n");
        exit(EXIT_FAILURE);
    }

    OSStatus status = AudioComponentInstanceNew(component, &state.audio_unit);
    if (status != noErr) {
        fprintf(stderr, "Error: could not instantiate Core Audio unit (status %d)\n", (int)status);
        exit(EXIT_FAILURE);
    }

    UInt32 enable_input = 1;
    UInt32 disable_output = 0;
    status = AudioUnitSetProperty(state.audio_unit, kAudioOutputUnitProperty_EnableIO,
                                  kAudioUnitScope_Input, 1, &enable_input, sizeof(enable_input));
    if (status == noErr) {
        status = AudioUnitSetProperty(state.audio_unit, kAudioOutputUnitProperty_EnableIO,
                                      kAudioUnitScope_Output, 0, &disable_output,
                                      sizeof(disable_output));
    }
    if (status != noErr) {
        fprintf(stderr, "Error: could not configure Core Audio IO mode (status %d)\n", (int)status);
        AudioComponentInstanceDispose(state.audio_unit);
        exit(EXIT_FAILURE);
    }

    status = AudioUnitSetProperty(state.audio_unit, kAudioOutputUnitProperty_CurrentDevice,
                                  kAudioUnitScope_Global, 0, &device, sizeof(device));
    if (status != noErr) {
        fprintf(stderr, "Error: could not select Core Audio input device (status %d)\n",
                (int)status);
        AudioComponentInstanceDispose(state.audio_unit);
        exit(EXIT_FAILURE);
    }

    if (!configure_stream_format(&state)) {
        fprintf(stderr, "Error: could not set Core Audio stream format\n");
        AudioComponentInstanceDispose(state.audio_unit);
        exit(EXIT_FAILURE);
    }

    AURenderCallbackStruct callback;
    callback.inputProc = input_callback;
    callback.inputProcRefCon = &state;
    status = AudioUnitSetProperty(state.audio_unit, kAudioOutputUnitProperty_SetInputCallback,
                                  kAudioUnitScope_Global, 0, &callback, sizeof(callback));
    if (status != noErr) {
        fprintf(stderr, "Error: could not set Core Audio input callback (status %d)\n",
                (int)status);
        AudioComponentInstanceDispose(state.audio_unit);
        exit(EXIT_FAILURE);
    }

    UInt32 max_frames = audio->input_buffer_size / (unsigned int)audio->channels;
    status = AudioUnitSetProperty(state.audio_unit, kAudioUnitProperty_MaximumFramesPerSlice,
                                  kAudioUnitScope_Global, 0, &max_frames, sizeof(max_frames));
    if (status != noErr) {
        fprintf(stderr, "Error: could not set Core Audio max frames (status %d)\n", (int)status);
        AudioComponentInstanceDispose(state.audio_unit);
        exit(EXIT_FAILURE);
    }

    status = AudioUnitInitialize(state.audio_unit);
    if (status != noErr) {
        fprintf(stderr, "Error: could not initialize Core Audio unit (status %d)\n", (int)status);
        AudioComponentInstanceDispose(state.audio_unit);
        exit(EXIT_FAILURE);
    }

    status = AudioOutputUnitStart(state.audio_unit);
    if (status != noErr) {
        fprintf(stderr, "Error: could not start Core Audio input (status %d)\n", (int)status);
        AudioUnitUninitialize(state.audio_unit);
        AudioComponentInstanceDispose(state.audio_unit);
        exit(EXIT_FAILURE);
    }

    signal_threadparams(audio);

    while (audio->terminate != 1) {
        usleep(100000);
    }

    AudioOutputUnitStop(state.audio_unit);
    AudioUnitUninitialize(state.audio_unit);
    AudioComponentInstanceDispose(state.audio_unit);
    free(state.buffer);

    return NULL;
}
