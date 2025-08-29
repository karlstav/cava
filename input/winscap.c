#define UNICODE
#define _UNICODE

#include <initguid.h>
#include <mmdeviceapi.h>

#include <audioclient.h>
#include <fcntl.h>
#include <io.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <functiondiscoverykeys.h>

#include "common.h"

static void make_stereo_waveformatex(const WAVEFORMATEX *orig, WAVEFORMATEX *stereo) {
    *stereo = *orig;
    stereo->wFormatTag = WAVE_FORMAT_PCM;
    stereo->nChannels = 2;
    stereo->wBitsPerSample = 16;
    stereo->nBlockAlign = stereo->nChannels * (stereo->wBitsPerSample / 8);
    stereo->nAvgBytesPerSec = stereo->nSamplesPerSec * stereo->nBlockAlign;
    stereo->cbSize = 0;
}

// Downmix interleaved 16-bit PCM to stereo
static void downmix_to_stereo_s16(const int16_t *src, int16_t *dst, size_t frames, int channels) {
    size_t i;
    for (i = 0; i < frames; ++i) {
        int left = 0, right = 0;
        if (channels >= 2) {
            left += src[i * channels + 0];
            right += src[i * channels + 1];
        }
        if (channels >= 3) {
            left += (int)(src[i * channels + 2] * 0.7f);
            right += (int)(src[i * channels + 2] * 0.7f);
        }
        if (channels >= 5) {
            left += (int)(src[i * channels + 4] * 0.7f);
            right += (int)(src[i * channels + 5] * 0.7f);
        }
        if (channels >= 7) {
            left += (int)(src[i * channels + 6] * 0.7f);
            right += (int)(src[i * channels + 7] * 0.7f);
        }
        if (left > 32767)
            left = 32767;
        if (left < -32768)
            left = -32768;
        if (right > 32767)
            right = 32767;
        if (right < -32768)
            right = -32768;
        dst[i * 2 + 0] = (int16_t)left;
        dst[i * 2 + 1] = (int16_t)right;
    }
}

// Helper to convert 3 bytes (little-endian) to signed 32-bit int
static inline int32_t s24_to_s32(const uint8_t *p) {
    int32_t val = p[0] | (p[1] << 8) | (p[2] << 16);
    // Sign-extend if negative
    if (val & 0x800000)
        val |= ~0xFFFFFF;
    return val;
}

// Downmix interleaved 24-bit PCM to stereo
static void downmix_to_stereo_s24(const uint8_t *src, int16_t *dst, size_t frames, int channels) {
    size_t i;
    for (i = 0; i < frames; ++i) {
        int left = 0, right = 0;

        const uint8_t *frame = src + i * channels * 3;
        if (channels >= 2) {
            left += s24_to_s32(frame + 0 * 3) >> 8;
            right += s24_to_s32(frame + 1 * 3) >> 8;
        }
        if (channels >= 3) {
            int32_t c = s24_to_s32(frame + 2 * 3) >> 8;
            left += (int)(c * 0.7f);
            right += (int)(c * 0.7f);
        }
        if (channels >= 5) {
            int32_t c4 = s24_to_s32(frame + 4 * 3) >> 8;
            int32_t c5 = s24_to_s32(frame + 5 * 3) >> 8;
            left += (int)(c4 * 0.7f);
            right += (int)(c5 * 0.7f);
        }
        if (channels >= 7) {
            int32_t c6 = s24_to_s32(frame + 6 * 3) >> 8;
            int32_t c7 = s24_to_s32(frame + 7 * 3) >> 8;
            left += (int)(c6 * 0.7f);
            right += (int)(c7 * 0.7f);
        }
        // Clip to int16_t
        if (left > 32767)
            left = 32767;
        if (left < -32768)
            left = -32768;
        if (right > 32767)
            right = 32767;
        if (right < -32768)
            right = -32768;
        dst[i * 2 + 0] = (int16_t)left;
        dst[i * 2 + 1] = (int16_t)right;
    }
}

