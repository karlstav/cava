AUTOMAKE_OPTIONS = foreign

if !SYSTEM_LIBINIPARSER
    SUBDIRS = iniparser
endif

ACLOCAL_AMFLAGS = -I m4

M_CPPFLAGS = -DSYSTEM_LIBINIPARSER=@SYSTEM_LIBINIPARSER@

bin_PROGRAMS = cava
cava_SOURCES = cava.c
cava_LDADD = -liniparser
cava_LDFLAGS = -L/usr/local/lib -Wl,-rpath /usr/local/lib
cava_CPPFLAGS = -DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\" \
           -D_POSIX_SOURCE -D _POSIX_C_SOURCE=200809L
cava_CFLAGS = -std=c99 -Wall -Wextra -Wno-unused-result -Wno-maybe-uninitialized

cava_font_dir = @FONT_DIR@
cava_font__DATA = cava.psf

if !SYSTEM_LIBINIPARSER
    cava_SOURCES += iniparser/libiniparser.la
    cava_LDADD += -Liniparser/.libs
    cava_CPPFLAGS += -Iiniparser/src
endif

cava_SOURCES += input/fifo.c output/raw.c output/terminal_noncurses.c

if HAVE_ALSA
    cava_SOURCES += input/alsa.c
endif

if HAVE_PULSE
    cava_SOURCES += input/pulse.c
endif

if HAVE_CURSES
    cava_SOURCES += output/terminal_ncurses.c output/terminal_bcircle.c
endif
