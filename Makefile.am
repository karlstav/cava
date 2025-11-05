AUTOMAKE_OPTIONS = foreign

ACLOCAL_AMFLAGS = -I m4

CLEANFILES = $(NULL)

# Install the pkgconfig file for the library
pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libcava.pc

# Generate the pkgconfig file
libcava.pc: Makefile.am
	@echo "Generating $@..."
	@mkdir -p $(@D)
	@echo "prefix=$(prefix)" > $@
	@echo "exec_prefix=$(exec_prefix)" >> $@
	@echo "libdir=$(libdir)" >> $@
	@echo "includedir=$(includedir)" >> $@
	@echo "Name: $(PKG_CONFIG_NAME)" >> $@
	@echo "Description: $(PKG_CONFIG_DESC)" >> $@
	@echo "Version: $(PKG_CONFIG_VERSION)" >> $@
	@echo "Libs: -L\$(libdir) -lcava" >> $@ # Link against libcava library
	@echo "Cflags: -I\$(includedir)/cava -I\$(includedir)/cava/input -I\$(includedir)/cava/output" >> $@

# Clean up the generated pkgconfig file
CLEANFILES += libcava.pc

# Define the library to be built
# 'lib' directory implies shared library by default with Automake.
# Use 'noinst_LTLIBRARIES' if you don't want to install it but build it for internal use.
lib_LTLIBRARIES = libcava.la

AM_LIBTOOL_VERSION = $(AM_LIBTOOL_CURRENT):$(AM_LIBTOOL_REVISION):$(AM_LIBTOOL_RELEASE)

libcava_la_LDFLAGS = -version-info $(AM_LIBTOOL_VERSION)

bin_PROGRAMS = cava

cavadir = $(top_srcdir)
cava_SOURCES = cava.c
libcava_la_SOURCES = cavacore.c config.c input/common.c input/fifo.c input/shmem.c \
               output/common.c output/terminal_noncurses.c output/raw.c output/noritake.c

cava_CPPFLAGS = \
	-DPACKAGE=\"$(PACKAGE)\" \
	-DVERSION=\"$(VERSION)\" \
	-DLIB_VERSION=\"$(VERSION)\" \
	-D_POSIX_SOURCE \
	-D_POSIX_C_SOURCE=200809L \
	-D_XOPEN_SOURCE_EXTENDED \
	-DFONTDIR=\"@FONT_DIR@\" \
	-DFONTFILE=\"@FONT_FILE@\" \
	-I$(srcdir) \
	-I$(srcfir)/input \
	-I$(srcfir)/output \
	-I$(srcdir)/include

AM_CFLAGS = $(cava_CPPFLAGS)

cava_CFLAGS = -std=c99 -Wall -Wextra -Wno-unused-result -Wno-unknown-warning-option -Wno-maybe-uninitialized -Wno-vla-parameter

EXTRA_DIST = \
	include \
    output/shaders/pass_through.vert \
    output/shaders/bar_spectrum.frag \
    output/shaders/northern_lights.frag \
    output/shaders/winamp_line_style_spectrum.frag \
    output/shaders/spectrogram.frag \
    output/shaders/eye_of_phi.frag \
    output/themes/solarized_dark \
    output/themes/tricolor \
    example_files/config \
    cava.psf \
    LICENSE \
    run_all_tests.sh \
    SHADERS.md \
    .clang-format \
    CMakeLists.txt

# Install entire directories preserving structure
dist-hook:
	cp -r include/cava $(DESTDIR)$(includedir)
# Define the target to install the headers
install-exec-hook:
	@$(MKDIR_P) $(DESTDIR)$(includedir)/cava
# Install entire directories preserving structure
install-data-hook:
	cp -r include/cava/* $(DESTDIR)$(includedir)/cava/
# Define uninstall hook to remove the directory
uninstall-hook:
	$(AM_V_GEN) rm -rf "$(DESTDIR)$(includedir)/cava"

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
    CLEANFILES += cava.bdf cava.fnt

cava.fnt: ${srcdir}/cava.psf
	${PSF2BDF} --fontname="-gnu-cava-medium-r-normal--16-160-75-75-c-80-iso10646-1" ${srcdir}/cava.psf cava.bdf
	${VTFONTCVT} -o cava.fnt cava.bdf
endif
endif

if ALSA
    libcava_la_SOURCES += input/alsa.c input/alsa.h
endif

if PORTAUDIO
    libcava_la_SOURCES += input/portaudio.c input/portaudio.h
endif

if PIPEWIRE
    libcava_la_SOURCES += input/pipewire.c input/pipewire.h
endif

if PULSE
    libcava_la_SOURCES += input/pulse.c input/pulse.h
endif

if SNDIO
    libcava_la_SOURCES += input/sndio.c input/sndio.h
endif

if OSS
    libcava_la_SOURCES += input/oss.c input/oss.h
endif

if JACK
    libcava_la_SOURCES += input/jack.c input/jack.h
endif

if NCURSES
    libcava_la_SOURCES += output/terminal_ncurses.c output/terminal_bcircle.c \
                    output/terminal_ncurses.h output/terminal_bcircle.h

endif

if SDL
    libcava_la_SOURCES += output/sdl_cava.c output/sdl_cava.h
endif

if SDL_GLSL
    libcava_la_SOURCES += output/sdl_glsl.c output/sdl_glsl.h
endif

cava_SOURCES += $(libcava_la_SOURCES)
