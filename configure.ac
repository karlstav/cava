AC_INIT([cava], [m4_esyscmd_s([cat version])], [karl@stavestrand.no])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign])
dnl AC_CONFIG_MACRO_DIRS([m4])
AM_PROG_AR
LT_INIT
AC_PROG_CC
AC_PROG_CC_STDC

AM_PROG_LIBTOOL


dnl ############################
dnl checking if debug is enabled
dnl ############################

AC_ARG_ENABLE([debug],
  AS_HELP_STRING([--enable-debug],
    [enable debug messages and frequency table output])
)

AS_IF([test "x$enable_debug" != "xyes"], [
  dnl enabling debug mode
  CPPFLAGS="$CPPFLAGS -DNDEBUG"
])

AC_ARG_ENABLE([asan],
  AS_HELP_STRING([--enable-asan],
    [build with AddressSanitizer])
)

AS_IF([test "x$enable_asan" = "xyes"], [
  dnl enabling asan
  CPPFLAGS="$CPPFLAGS -fsanitize=address"
  LDFLAGS="$LDFLAGS -fsanitize=address"
])

AC_ARG_ENABLE([tsan],
  AS_HELP_STRING([--enable-tsan],
    [build with ThreadSanitizer])
)

AS_IF([test "x$enable_tsan" = "xyes"], [
  dnl enabling tsan
  CPPFLAGS="$CPPFLAGS -fsanitize=thread"
  LDFLAGS="$LDFLAGS -fsanitize=thread"
])

AC_ARG_ENABLE([ubsan],
  AS_HELP_STRING([--enable-ubsan],
    [build with UndefinedBehaviorSanitizer])
)

AS_IF([test "x$enable_ubsan" = "xyes"], [
  dnl enabling ubsan
  CPPFLAGS="$CPPFLAGS -fsanitize=undefined"
  LDFLAGS="$LDFLAGS -fsanitize=undefined"
])


dnl ######################
dnl checking for pthread
dnl ######################

AC_CHECK_HEADERS([pthread.h],
	AC_CHECK_LIB(pthread, pthread_create, LIBS="$LIBS -lpthread",
		AC_MSG_ERROR([pthread.h found but there is no pthread library to make use of])
	),
	AC_MSG_ERROR([no pthread.h header header file found])
)

dnl ######################
dnl checking for alloca.h
dnl ######################

AC_CHECK_HEADER([alloca.h], [CPPFLAGS="$CPPFLAGS -DHAVE_ALLOCA_H"])

dnl ######################
dnl checking for alsa dev
dnl ######################
AC_ARG_ENABLE([input_alsa],
  AS_HELP_STRING([--disable-input-alsa],
    [do not include support for input from alsa streams])
)

AS_IF([test "x$enable_input_alsa" != "xno"], [
  AC_CHECK_LIB(asound, snd_pcm_open, have_alsa=yes, have_alsa=no)
  if [[ $have_alsa = "yes" ]] ; then
    LIBS="$LIBS -lasound"
    CPPFLAGS="$CPPFLAGS -DALSA"
  fi
  if [[ $have_alsa = "no" ]] ; then
    AC_MSG_NOTICE([WARNING: No alsa dev files found building without alsa support])
  fi],
  [have_alsa=no]
)

AM_CONDITIONAL([ALSA], [test "x$have_alsa" = "xyes"])


dnl ######################
dnl checking for pulse dev
dnl ######################
AC_ARG_ENABLE([input_pulse],
  AS_HELP_STRING([--disable-input-pulse],
    [do not include support for input from pulseaudio])
)

AS_IF([test "x$enable_input_pulse" != "xno"], [
  AC_CHECK_LIB(pulse-simple, pa_simple_new, have_pulse=yes, have_pulse=no)
  if [[ $have_pulse = "yes" ]] ; then
    LIBS="$LIBS -lpulse-simple -lpulse"
    CPPFLAGS="$CPPFLAGS -DPULSE"
  fi

  if [[ $have_pulse = "no" ]] ; then
    AC_MSG_NOTICE([WARNING: No pusleaudio dev files found building without pulseaudio support])
  fi],
  [have_pulse=no]
)

AM_CONDITIONAL([PULSE], [test "x$have_pulse" = "xyes"])

dnl ######################
dnl checking for portaudio dev
dnl ######################
AC_ARG_ENABLE([input_portaudio],
  AS_HELP_STRING([--disable-input-portaudio],
    [do not include support for input from portaudio])
)

AS_IF([test "x$enable_input_portaudio" != "xno"], [
  AC_CHECK_LIB(portaudio, Pa_Initialize, have_portaudio=yes, have_portaudio=no)
  if [[ $have_portaudio = "yes" ]] ; then
    LIBS="$LIBS -lportaudio"
    CPPFLAGS="$CPPFLAGS -DPORTAUDIO"
  fi

  if [[ $have_portaudio = "no" ]] ; then
    AC_MSG_NOTICE([WARNING: No portaudio dev files found building without portaudio support])
  fi],
  [have_portaudio=no]
)

AM_CONDITIONAL([PORTAUDIO], [test "x$have_portaudio" = "xyes"])

dnl ######################
dnl checking for sndio dev
dnl ######################
AC_ARG_ENABLE([input_sndio],
  AS_HELP_STRING([--disable-input-sndio],
    [do not include support for input from sndio])
)

