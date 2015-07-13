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

debug ?= 0

ifeq ($(debug),1)
CPPFLAGS += -DDEBUG
endif

all: iniparser cava

cava: cava.c

iniparser:
	cd iniparser && $(MAKE)

install: all
	$(INSTALL_BIN) cava $(BINDIR)/cava

uninstall:
	$(RM) $(BINDIR)/cava

clean:
	cd iniparser && $(MAKE) clean
	$(RM) cava

.PHONY: iniparser all clean install uninstall
