AUTOMAKE_OPTIONS = foreign

if !SYSTEM_LIBINIPARSER
    SUBDIRS = iniparser
endif

ACLOCAL_AMFLAGS = -I m4

M_CPPFLAGS = -DSYSTEM_LIBINIPARSER=@SYSTEM_LIBINIPARSER@

bin_PROGRAMS = cava
cava_SOURCES = cava.c
cava_LDADD = -liniparser
cava_CPPFLAGS =


if !SYSTEM_LIBINIPARSER
    cava_SOURCES += iniparser/libiniparser.la
    cava_LDADD += -Liniparser/.libs
    cava_CPPFLAGS += -Iiniparser/src
endif


