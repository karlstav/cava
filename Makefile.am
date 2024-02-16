AUTOMAKE_OPTIONS = foreign

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = cava
cava_SOURCES = src/cava.c src/cavacore.c src/common.c src/config.c src/input/common.c src/input/fifo.c src/input/shmem.c \
               src/output/terminal_noncurses.c src/output/raw.c src/output/noritake.c src/output/common.c

cava_CPPFLAGS = -DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\" \
           -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE_EXTENDED \
           -DFONTDIR=\"@FONT_DIR@\" -DFONTFILE=\"@FONT_FILE@\" \
           -I$(top_builddir) -I$(top_builddir)/include
cava_CFLAGS = -std=c99 -Wall -Wextra -Wno-unused-result -Wno-unknown-warning-option -Wno-maybe-uninitialized -Wno-vla-parameter

if OSX
    cava_CFLAGS += -DNORT
    cava_LDADD =
else
    cava_LDADD = -lrt
    cava_font_dir = @FONT_DIR@
    cava_font__DATA = @FONT_FILE@
endif

if FREEBSD
if CAVAFONT
    CLEANFILES = cava.bdf cava.fnt

cava.fnt: ${srcdir}/cava.psf
	${PSF2BDF} --fontname="-gnu-cava-medium-r-normal--16-160-75-75-c-80-iso10646-1" ${srcdir}/cava.psf cava.bdf
	${VTFONTCVT} -o cava.fnt cava.bdf
endif
endif

if ALSA
    cava_SOURCES += src/input/alsa.c
endif

if PORTAUDIO
    cava_SOURCES += src/input/portaudio.c
endif

if PIPEWIRE
    cava_SOURCES += src/input/pipewire.c
endif

if PULSE
    cava_SOURCES += src/input/pulse.c
endif

if SNDIO
    cava_SOURCES += src/input/sndio.c
endif

if OSS
    cava_SOURCES += src/input/oss.c
endif

if JACK
    cava_SOURCES += src/input/jack.c
endif

if NCURSES
    cava_SOURCES += src/output/terminal_ncurses.c
endif

if SDL
    cava_SOURCES += src/output/sdl_cava.c
endif

if SDL_GLSL
    cava_SOURCES += src/output/sdl_glsl.c
endif
