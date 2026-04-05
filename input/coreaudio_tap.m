#import <Foundation/Foundation.h>

#include <CoreAudio/AudioHardware.h>
#include <CoreAudio/AudioHardwareTapping.h>
#include <CoreAudio/CATapDescription.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "input/common.h"
#include "input/coreaudio_tap.h"

#ifndef MAC_OS_VERSION_14_2
#define MAC_OS_VERSION_14_2 140200
#endif

#if __MAC_OS_X_VERSION_MAX_ALLOWED < MAC_OS_VERSION_14_2
int coreaudio_tap_source_enabled(const char *source) {
    (void)source;
    return 0;
}

void *input_coreaudio_tap(void *data) {
    struct audio_data *audio = (struct audio_data *)data;
    fprintf(stderr,
            "CoreAudio taps require macOS 14.2+ SDK support. Rebuild on a newer SDK or use source=auto with a loopback-capable device.\n");
    signal_threadparams(audio);
    signal_terminate(audio);
    return NULL;
}

#else

struct tap_state {
    struct audio_data *audio;
    AudioObjectID tap_id;
    AudioObjectID aggregate_device_id;
    AudioDeviceIOProcID io_proc_id;
    AudioStreamBasicDescription asbd;
    unsigned char *interleaved_buffer;
    size_t interleaved_buffer_size;
    bool io_running;
    bool io_proc_created;
    bool aggregate_created;
    bool tap_created;
};

static CFStringRef cfstring_from_cstr(const char *value) {
    return CFStringCreateWithCString(kCFAllocatorDefault, value, kCFStringEncodingUTF8);
}

static OSStatus get_device_input_asbd(AudioObjectID device, AudioStreamBasicDescription *asbd) {
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyStreamFormat,
        kAudioDevicePropertyScopeInput,
        kAudioObjectPropertyElementMain,
    };

    UInt32 size = sizeof(*asbd);
    return AudioObjectGetPropertyData(device, &address, 0, NULL, &size, asbd);
}

static OSStatus get_device_nominal_rate(AudioObjectID device, Float64 *rate) {
    AudioObjectPropertyAddress address = {
        kAudioDevicePropertyNominalSampleRate,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMain,
    };

    UInt32 size = sizeof(*rate);
    return AudioObjectGetPropertyData(device, &address, 0, NULL, &size, rate);
}

static bool source_is(const char *source, const char *value) {
    return strcmp(source, value) == 0;
}

int coreaudio_tap_source_enabled(const char *source) {
    if (source == NULL) {
        return 0;
    }

    return source_is(source, "tap") || source_is(source, "tap_mono") ||
           source_is(source, "tap_stereo") || source_is(source, "tap_system") ||
           source_is(source, "tap_system_mono");
}

static CATapDescription *build_tap_description(const char *source) {
    NSArray<NSNumber *> *excluded = @[];

    if (source_is(source, "tap_mono") || source_is(source, "tap_system_mono")) {
        return [[CATapDescription alloc] initMonoGlobalTapButExcludeProcesses:excluded];
    }

    return [[CATapDescription alloc] initStereoGlobalTapButExcludeProcesses:excluded];
}

static bool copy_buffer_interleaved(const AudioBufferList *input, struct tap_state *state,
                                    UInt32 bytes_per_sample, UInt32 channels, UInt32 *out_samples,
                                    unsigned char **out_data) {
    if (input == NULL || input->mNumberBuffers == 0 || channels == 0) {
        return false;
    }

    if (input->mNumberBuffers == 1) {
        const AudioBuffer *buffer = &input->mBuffers[0];
        if (buffer->mData == NULL || buffer->mDataByteSize == 0) {
            return false;
        }

        *out_samples = buffer->mDataByteSize / bytes_per_sample;
        *out_data = (unsigned char *)buffer->mData;
        return true;
    }

    if (input->mNumberBuffers < channels || channels > 2) {
        return false;
    }

    const AudioBuffer *ch0 = &input->mBuffers[0];
    if (ch0->mData == NULL || ch0->mDataByteSize == 0) {
        return false;
    }

    UInt32 frames = ch0->mDataByteSize / bytes_per_sample;
    size_t total_bytes = (size_t)frames * channels * bytes_per_sample;
    if (total_bytes > state->interleaved_buffer_size) {
        unsigned char *new_buffer = (unsigned char *)realloc(state->interleaved_buffer, total_bytes);
        if (new_buffer == NULL) {
            return false;
        }
        state->interleaved_buffer = new_buffer;
        state->interleaved_buffer_size = total_bytes;
    }

    for (UInt32 frame = 0; frame < frames; frame++) {
        for (UInt32 ch = 0; ch < channels; ch++) {
            const AudioBuffer *source = &input->mBuffers[ch];
            if (source->mData == NULL || source->mDataByteSize < (frame + 1) * bytes_per_sample) {
                return false;
            }

            memcpy(state->interleaved_buffer + ((size_t)(frame * channels + ch) * bytes_per_sample),
                   (unsigned char *)source->mData + ((size_t)frame * bytes_per_sample),
                   bytes_per_sample);
        }
    }

    *out_samples = frames * channels;
    *out_data = state->interleaved_buffer;
    return true;
}

