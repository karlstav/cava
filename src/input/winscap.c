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

#include "input/common.h"

#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

// the device change funcitonality is disabled until I figure out how
// or if to rewrite it to c
/*
class DeviceChangeNotification : public IMMNotificationClient {
    volatile ULONG ref;
    volatile bool &changed;
    HANDLE hEvent;

  public:
    DeviceChangeNotification(volatile bool &changed, HANDLE hEvent)
        : changed(changed), hEvent(hEvent) {}

    // This is meant to be allocated on stack, so we don't actually free.
    STDMETHODIMP_(ULONG) AddRef() { return InterlockedIncrement(&ref); }
    STDMETHODIMP_(ULONG) Release() { return InterlockedDecrement(&ref); }

    STDMETHODIMP QueryInterface(REFIID iid, void **ppv) {
        if (iid == IID_IUnknown) {
            AddRef();
            *ppv = (IUnknown *)this;
        } else if (iid == __uuidof(IMMNotificationClient)) {
            AddRef();
            *ppv = (IMMNotificationClient *)this;
        } else {
            *ppv = nullptr;
            return E_NOINTERFACE;
        }
        return S_OK;
    }

    STDMETHODIMP OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR) {
        if (flow == eRender && role == eConsole) {
            changed = true;
            SetEvent(hEvent);
        }
        return S_OK;
    }

    STDMETHODIMP OnDeviceAdded(LPCWSTR) { return S_OK; }
    STDMETHODIMP OnDeviceRemoved(LPCWSTR) { return S_OK; }
    STDMETHODIMP OnDeviceStateChanged(LPCWSTR, DWORD) { return S_OK; }
    STDMETHODIMP OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) { return S_OK; }
};
*/

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

    static const GUID IID_IAudioCaptureClient = {
        0xc8adbd64, 0xe71e, 0x48a0, {0xa4, 0xde, 0x18, 0x5c, 0x39, 0x5c, 0xd3, 0x17}};

    struct audio_data *audio = (struct audio_data *)data;
    pthread_mutex_lock(&audio->lock);
    CoInitialize(0);

    WAVEFORMATEX *wfx = NULL;
    WAVEFORMATEXTENSIBLE *wfx_ext = NULL;
    IMMDeviceEnumerator *pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(&CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                                  &IID_IMMDeviceEnumerator, (void **)&pEnumerator);

    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    /*
    volatile bool deviceChanged = false;
    DeviceChangeNotification deviceChangeNotification(deviceChanged, hEvent);
    pEnumerator->RegisterEndpointNotificationCallback(&deviceChangeNotification);
    */

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
