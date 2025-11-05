#pragma once
#include "cava/config.h"

#include <stdio.h>
#include <string.h>

#ifdef PORTAUDIO
#define HAS_PORTAUDIO true
#else
#define HAS_PORTAUDIO false
#endif

#ifdef ALSA
#define HAS_ALSA true
#else
#define HAS_ALSA false
#endif

#ifdef PULSE
#define HAS_PULSE true
#else
#define HAS_PULSE false
#endif

#ifdef PIPEWIRE
#define HAS_PIPEWIRE true
#else
#define HAS_PIPEWIRE false
#endif

#ifdef SNDIO
#define HAS_SNDIO true
#else
#define HAS_SNDIO false
#endif

#ifdef OSS
#define HAS_OSS true
#else
#define HAS_OSS false
#endif

#ifdef JACK
#define HAS_JACK true
#else
#define HAS_JACK false
#endif

#ifdef _WIN32
#define HAS_WINSCAP true
#define SDL true
#define HAS_FIFO false
#define HAS_SHMEM false
#else
#define HAS_WINSCAP false
#define HAS_FIFO true
#define HAS_SHMEM true

#endif
