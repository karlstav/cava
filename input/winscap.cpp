#define UNICODE
#define _UNICODE

#include <audioclient.h>
#include <comdef.h>
#include <comip.h>
#include <fcntl.h>
#include <io.h>
#include <mmdeviceapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <functiondiscoverykeys_devpkey.h>

#include "input/common.h"


#define REFTIMES_PER_SEC 10000000
#define REFTIMES_PER_MILLISEC 10000

class CCoInitialize {
    HRESULT m_hr;

  public:
    CCoInitialize() : m_hr(CoInitialize(NULL)) {}
    ~CCoInitialize() {
        if (SUCCEEDED(m_hr))
            CoUninitialize();
    }
    operator HRESULT() const { return m_hr; }
};


const CLSID CLSID_MMDeviceEnumerator = __uuidof(MMDeviceEnumerator);
const IID IID_IMMDeviceEnumerator = __uuidof(IMMDeviceEnumerator);
const IID IID_IAudioClient = __uuidof(IAudioClient);
const IID IID_IAudioCaptureClient = __uuidof(IAudioCaptureClient);


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

class EnsureCaptureStop {
    IAudioClient *m_client;

  public:
    EnsureCaptureStop(IAudioClient *pClient) : m_client(pClient) {}
    ~EnsureCaptureStop() { m_client->Stop(); }
};

class EnsureFree {
    void *m_memory;

  public:
    EnsureFree(void *memory) : m_memory(memory) {}
    ~EnsureFree() { free(m_memory); }
};

struct {
    HRESULT hr;
    LPCWSTR error;
} error_table[] = {
    {AUDCLNT_E_UNSUPPORTED_FORMAT, L"Requested sound format unsupported"},
};

LPCWSTR error_message(const _com_error &err) {
    for (int i = 0; i < sizeof error_table / sizeof error_table[0]; ++i) {
        if (error_table[i].hr == err.Error()) {
            return error_table[i].error;
        }
    }
    return err.ErrorMessage();
}

#define ensure(hr) ensure_(__FILE__, __LINE__, hr)
void ensure_(const char *file, int line, HRESULT hr) {
    if (FAILED(hr)) {
        _com_error err(hr);
        fwprintf(stderr, L"Error at %S:%d (0x%08x): %s\n", file, line, hr, error_message(err));
        exit(1);
    }
}

#ifdef __cplusplus
extern "C" {
#endif

void input_winscap(void *data) {

    struct audio_data *audio = (struct audio_data *)data;
    pthread_mutex_lock(&audio->lock);
    CCoInitialize comInit;
    ensure(comInit);

    WAVEFORMATEX *wfx = NULL;
    WAVEFORMATEXTENSIBLE *wfx_ext = NULL;
    /*
    *wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = (WORD)channels;
    wfx.nSamplesPerSec = (DWORD)rate;
    wfx.wBitsPerSample = (WORD)bits;
    wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
    */
    IMMDeviceEnumerator *pEnumerator = NULL;
    HRESULT hr = CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL,
                                  IID_IMMDeviceEnumerator, (void **)&pEnumerator);

    volatile bool deviceChanged = false;
    HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    DeviceChangeNotification deviceChangeNotification(deviceChanged, hEvent);
    pEnumerator->RegisterEndpointNotificationCallback(&deviceChangeNotification);

    for (;;) {
        ResetEvent(hEvent);

        IMMDevice *pDevice = NULL;
        ensure(pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice));

        IAudioClient *pClient = NULL;
        ensure(pDevice->Activate(IID_IAudioClient, CLSCTX_ALL, nullptr, (void **)&pClient));

        hr = pClient->GetMixFormat(&wfx);

        /*
        WAVEFORMATEX wfx;
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        pthread_mutex_lock(&audio->lock);
        audio->channels = 2;
        audio->rate = 48000;
        audio->format = 16;
        wfx.nChannels = (WORD)audio->channels;
        wfx.nSamplesPerSec = (DWORD)audio->rate;
        wfx.wBitsPerSample = (WORD)audio->format;
        pthread_mutex_unlock(&audio->lock);
        wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
                */
        HRESULT hrInit = pClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK,
                                             16 * REFTIMES_PER_MILLISEC, 0, wfx, nullptr);
        if (FAILED(hrInit)) {
            _com_error err(hrInit);

            IPropertyStore *pProps = NULL;
            ensure(pDevice->OpenPropertyStore(STGM_READ, &pProps));

            PROPVARIANT varName;
            PropVariantInit(&varName);
            ensure(pProps->GetValue(PKEY_Device_FriendlyName, &varName));
            fwprintf(stderr, L"Failed to open: %s: %s\n", varName.pwszVal, error_message(err));
            PropVariantClear(&varName);
            WaitForSingleObject(hEvent, INFINITE);
            continue;
        }

        UINT32 bufferFrameCount;
        ensure(pClient->GetBufferSize(&bufferFrameCount));

        IAudioCaptureClient *pCapture = NULL;

        ensure(pClient->GetService(IID_IAudioCaptureClient, (void **)&pCapture));

        WAVEFORMATEX format = *wfx;

        
        DWORD dwDelay = (DWORD)(((double)REFTIMES_PER_SEC * bufferFrameCount / format.nSamplesPerSec) /
                                REFTIMES_PER_MILLISEC / 2);

        LPBYTE pSilence = (LPBYTE)malloc(bufferFrameCount * format.nBlockAlign);
        EnsureFree freeSilence(pSilence);
        ZeroMemory(pSilence, bufferFrameCount * format.nBlockAlign);
        
        ensure(pClient->Start());
        EnsureCaptureStop autoStop(pClient);

        audio->channels = format.nChannels;
        audio->rate = format.nSamplesPerSec;
        audio->format = format.wBitsPerSample;
        if (format.wBitsPerSample == 32)
            audio->IEEE_FLOAT = 1;
        pthread_mutex_unlock(&audio->lock);

        while (!deviceChanged) {
            Sleep(dwDelay);

            UINT32 packetLength;
            HRESULT hrNext = pCapture->GetNextPacketSize(&packetLength);
            if (hrNext == AUDCLNT_E_DEVICE_INVALIDATED) {
                while (!deviceChanged)
                    ;
                break;
            } else {
                ensure(hrNext);
            }

            while (packetLength) {
                LPBYTE pData;
                UINT32 numFramesAvailable;
                DWORD flags;

                ensure(pCapture->GetBuffer(&pData, &numFramesAvailable, &flags, nullptr, nullptr));

                if (flags & AUDCLNT_BUFFERFLAGS_SILENT)
                    pData = pSilence;

                write_to_cava_input_buffers(numFramesAvailable * format.nChannels, (unsigned char *)pData, audio);

                ensure(pCapture->ReleaseBuffer(numFramesAvailable));
                ensure(pCapture->GetNextPacketSize(&packetLength));
            }
        }
        deviceChanged = false;
    }
}

#ifdef __cplusplus
}
#endif