AS_IF([test "x$enable_input_sndio" != "xno"], [
  AC_CHECK_LIB(sndio, sio_open, have_sndio=yes, have_sndio=no)
  if [[ $have_sndio = "yes" ]] ; then
    LIBS="$LIBS -lsndio"
    CPPFLAGS="$CPPFLAGS -DSNDIO"
  fi

  if [[ $have_sndio = "no" ]] ; then
    AC_MSG_NOTICE([WARNING: No sndio dev files found building without sndio support])
  fi],
  [have_portaudio=no]
)

AM_CONDITIONAL([SNDIO], [test "x$have_sndio" = "xyes"])

dnl ######################
dnl checking for math lib
dnl ######################
AC_CHECK_LIB(m, sqrt, have_m=yes, have_m=no)
    if [[ $have_m = "yes" ]] ; then
      LIBS="$LIBS -lm"
    fi

    if [[ $have_m = "no" ]] ; then
      AC_MSG_ERROR([math library is required!])
    fi


dnl ######################
dnl checking for fftw3 
dnl ######################
AC_CHECK_LIB(fftw3,fftw_execute, have_fftw=yes, have_fftw=no)
    if [[ $have_fftw = "yes" ]] ; then
      LIBS="$LIBS -lfftw3"
    fi

    if [[ $have_fftw = "no" ]] ; then
      AC_MSG_ERROR([fftw library is required!])
    fi

dnl ######################
dnl checking for ncursesw
dnl ######################
AC_ARG_ENABLE([output_ncurses],
  AS_HELP_STRING([--disable-output-ncurses],
    [do not include support for output with ncurses])
)

AS_IF([test "x$enable_output_ncurses" != "xno"], [
  curses_config_bin="ncursesw6-config ncursesw5-config"

  AC_PATH_PROGS(CURSES_CONFIG, $curses_config_bin)

  AC_CHECK_LIB(ncursesw, initscr,
    curses_lib=ncursesw
  )

  AC_CHECK_LIB($curses_lib, initscr,
    if test "$CURSES_CONFIG" = "" ; then
      LIBS="$LIBS -l$curses_lib"
      CPPFLAGS="$CPPFLAGS -DNCURSES"
    fi
    if test "$CURSES_CONFIG" != "" ; then
      CPPFLAGS="$CPPFLAGS `$CURSES_CONFIG --cflags` -DNCURSES"
      LIBS="$LIBS `$CURSES_CONFIG --libs`"
    fi

    AC_CHECK_HEADERS([curses.h], , AC_MSG_ERROR([missing curses.h header]))
    have_ncurses=yes,

    AC_MSG_NOTICE([WARNING: building without ncursesw support ncursesw is recomended!])
    have_ncurses=no
  )],
  [have_ncurses=no]
)

AM_CONDITIONAL([NCURSES], [test "x$have_ncurses" = "xyes"])


dnl ######################
dnl checking for system iniparser
dnl ######################

AC_ARG_ENABLE([system_iniparser],
  AS_HELP_STRING([--disable-system-iniparser],
    [do not use system iniparser library (use bundled iniparser library)])
)

AS_IF([test "x$enable_system_iniparser" != "xno"], [
  AC_SEARCH_LIBS([iniparser_load], [iniparser], [
    AC_CHECK_HEADERS([iniparser.h], [have_system_iniparser=yes])
    ])
  ],
  [have_system_iniparser=no]
)

AM_CONDITIONAL([SYSTEM_LIBINIPARSER], [test "x$have_system_iniparser" = "xyes"])

if test "x$have_system_iniparser" = "xyes"; then
  AC_SUBST(SYSTEM_LIBINIPARSER, 1)
  AC_MSG_NOTICE([Using installed iniparser])
  LIBS="$LIBS -liniparser"
  AC_LINK_IFELSE([AC_LANG_PROGRAM([[#include <iniparser.h>]],
    [[dictionary* ini;
    const char *keys[3];
    iniparser_getseckeys(ini, "eq", keys);]])],
    [AC_MSG_RESULT(iniparser > 3.2 test OK)],
    [AC_MSG_RESULT(iniparser > 3.2 test failed falling back to legacy iniparser mode)
    CPPFLAGS="$CPPFLAGS -DLEGACYINIPARSER"])
else
  AC_SUBST(SYSTEM_LIBINIPARSER, 0)
  AC_CONFIG_FILES(iniparser/Makefile)
  AC_MSG_NOTICE([Building iniparser])
fi


dnl ############################
dnl Set font directory
dnl ############################
DEFAULT_FONT_DIR="${datarootdir}/consolefonts"
AC_ARG_VAR(FONT_DIR, [Directory where the font will be installed.])
if test -z "$FONT_DIR" ; then
  FONT_DIR="$DEFAULT_FONT_DIR"
fi

AC_CANONICAL_HOST

build_linux=no
build_windows=no
build_mac=no

AC_MSG_NOTICE([Checking OS])
# Detect the target system
case "${host_os}" in
    linux*)
        AC_MSG_NOTICE([Linux detected])
        build_linux=yes
        ;;
    darwin*)
        AC_MSG_NOTICE([OSX detected])
        build_mac=yes
        ;;
    *)
        AC_MSG_ERROR(["OS $host_os is not supported"])
        ;;
esac

# Pass the conditionals to automake
AM_CONDITIONAL([LINUX], [test "$build_linux" = "yes"])
AM_CONDITIONAL([OSX], [test "$build_mac" = "yes"])


AC_CONFIG_FILES([Makefile])
AC_OUTPUT