// Downmix interleaved 32-bit PCM to stereo
static void downmix_to_stereo_s32(const int32_t *src, int16_t *dst, size_t frames, int channels) {
    size_t i;
    for (i = 0; i < frames; ++i) {
        int64_t left = 0, right = 0;
        if (channels >= 2) {
            left += src[i * channels + 0];
            right += src[i * channels + 1];
        }
        if (channels >= 3) {
            left += (int64_t)(src[i * channels + 2] * 0.7f);
            right += (int64_t)(src[i * channels + 2] * 0.7f);
        }
        if (channels >= 5) {
            left += (int64_t)(src[i * channels + 4] * 0.7f);
            right += (int64_t)(src[i * channels + 5] * 0.7f);
        }
        if (channels >= 7) {
            left += (int64_t)(src[i * channels + 6] * 0.7f);
            right += (int64_t)(src[i * channels + 7] * 0.7f);
        }
        // Convert from 32-bit to 16-bit with clipping
        left >>= 16;
        right >>= 16;
        if (left > 32767)
            left = 32767;
        if (left < -32768)
            left = -32768;
        if (right > 32767)
            right = 32767;
        if (right < -32768)
            right = -32768;
        dst[i * 2 + 0] = (int16_t)left;
        dst[i * 2 + 1] = (int16_t)right;
    }
}

// Downmix interleaved 32-bit float to stereo
static void downmix_to_stereo_f32(const float *src, int16_t *dst, size_t frames, int channels) {
    size_t i;
    for (i = 0; i < frames; ++i) {
        float left = 0.0f, right = 0.0f;
        if (channels >= 2) {
            left += src[i * channels + 0];
            right += src[i * channels + 1];
        }
        if (channels >= 3) {
            left += src[i * channels + 2] * 0.7f;
            right += src[i * channels + 2] * 0.7f;
        }
        if (channels >= 5) {
            left += src[i * channels + 4] * 0.7f;
            right += src[i * channels + 5] * 0.7f;
        }
        if (channels >= 7) {
            left += src[i * channels + 6] * 0.7f;
            right += src[i * channels + 7] * 0.7f;
        }
        // Convert from float [-1.0, 1.0] to int16_t
        int l = (int)(left * 32767.0f);
        int r = (int)(right * 32767.0f);
        if (l > 32767)
            l = 32767;
        if (l < -32768)
            l = -32768;
        if (r > 32767)
            r = 32767;
        if (r < -32768)
            r = -32768;
        dst[i * 2 + 0] = (int16_t)l;
        dst[i * 2 + 1] = (int16_t)r;
    }
}

// IID_IMMNotificationClient definition for linking

#if !defined(__MINGW32__) && !defined(__MINGW64__)
DEFINE_GUID(IID_IMMNotificationClient, 0x7991eec9, 0x7e89, 0x4d85, 0x83, 0x90, 0x6c, 0x70, 0x3c,
            0xec, 0x60, 0xc0);
#endif

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

typedef struct DeviceChangeNotification {
    IMMNotificationClientVtbl *lpVtbl;
    LONG ref;
    volatile BOOL *changed;
    HANDLE hEvent;
} DeviceChangeNotification;

// Forward declarations
HRESULT STDMETHODCALLTYPE DeviceChangeNotification_QueryInterface(IMMNotificationClient *This,
                                                                  REFIID riid, void **ppvObject);
ULONG STDMETHODCALLTYPE DeviceChangeNotification_AddRef(IMMNotificationClient *This);
ULONG STDMETHODCALLTYPE DeviceChangeNotification_Release(IMMNotificationClient *This);
HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDefaultDeviceChanged(
    IMMNotificationClient *This, EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId);
HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceAdded(IMMNotificationClient *This,
                                                                 LPCWSTR pwstrDeviceId);
HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceRemoved(IMMNotificationClient *This,
                                                                   LPCWSTR pwstrDeviceId);
HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceStateChanged(IMMNotificationClient *This,
                                                                        LPCWSTR pwstrDeviceId,
                                                                        DWORD dwNewState);
HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnPropertyValueChanged(
    IMMNotificationClient *This, LPCWSTR pwstrDeviceId, const PROPERTYKEY key);

IMMNotificationClientVtbl g_DeviceChangeNotification_Vtbl = {
    DeviceChangeNotification_QueryInterface,
    DeviceChangeNotification_AddRef,
    DeviceChangeNotification_Release,
    DeviceChangeNotification_OnDeviceStateChanged,
    DeviceChangeNotification_OnDeviceAdded,
    DeviceChangeNotification_OnDeviceRemoved,
    DeviceChangeNotification_OnDefaultDeviceChanged,
    DeviceChangeNotification_OnPropertyValueChanged};

