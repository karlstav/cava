AUTOMAKE_OPTIONS = foreign

if !SYSTEM_LIBINIPARSER
    SUBDIRS = iniparser
endif

ACLOCAL_AMFLAGS = -I m4

M_CPPFLAGS = -DSYSTEM_LIBINIPARSER=@SYSTEM_LIBINIPARSER@

bin_PROGRAMS = cava
cava_SOURCES = cava.c config.c input/common.c input/fifo.c input/shmem.c \
               output/terminal_noncurses.c output/raw.c
cava_LDFLAGS = -L/usr/local/lib -Wl,-rpath /usr/local/lib
cava_CPPFLAGS = -DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\" \
           -D_POSIX_SOURCE -D _POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE_EXTENDED \
	   -DFONTDIR=\"@FONT_DIR@\"
cava_CFLAGS = -std=c99 -Wall -Werror -Wextra -Wno-unused-result -Wno-unknown-warning-option -Wno-maybe-uninitialized -Wno-vla-parameter

if OSX
    cava_CFLAGS += -DNORT
else
    cava_LDFLAGS += -lrt
    cava_font_dir = @FONT_DIR@
    cava_font__DATA = cava.psf
endif

if ALSA
    cava_SOURCES += input/alsa.c
endif

if PORTAUDIO
    cava_SOURCES += input/portaudio.c
endif

if PULSE
    cava_SOURCES += input/pulse.c
endif

if SNDIO
    cava_SOURCES += input/sndio.c
endif

if NCURSES
    cava_SOURCES += output/terminal_ncurses.c
endif


if !SYSTEM_LIBINIPARSER
    cava_LDADD = -liniparser
    cava_SOURCES += iniparser/libiniparser.la
    cava_LDADD += -Liniparser/.libs
    cava_CPPFLAGS += -Iiniparser/src
endif


