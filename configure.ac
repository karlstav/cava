AC_INIT([cava], [0.3.5], [karl@stavestrand.no])
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign])
AC_CONFIG_MACRO_DIRS([m4])
AM_PROG_AR
LT_INIT
AC_PROG_CC
AC_PROG_CC_STDC

AM_PROG_LIBTOOL



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
dnl checking for alsa dev
dnl ######################
AC_CHECK_LIB(asound, snd_pcm_open, have_alsa=yes, have_alsa=no)
    if [[ $have_alsa = "yes" ]] ; then
      LIBS="$LIBS -lasound"
    fi

    if [[ $have_alsa = "no" ]] ; then
      AC_MSG_ERROR([alsa dev files library is required!])
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

PKG_CHECK_MODULES([fftw3], [fftw3 >= 3], [
	AC_SUBST(fftw3_LIBS)
	AC_SUBST(fftw3_CFLAGS)
	CPPFLAGS="$CPPFLAGS $fftw3_CFLAGS"
	AC_CHECK_HEADERS([fftw3.h],
		LIBS="$LIBS $fftw3_LIBS"
	,
		if test "$fftw" = "yes" ; then
			AC_MSG_ERROR([missing fftw3.h header])
		fi
	)
],
	if test "$fftw" = "yes" ; then
		AC_MSG_ERROR([fftw3 library is required!])
	fi
)


dnl ######################
dnl checking for ncursesw
dnl ######################


curses_config_bin="ncursesw6-config ncursesw5-config"

AC_PATH_PROGS(CURSES_CONFIG, $curses_config_bin)
if test "$CURSES_CONFIG" != "" ; then
	CPPFLAGS="$CPPFLAGS `$CURSES_CONFIG --cflags`"
	LIBS="$LIBS `$CURSES_CONFIG --libs`"
fi
AC_CHECK_LIB(ncursesw, initscr,
	curses_lib=ncursesw
)

AC_CHECK_LIB($curses_lib, initscr,
	if test "$CURSES_CONFIG" = "" ; then
		LIBS="$LIBS -l$curses_lib"
	fi
	,
	AC_MSG_ERROR([$curses_lib library is required])
	)
AC_CHECK_HEADERS([curses.h], , AC_MSG_ERROR([missing curses.h header]))


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

dnl to do, use "noncurses mode" if no ncurses installed. use mpd fifo only if no alsa dev files found 

AC_CONFIG_FILES([Makefile])
AC_OUTPUT



