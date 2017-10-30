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

AS_IF([test "x$enable_debug" = "xyes"], [
  dnl enabling debug mode
  CPPFLAGS="$CPPFLAGS -DDEBUG"
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
AC_CHECK_LIB(asound, snd_pcm_open, have_alsa=yes, have_alsa=no)
    if [[ $have_alsa = "yes" ]] ; then
      LIBS="$LIBS -lasound"
      CPPFLAGS="$CPPFLAGS -DALSA"
    fi

    if [[ $have_alsa = "no" ]] ; then
      AC_MSG_NOTICE([WARNING: No alsa dev files found building without alsa support])
    fi


dnl ######################
dnl checking for pulse dev
dnl ######################
AC_CHECK_LIB(pulse-simple, pa_simple_new, have_pulse=yes, have_pulse=no)
    if [[ $have_pulse = "yes" ]] ; then
      LIBS="$LIBS -lpulse-simple -lpulse"
      CPPFLAGS="$CPPFLAGS -DPULSE"
    fi

    if [[ $have_pulse = "no" ]] ; then
      AC_MSG_NOTICE([WARNING: No pusleaudio dev files found building without pulseaudio support])
    fi

dnl ######################
dnl checking for sndio dev
dnl ######################
AC_CHECK_LIB(sndio, sio_open, have_sndio=yes, have_sndio=no)
    if [[ $have_sndio = "yes" ]] ; then
      LIBS="$LIBS -lsndio"
      CPPFLAGS="$CPPFLAGS -DSNDIO"
    fi

    if [[ $have_sndio = "no" ]] ; then
      AC_MSG_NOTICE([WARNING: No sndio dev files found building without sndio support])
    fi

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
	,
	AC_MSG_NOTICE([WARNING: building without ncursesw support ncursesw is recomended!])
	)



dnl ######################
dnl checking for system iniparser
dnl ######################

AC_SEARCH_LIBS([iniparser_load], [iniparser], [
	AC_CHECK_HEADERS([iniparser.h], [have_system_iniparser=yes])
])
AM_CONDITIONAL([SYSTEM_LIBINIPARSER], [test "x$have_system_iniparser" = "xyes"])
if test "x$have_system_iniparser" = "xyes"; then
    AC_SUBST(SYSTEM_LIBINIPARSER, 1)
    AC_MSG_NOTICE([Using installed iniparser])
else
    AC_SUBST(SYSTEM_LIBINIPARSER, 0)
    AC_CONFIG_FILES(iniparser/Makefile)
    AC_MSG_NOTICE([Building iniparser])
fi



dnl ############################
dnl checking if legacy iniparser is enabled
dnl ############################

AC_ARG_ENABLE([legacy_iniparser],
  AS_HELP_STRING([--enable-legacy_iniparser],
    [enable legacy iniparser mode necessary to build cava with iniparser < 4.0])
)

AS_IF([test "x$enable_legacy_iniparser" = "xyes"], [
  dnl enabling legacy-iniparser mode
  CPPFLAGS="$CPPFLAGS -DLEGACYINIPARSER"
])

dnl ############################
dnl Set font directory
dnl ############################
DEFAULT_FONT_DIR="/usr/share/consolefonts"
AC_ARG_VAR(FONT_DIR, [Directory where the font will be installed.])
if test -z "$FONT_DIR" ; then
  FONT_DIR="$DEFAULT_FONT_DIR"
fi




AC_CONFIG_FILES([Makefile])
AC_OUTPUT





