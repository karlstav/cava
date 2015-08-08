PACKAGE ?= cava
VERSION ?= $(shell git describe --always --tags --dirty)

ifeq ($(SYSTEM_INIPARSER),1)
CPPFLAGS_INIPARSER = -I/usr/include/iniparser4
LDLIBS_INIPARSER = -liniparser4
DEP_INIPARSER =
else
CPPFLAGS_INIPARSER = -Iiniparser/src
LDLIBS_INIPARSER = iniparser/libiniparser.a
DEP_INIPARSER = iniparser/libiniparser.a
endif

CC       = gcc
CFLAGS   += -std=c99 -Wall -Wextra
CPPFLAGS += -DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\" \
           -D_POSIX_SOURCE -D _POSIX_C_SOURCE=200809L $(CPPFLAGS_INIPARSER)
LDLIBS   = $(LDLIBS_INIPARSER) -lasound -lm -lfftw3 -lpthread $(shell ncursesw5-config --cflags --libs)

INSTALL     = install
INSTALL_BIN = $(INSTALL) -D -m 755

PREFIX ?= /usr/local
BINDIR  = $(DESTDIR)/$(PREFIX)/bin

CONFIGDIR = $(XDG_CONFIG_HOME)/cava

debug ?= 0

ifeq ($(debug),1)
CPPFLAGS += -DDEBUG
endif

all: cava check-env copyconf

cava: cava.c $(DEP_INIPARSER)

iniparser/libiniparser.a:
	cd iniparser && $(MAKE)

check-env:
ifndef XDG_CONFIG_HOME
    CONFIGDIR = $(HOME)/.config/cava
endif

copyconf:
	mkdir -p $(CONFIGDIR)
	cp -n example_files/config $(CONFIGDIR)/config

install: all
	$(INSTALL_BIN) cava $(BINDIR)/cava

uninstall:
	$(RM) $(BINDIR)/cava

clean:
	cd iniparser && $(MAKE) clean
	$(RM) cava

.PHONY: all clean install uninstall