static OSStatus tap_io_proc(AudioObjectID in_device, const AudioTimeStamp *in_now,
                            const AudioBufferList *in_input_data,
                            const AudioTimeStamp *in_input_time,
                            AudioBufferList *out_output_data,
                            const AudioTimeStamp *in_output_time, void *in_client_data) {
    (void)in_device;
    (void)in_now;
    (void)in_input_time;
    (void)out_output_data;
    (void)in_output_time;

    struct tap_state *state = (struct tap_state *)in_client_data;
    struct audio_data *audio = state->audio;

    if (audio->terminate == 1) {
        return noErr;
    }

    UInt32 bytes_per_sample = (UInt32)(audio->format / 8);
    if (bytes_per_sample == 0) {
        return noErr;
    }

    UInt32 samples = 0;
    unsigned char *data = NULL;

    if (!copy_buffer_interleaved(in_input_data, state, bytes_per_sample, audio->channels, &samples,
                                 &data)) {
        return noErr;
    }

    write_to_cava_input_buffers((int16_t)samples, data, audio);
    return noErr;
}

static bool setup_audio_format_from_device(struct tap_state *state) {
    struct audio_data *audio = state->audio;

    if (get_device_input_asbd(state->aggregate_device_id, &state->asbd) != noErr) {
        return false;
    }

    Float64 nominal_rate = 0;
    if (get_device_nominal_rate(state->aggregate_device_id, &nominal_rate) == noErr &&
        nominal_rate > 0) {
        audio->rate = (unsigned int)nominal_rate;
    }

    audio->channels = state->asbd.mChannelsPerFrame;
    if (audio->channels < 1) {
        audio->channels = 1;
    }
    if (audio->channels > 2) {
        audio->channels = 2;
    }

    if ((state->asbd.mFormatFlags & kAudioFormatFlagIsFloat) != 0 ||
        (state->asbd.mFormatID == kAudioFormatLinearPCM &&
         (state->asbd.mFormatFlags & kAudioFormatFlagIsPacked) != 0 &&
         state->asbd.mBitsPerChannel == 32 && state->asbd.mBytesPerFrame == 4 * audio->channels)) {
        audio->format = 32;
        audio->IEEE_FLOAT = 1;
        return true;
    }

    audio->IEEE_FLOAT = 0;
    switch (state->asbd.mBitsPerChannel) {
    case 8:
    case 16:
    case 24:
    case 32:
        audio->format = (int)state->asbd.mBitsPerChannel;
        return true;
    default:
        return false;
    }
}

static void cleanup_tap_state(struct tap_state *state) {
    if (state->io_running) {
        AudioDeviceStop(state->aggregate_device_id, state->io_proc_id);
        state->io_running = false;
    }

    if (state->io_proc_created) {
        AudioDeviceDestroyIOProcID(state->aggregate_device_id, state->io_proc_id);
        state->io_proc_created = false;
    }

    if (state->aggregate_created) {
        AudioHardwareDestroyAggregateDevice(state->aggregate_device_id);
        state->aggregate_created = false;
    }

    if (state->tap_created) {
        AudioHardwareDestroyProcessTap(state->tap_id);
        state->tap_created = false;
    }

    free(state->interleaved_buffer);
    state->interleaved_buffer = NULL;
    state->interleaved_buffer_size = 0;
}

