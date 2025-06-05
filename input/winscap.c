#define UNICODE
#define _UNICODE

#include <initguid.h>
#include <mmdeviceapi.h>

#include <audioclient.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <functiondiscoverykeys.h>

#include "common.h"

// IID_IMMNotificationClient definition for linking
DEFINE_GUID(IID_IMMNotificationClient, 
    0x7991eec9, 0x7e89, 0x4d85, 0x83, 0xd0, 0x68, 0x5a, 0xe8, 0x7e, 0x5c, 0x04);

DEFINE_GUID(IID_IAudioCaptureClient, 
    0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

// the device change funcitonality is disabled until I figure out how
// or if to rewrite it to c

// C implementation of IMMNotificationClient for device change notification

typedef struct DeviceChangeNotification {
    IMMNotificationClientVtbl *lpVtbl;
    LONG ref;
    volatile BOOL *changed;
    HANDLE hEvent;
} DeviceChangeNotification;

// Forward declarations
HRESULT STDMETHODCALLTYPE DeviceChangeNotification_QueryInterface(
    IMMNotificationClient *This, REFIID riid, void **ppvObject);
ULONG STDMETHODCALLTYPE DeviceChangeNotification_AddRef(IMMNotificationClient *This);
ULONG STDMETHODCALLTYPE DeviceChangeNotification_Release(IMMNotificationClient *This);
HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDefaultDeviceChanged(
    IMMNotificationClient *This, EDataFlow flow, ERole role, LPCWSTR pwstrDeviceId);
HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceAdded(
    IMMNotificationClient *This, LPCWSTR pwstrDeviceId);
HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceRemoved(
    IMMNotificationClient *This, LPCWSTR pwstrDeviceId);
HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceStateChanged(
    IMMNotificationClient *This, LPCWSTR pwstrDeviceId, DWORD dwNewState);
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
    DeviceChangeNotification_OnPropertyValueChanged
};

void DeviceChangeNotification_Init(DeviceChangeNotification *self, volatile BOOL *changed, HANDLE hEvent) {
    self->lpVtbl = &g_DeviceChangeNotification_Vtbl;
    self->ref = 1;
    self->changed = changed;
    self->hEvent = hEvent;
}