void DeviceChangeNotification_Init(DeviceChangeNotification *self, volatile BOOL *changed,
                                   HANDLE hEvent) {
    self->lpVtbl = &g_DeviceChangeNotification_Vtbl;
    self->ref = 1;
    self->changed = changed;
    self->hEvent = hEvent;
}

HRESULT STDMETHODCALLTYPE DeviceChangeNotification_QueryInterface(IMMNotificationClient *This,
                                                                  REFIID riid, void **ppvObject) {
    if (!ppvObject)
        return E_POINTER;
    if (IsEqualIID(riid, &IID_IUnknown) || IsEqualIID(riid, &IID_IMMNotificationClient)) {
        *ppvObject = This;
        DeviceChangeNotification_AddRef(This);
        return S_OK;
    }
    *ppvObject = NULL;
    return E_NOINTERFACE;
}

ULONG STDMETHODCALLTYPE DeviceChangeNotification_AddRef(IMMNotificationClient *This) {
    DeviceChangeNotification *self = (DeviceChangeNotification *)This;
    return InterlockedIncrement(&self->ref);
}

ULONG STDMETHODCALLTYPE DeviceChangeNotification_Release(IMMNotificationClient *This) {
    DeviceChangeNotification *self = (DeviceChangeNotification *)This;
    LONG ref = InterlockedDecrement(&self->ref);
    // This object is meant to be stack-allocated, so do not free.
    return ref;
}

HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDefaultDeviceChanged(
    IMMNotificationClient *This, EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId) {
    DeviceChangeNotification *self = (DeviceChangeNotification *)This;
    if (flow == eRender && role == eConsole) {
        if (self->changed)
            *self->changed = TRUE;
        if (self->hEvent)
            SetEvent(self->hEvent);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceAdded(IMMNotificationClient *This,
                                                                 LPCWSTR pwstrDeviceId) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceRemoved(IMMNotificationClient *This,
                                                                   LPCWSTR pwstrDeviceId) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceStateChanged(IMMNotificationClient *This,
                                                                        LPCWSTR pwstrDeviceId,
                                                                        DWORD dwNewState) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnPropertyValueChanged(
    IMMNotificationClient *This, LPCWSTR pwstrDeviceId, const PROPERTYKEY key) {
    return S_OK;
}

struct {
    HRESULT hr;
    LPCWSTR error;
} error_table[] = {
    {AUDCLNT_E_UNSUPPORTED_FORMAT, L"Requested sound format unsupported"},
};

void write_silent_frame(struct audio_data *audio, IAudioCaptureClient *pCapture,
                        UINT32 numFramesAvailable, UINT32 packetLength) {
    // Batch silent frames to reduce CPU usage
    int bytes_per_sample = audio->format / 8;
    int silent_channels = audio->channels;
    static LPBYTE silence = NULL;
    static int silence_size = 0;
    int required_size = numFramesAvailable * silent_channels * bytes_per_sample;
    if (!silence || silence_size < required_size) {
        if (silence)
            free(silence);
        silence = (LPBYTE)calloc(1, required_size);
        silence_size = required_size;
    }

    // Write all silent frames in one batch
    write_to_cava_input_buffers(numFramesAvailable * silent_channels, (unsigned char *)silence,
                                audio);
    pCapture->lpVtbl->ReleaseBuffer(pCapture, numFramesAvailable);
    pCapture->lpVtbl->GetNextPacketSize(pCapture, &packetLength);
    Sleep(1); // Prevent busy loop
}

void process_multichannel(UINT32 numFramesAvailable, const WAVEFORMATEX format, const void *pData,
                          struct audio_data *audio, IAudioCaptureClient *pCapture,
                          UINT32 packetLength, WAVEFORMATEX stereo_format) {

    int16_t *stereo_buffer = (int16_t *)malloc(numFramesAvailable * 2 * sizeof(int16_t));
    if (format.wFormatTag == WAVE_FORMAT_IEEE_FLOAT && format.wBitsPerSample == 32) {
        downmix_to_stereo_f32((const float *)pData, stereo_buffer, numFramesAvailable,
                              format.nChannels);
    } else if (format.wBitsPerSample == 32) {
        downmix_to_stereo_s32((const int32_t *)pData, stereo_buffer, numFramesAvailable,
                              format.nChannels);
    } else if (format.wBitsPerSample == 24) {
        downmix_to_stereo_s24((const uint8_t *)pData, stereo_buffer, numFramesAvailable,
                              format.nChannels);
    } else if (format.wBitsPerSample == 16) {
        downmix_to_stereo_s16((const int16_t *)pData, stereo_buffer, numFramesAvailable,
                              format.nChannels);
    } else {
        // Unsupported format, handle error
        write_silent_frame(audio, pCapture, numFramesAvailable, packetLength);
        return;
    }
    write_to_cava_input_buffers(numFramesAvailable * stereo_format.nChannels,
                                (unsigned char *)stereo_buffer, audio);
    free(stereo_buffer);
}

void input_winscap(void *data) {

    static const GUID CLSID_MMDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4,
                                                  0x57,       0x92,   0x91,   0x69, 0x2E};
    static const GUID IID_IMMDeviceEnumerator = {0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE,
                                                 0x8D,       0xB6,   0x36,   0x17, 0xE6};
    static const GUID IID_IAudioClient = {0x1CB9AD4C, 0xDBFA, 0x4c32, 0xB1, 0x78, 0xC2,
                                          0xF5,       0x68,   0xA7,   0x03, 0xB2};
    static const GUID IID_IAudioCaptureClient = {
        0xc8adbd64, 0xe71e, 0x48a0, {0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17}};

    struct audio_data *audio = (struct audio_data *)data;
    pthread_mutex_lock(&audio->lock);

    HRESULT hr = CoInitialize(0);
    if (FAILED(hr)) {
        fwprintf(stderr, L"CoInitialize failed: 0x%08lx\n", hr);
        pthread_mutex_unlock(&audio->lock);
        return;
    }

    WAVEFORMATEX *wfx = NULL;
    WAVEFORMATEXTENSIBLE *wfx_ext = NULL;
    IMMDeviceEnumerator *pEnumerator = NULL;
    hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, &IID_IMMDeviceEnumerator,
                          (void **)&pEnumerator);
    if (FAILED(hr)) {
        fwprintf(stderr, L"Failed to create device enumerator: 0x%08lx\n", hr);
        CoUninitialize();
        pthread_mutex_unlock(&audio->lock);
        return;
    }

    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    volatile BOOL deviceChanged = FALSE;
    DeviceChangeNotification deviceChangeNotification;
    DeviceChangeNotification_Init(&deviceChangeNotification, &deviceChanged, hEvent);
    pEnumerator->lpVtbl->RegisterEndpointNotificationCallback(
        pEnumerator, (IMMNotificationClient *)&deviceChangeNotification);

    while (!audio->terminate) {
        ResetEvent(hEvent);

        IMMDevice *pDevice = NULL;
        pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice);

        IAudioClient *pClient = NULL;
        pDevice->lpVtbl->Activate(pDevice, &IID_IAudioClient, CLSCTX_ALL, 0, (void **)&pClient);

        hr = pClient->lpVtbl->GetMixFormat(pClient, &wfx);

        if (FAILED(hr) || wfx == NULL) {
            fwprintf(stderr, L"Failed to GetMixFormat for client\n");
            continue;
        }

        HRESULT hrInit = pClient->lpVtbl->Initialize(pClient, AUDCLNT_SHAREMODE_SHARED,
                                                     AUDCLNT_STREAMFLAGS_LOOPBACK |
                                                         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
                                                     16 * REFTIMES_PER_MILLISEC, 0, wfx, 0);

        if (FAILED(hrInit)) {
            //_com_error err(hrInit);

            IPropertyStore *pProps = NULL;
            pDevice->lpVtbl->OpenPropertyStore(pDevice, STGM_READ, &pProps);

            PROPVARIANT varName;
            PropVariantInit(&varName);
            pProps->lpVtbl->GetValue(pProps, &PKEY_Device_FriendlyName, &varName);
            fwprintf(stderr, L"Failed to open: %s\n", varName.pwszVal);
            PropVariantClear(&varName);
            if (pProps)
                pProps->lpVtbl->Release(pProps);
            if (pClient)
                pClient->lpVtbl->Release(pClient);
            if (pDevice)
                pDevice->lpVtbl->Release(pDevice);
            WaitForSingleObject(hEvent, INFINITE); // may freeze, need to test on spacial device
            continue;
        }

        // use event handling instead of polling
        HRESULT hrEvent = pClient->lpVtbl->SetEventHandle(pClient, hEvent);
        if (FAILED(hrEvent)) {
            fwprintf(stderr, L"SetEventHandle failed: 0x%08lx\n", hrEvent);
            // Clean up resources as needed
            if (pClient)
                pClient->lpVtbl->Release(pClient);
            if (pDevice)
                pDevice->lpVtbl->Release(pDevice);
            continue;
        }

        UINT32 bufferFrameCount;
        pClient->lpVtbl->GetBufferSize(pClient, &bufferFrameCount);

        IAudioCaptureClient *pCapture = NULL;

        pClient->lpVtbl->GetService(pClient, &IID_IAudioCaptureClient, (void **)&pCapture);

        WAVEFORMATEX format = *wfx;

        pClient->lpVtbl->Start(pClient);

        WAVEFORMATEX stereo_format;
        make_stereo_waveformatex(&format, &stereo_format);

        if (format.nChannels > 2) {
            audio->channels = 2;
            audio->rate = stereo_format.nSamplesPerSec;
            audio->format = stereo_format.wBitsPerSample;
            audio->IEEE_FLOAT = 0;
        } else {
            audio->channels = format.nChannels;
            audio->rate = format.nSamplesPerSec;
            audio->format = format.wBitsPerSample;
            if (format.wBitsPerSample == 32)
                audio->IEEE_FLOAT = 1;
        }

        pthread_mutex_unlock(&audio->lock);

        // deviceChanged
        while (!audio->terminate) {
            DWORD waitResult = WaitForSingleObject(hEvent, INFINITE);
            if (waitResult != WAIT_OBJECT_0) {
                // Handle error or termination
                continue;
            }

            UINT32 packetLength;
            HRESULT hrNext = pCapture->lpVtbl->GetNextPacketSize(pCapture, &packetLength);

            if (hrNext == AUDCLNT_E_DEVICE_INVALIDATED || deviceChanged) {
                // Device was changed or invalidated, break to reinitialize
                deviceChanged = FALSE;
                break;
            } else if (FAILED(hrNext)) {
                // Handle other errors if necessary
                fwprintf(stderr, L"Audio capture error: 0x%08lx\n", hrNext);
                // If the error is related to device change, break to reinitialize
                if (hrNext == AUDCLNT_E_ENDPOINT_CREATE_FAILED ||
                    hrNext == AUDCLNT_E_SERVICE_NOT_RUNNING || hrNext == AUDCLNT_E_DEVICE_IN_USE) {
                    break;
                }
                break;
            }

            while (packetLength) {
                LPBYTE pData;
                UINT32 numFramesAvailable;
                DWORD flags;

                pCapture->lpVtbl->GetBuffer(pCapture, &pData, &numFramesAvailable, &flags, 0, 0);

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT) {
                    write_silent_frame(audio, pCapture, numFramesAvailable, packetLength);
                    continue;
                }

                if (format.nChannels > 2) {
                    process_multichannel(numFramesAvailable, format, pData, audio, pCapture,
                                         packetLength, stereo_format);
                } else {
                    write_to_cava_input_buffers(numFramesAvailable * format.nChannels,
                                                (unsigned char *)pData, audio);
                }

                pCapture->lpVtbl->ReleaseBuffer(pCapture, numFramesAvailable);
                pCapture->lpVtbl->GetNextPacketSize(pCapture, &packetLength);
            }
        }
        deviceChanged = FALSE;
        pClient->lpVtbl->Stop(pClient);
        if (pCapture)
            pCapture->lpVtbl->Release(pCapture);
        if (pClient)
            pClient->lpVtbl->Release(pClient);
        if (pDevice)
            pDevice->lpVtbl->Release(pDevice);
    }
    if (pEnumerator)
        pEnumerator->lpVtbl->UnregisterEndpointNotificationCallback(
            pEnumerator, (IMMNotificationClient *)&deviceChangeNotification);
    if (pEnumerator)
        pEnumerator->lpVtbl->Release(pEnumerator);
    CoUninitialize();
}