void *input_coreaudio_tap(void *data) {
    struct tap_state state;
    memset(&state, 0, sizeof(state));
    state.audio = (struct audio_data *)data;

    @autoreleasepool {
        CATapDescription *tap = build_tap_description(state.audio->source);
        if (tap == nil) {
            fprintf(stderr, "Error: failed to build CoreAudio tap description\n");
            signal_threadparams(state.audio);
            signal_terminate(state.audio);
            return NULL;
        }

        tap.privateTap = YES;
        tap.muteBehavior = CATapUnmuted;

        NSString *tap_name = @"cava-tap";
        tap.name = tap_name;

        NSUUID *tap_uuid = [tap UUID];
        if (tap_uuid == nil) {
            tap_uuid = [NSUUID UUID];
            tap.UUID = tap_uuid;
        }

        OSStatus status = AudioHardwareCreateProcessTap(tap, &state.tap_id);
        if (status != noErr) {
            fprintf(stderr, "Error: AudioHardwareCreateProcessTap failed (status %d)\n", (int)status);
            signal_threadparams(state.audio);
            signal_terminate(state.audio);
            return NULL;
        }
        state.tap_created = true;

        NSString *tap_uid = [tap_uuid UUIDString];
        NSString *aggregate_uid = [NSString stringWithFormat:@"cava.coreaudio.tap.%d", getpid()];
        NSString *aggregate_name = @"cava tap aggregate";

        CFStringRef key_uid = cfstring_from_cstr(kAudioAggregateDeviceUIDKey);
        CFStringRef key_name = cfstring_from_cstr(kAudioAggregateDeviceNameKey);
        CFStringRef key_private = cfstring_from_cstr(kAudioAggregateDeviceIsPrivateKey);
        CFStringRef key_taps = cfstring_from_cstr(kAudioAggregateDeviceTapListKey);
        CFStringRef key_tap_autostart = cfstring_from_cstr(kAudioAggregateDeviceTapAutoStartKey);
        CFStringRef key_subtap_uid = cfstring_from_cstr(kAudioSubTapUIDKey);
        CFStringRef key_subtap_drift = cfstring_from_cstr(kAudioSubTapDriftCompensationKey);

        int one = 1;
        int zero = 0;
        CFNumberRef cf_one = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &one);
        CFNumberRef cf_zero = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &zero);

        const void *tap_entry_keys[] = {key_subtap_uid, key_subtap_drift};
        const void *tap_entry_vals[] = {(__bridge CFStringRef)tap_uid, cf_zero};
        CFDictionaryRef tap_entry =
            CFDictionaryCreate(kCFAllocatorDefault, tap_entry_keys, tap_entry_vals, 2,
                               &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        const void *tap_entries[] = {tap_entry};
        CFArrayRef tap_list =
            CFArrayCreate(kCFAllocatorDefault, tap_entries, 1, &kCFTypeArrayCallBacks);

        const void *aggregate_keys[] = {key_uid, key_name, key_private, key_taps, key_tap_autostart};
        const void *aggregate_vals[] = {(__bridge CFStringRef)aggregate_uid,
                                        (__bridge CFStringRef)aggregate_name, cf_one, tap_list,
                                        cf_one};
        CFDictionaryRef aggregate_desc =
            CFDictionaryCreate(kCFAllocatorDefault, aggregate_keys, aggregate_vals, 5,
                               &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);

        status = AudioHardwareCreateAggregateDevice(aggregate_desc, &state.aggregate_device_id);

        CFRelease(aggregate_desc);
        CFRelease(tap_list);
        CFRelease(tap_entry);
        CFRelease(cf_zero);
        CFRelease(cf_one);
        CFRelease(key_subtap_drift);
        CFRelease(key_subtap_uid);
        CFRelease(key_tap_autostart);
        CFRelease(key_taps);
        CFRelease(key_private);
        CFRelease(key_name);
        CFRelease(key_uid);

        if (status != noErr) {
            fprintf(stderr, "Error: AudioHardwareCreateAggregateDevice failed (status %d)\n",
                    (int)status);
            cleanup_tap_state(&state);
            signal_threadparams(state.audio);
            signal_terminate(state.audio);
            return NULL;
        }
        state.aggregate_created = true;

        if (!setup_audio_format_from_device(&state)) {
            fprintf(stderr, "Error: failed to query aggregate tap stream format\n");
            cleanup_tap_state(&state);
            signal_threadparams(state.audio);
            signal_terminate(state.audio);
            return NULL;
        }

        status = AudioDeviceCreateIOProcID(state.aggregate_device_id, tap_io_proc, &state,
                                           &state.io_proc_id);
        if (status != noErr) {
            fprintf(stderr, "Error: AudioDeviceCreateIOProcID failed (status %d)\n", (int)status);
            cleanup_tap_state(&state);
            signal_threadparams(state.audio);
            signal_terminate(state.audio);
            return NULL;
        }
        state.io_proc_created = true;

        status = AudioDeviceStart(state.aggregate_device_id, state.io_proc_id);
        if (status != noErr) {
            fprintf(stderr, "Error: AudioDeviceStart failed (status %d)\n", (int)status);
            cleanup_tap_state(&state);
            signal_threadparams(state.audio);
            signal_terminate(state.audio);
            return NULL;
        }
        state.io_running = true;

        signal_threadparams(state.audio);

        while (state.audio->terminate != 1) {
            usleep(100000);
        }

        cleanup_tap_state(&state);
        return NULL;
    }
}

#endif