HRESULT STDMETHODCALLTYPE DeviceChangeNotification_QueryInterface(
    IMMNotificationClient *This, REFIID riid, void **ppvObject) {
    if (!ppvObject) return E_POINTER;
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
        if (self->changed) *self->changed = TRUE;
        if (self->hEvent) SetEvent(self->hEvent);
    }
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceAdded(
    IMMNotificationClient *This, LPCWSTR pwstrDeviceId) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceRemoved(
    IMMNotificationClient *This, LPCWSTR pwstrDeviceId) {
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeviceChangeNotification_OnDeviceStateChanged(
    IMMNotificationClient *This, LPCWSTR pwstrDeviceId, DWORD dwNewState) {
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

void input_winscap(void *data) {

    static const GUID CLSID_MMDeviceEnumerator = {0xBCDE0395, 0xE52F, 0x467C, 0x8E, 0x3D, 0xC4,
                                                  0x57,       0x92,   0x91,   0x69, 0x2E};
    static const GUID IID_IMMDeviceEnumerator = {0xA95664D2, 0x9614, 0x4F35, 0xA7, 0x46, 0xDE,
                                                 0x8D,       0xB6,   0x36,   0x17, 0xE6};
    static const GUID IID_IAudioClient = {0x1CB9AD4C, 0xDBFA, 0x4c32, 0xB1, 0x78, 0xC2,
                                          0xF5,       0x68,   0xA7,   0x03, 0xB2};
    // IID_IMMNotificationClient definition for linking
    // (moved to file scope below)

    struct audio_data *audio = (struct audio_data *)data;
    pthread_mutex_lock(&audio->lock);
    CoInitialize(0);

    WAVEFORMATEX *wfx = NULL;
    WAVEFORMATEXTENSIBLE *wfx_ext = NULL;
    IMMDeviceEnumerator *pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                                  &IID_IMMDeviceEnumerator, (void **)&pEnumerator);

    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    
    volatile BOOL deviceChanged = FALSE;
    DeviceChangeNotification deviceChangeNotification;
    DeviceChangeNotification_Init(&deviceChangeNotification, &deviceChanged, hEvent);
    pEnumerator->lpVtbl->RegisterEndpointNotificationCallback(pEnumerator, (IMMNotificationClient *)&deviceChangeNotification);
    
   

    while (!audio->terminate) {
        ResetEvent(hEvent);

        IMMDevice *pDevice = NULL;
        pEnumerator->lpVtbl->GetDefaultAudioEndpoint(pEnumerator, eRender, eConsole, &pDevice);

        IAudioClient *pClient = NULL;
        pDevice->lpVtbl->Activate(pDevice, &IID_IAudioClient, CLSCTX_ALL, 0, (void **)&pClient);

        hr = pClient->lpVtbl->GetMixFormat(pClient, &wfx);

        HRESULT hrInit = pClient->lpVtbl->Initialize(pClient, AUDCLNT_SHAREMODE_SHARED,
                                                     AUDCLNT_STREAMFLAGS_LOOPBACK,
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
            WaitForSingleObject(hEvent, INFINITE);
            continue;
        }

        UINT32 bufferFrameCount;
        pClient->lpVtbl->GetBufferSize(pClient, &bufferFrameCount);

        IAudioCaptureClient *pCapture = NULL;
        hr = pClient->lpVtbl->GetService(pClient, &IID_IAudioCaptureClient, (void **)&pCapture);
        if (FAILED(hr) || pCapture == NULL) {
            fwprintf(stderr, L"Failed to initialize IAudioCaptureClient\n");
            pClient->lpVtbl->Release(pClient);
            pDevice->lpVtbl->Release(pDevice);
            WaitForSingleObject(hEvent, INFINITE);
            continue;
        }

        pClient->lpVtbl->GetService(pClient, &IID_IAudioCaptureClient, (void **)&pCapture);

        WAVEFORMATEX format = *wfx;

        DWORD dwDelay =
            (DWORD)(((double)REFTIMES_PER_SEC * bufferFrameCount / format.nSamplesPerSec) /
                    REFTIMES_PER_MILLISEC / 2);

        LPBYTE pSilence = (LPBYTE)malloc(bufferFrameCount * format.nBlockAlign);

        ZeroMemory(pSilence, bufferFrameCount * format.nBlockAlign);

        pClient->lpVtbl->Start(pClient);

        audio->channels = format.nChannels;
        audio->rate = format.nSamplesPerSec;
        audio->format = format.wBitsPerSample;
        if (format.wBitsPerSample == 32)
            audio->IEEE_FLOAT = 1;
        pthread_mutex_unlock(&audio->lock);
        // deviceChanged
        while (!audio->terminate) {
            Sleep(dwDelay);

            UINT32 packetLength;
            HRESULT hrNext = pCapture->lpVtbl->GetNextPacketSize(pCapture, &packetLength);

            if (hrNext == AUDCLNT_E_DEVICE_INVALIDATED) {
                // while (!deviceChanged)
                //     ;
                break;
            } else {
                // ensure(hrNext);
                ;
            }

            while (packetLength) {
                LPBYTE pData;
                UINT32 numFramesAvailable;
                DWORD flags;

                pCapture->lpVtbl->GetBuffer(pCapture, &pData, &numFramesAvailable, &flags, 0, 0);

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    pData = pSilence;

                write_to_cava_input_buffers(numFramesAvailable * format.nChannels,
                                            (unsigned char *)pData, audio);

                pCapture->lpVtbl->ReleaseBuffer(pCapture, numFramesAvailable);
                pCapture->lpVtbl->GetNextPacketSize(pCapture, &packetLength);
            }
        }
        // deviceChanged = false;
        pClient->lpVtbl->Stop(pClient);
        free(pSilence);
    }
}
