PACKAGE ?= cava
VERSION ?= $(shell git describe --always --tags --dirty)

CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra
CPPFLAGS = -DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\" \
           -D_POSIX_SOURCE -D _POSIX_C_SOURCE=200809L
LDLIBS   = ./iniparser/libiniparser.a -lasound -lm -lfftw3 -lpthread $(shell ncursesw5-config --cflags --libs)

INSTALL     = install
INSTALL_BIN = $(INSTALL) -D -m 755

PREFIX ?= /usr/local
BINDIR  = $(DESTDIR)/$(PREFIX)/bin

CONFIGDIR = $(XDG_CONFIG_HOME)/cava

debug ?= 0

ifeq ($(debug),1)
CPPFLAGS += -DDEBUG
endif

all: iniparser cava check-env copyconf

cava: cava.c

iniparser:
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

.PHONY: iniparser all clean install uninstall
