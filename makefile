PACKAGE ?= cava
VERSION ?= $(shell git describe --always --tags --dirty)

CC       = gcc
CFLAGS   = -std=c99 -Wall -Wextra
CPPFLAGS = -DPACKAGE=\"$(PACKAGE)\" -DVERSION=\"$(VERSION)\" \
           -D_POSIX_SOURCE -D _POSIX_C_SOURCE=200809L
LDLIBS   = -lasound -lm -lfftw3 -lpthread -lncursesw

INSTALL     = install
INSTALL_BIN = $(INSTALL) -D -m 755

PREFIX ?= /usr/local
BINDIR  = $(DESTDIR)/$(PREFIX)/bin

debug ?= 0

ifeq ($(debug),1)
CPPFLAGS += -DDEBUG
endif

all: cava

cava: cava.c

install: all
	$(INSTALL_BIN) cava $(BINDIR)/cava

uninstall:
	$(RM) $(BINDIR)/cava

clean:
	$(RM) cava

.PHONY: all clean install uninstall
