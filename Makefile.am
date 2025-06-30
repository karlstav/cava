AUTOMAKE_OPTIONS = foreign

ACLOCAL_AMFLAGS = -I m4

bin_PROGRAMS = cava
cavadir = $(top_srcdir)
cava_SOURCES = cava.c cavacore.c config.c input/common.c input/fifo.c input/shmem.c \
               output/terminal_noncurses.c output/raw.c output/noritake.c \
               cavacore.h config.h input/common.h input/fifo.h input/shmem.h \
               output/terminal_noncurses.h output/raw.h output/noritake.h \
               debug.h util.h third_party/incbin.h
cava_CPPFLAGS = -DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\" \
           -D_POSIX_SOURCE -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE_EXTENDED \
	   -DFONTDIR=\"@FONT_DIR@\" -DFONTFILE=\"@FONT_FILE@\"
cava_CFLAGS = -std=c99 -Wall -Wextra -Wno-unused-result -Wno-unknown-warning-option -Wno-maybe-uninitialized -Wno-vla-parameter

EXTRA_DIST = \
    output/shaders/pass_through.vert \
    output/shaders/bar_spectrum.frag \
    output/shaders/northern_lights.frag \
    output/shaders/winamp_line_style_spectrum.frag \
    output/shaders/spectrogram.frag \
    example_files/config \
    cava.psf \
    LICENSE \
    run_all_tests.sh \
    SHADERS.md \
    .clang-format \
    CMakeLists.txt

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
    cava_SOURCES += input/alsa.c input/alsa.h
endif

if PORTAUDIO
    cava_SOURCES += input/portaudio.c input/portaudio.h
endif

if PIPEWIRE
    cava_SOURCES += input/pipewire.c input/pipewire.h
endif

if PULSE
    cava_SOURCES += input/pulse.c input/pulse.h
endif

if SNDIO
    cava_SOURCES += input/sndio.c input/sndio.h
endif

if OSS
    cava_SOURCES += input/oss.c input/oss.h
endif

if JACK
    cava_SOURCES += input/jack.c input/jack.h
endif

if NCURSES
    cava_SOURCES += output/terminal_ncurses.c output/terminal_bcircle.c \
                    output/terminal_ncurses.h output/terminal_bcircle.h

endif

if SDL
    cava_SOURCES += output/sdl_cava.c output/sdl_cava.h
endif

if SDL_GLSL
    cava_SOURCES += output/sdl_glsl.c output/sdl_glsl.h
endif
