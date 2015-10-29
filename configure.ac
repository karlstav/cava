AC_INIT([cava], [0.3.5], [karl@stavestrand.no])
AM_INIT_AUTOMAKE
AC_PROG_CC

dnl ######################
dnl checking for alsa dev
dnl ######################
ALSA_MIN_VERSION=1.0
PKG_CHECK_MODULES(ALSA, alsa >= ${ALSA_MIN_VERSION},, 
	AC_MSG_ERROR([Missing alsa development package: libasound2-dev or alsa-lib-devel]))



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


dnl to do, use "noncurses mode" if no ncurses installed, check for systemwide iniparser, use bundled if non found

AC_CONFIG_FILES([Makefile])
AC_OUTPUT



