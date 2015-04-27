CC       = gcc
CFLAGS   = -Wall -Wextra
CPPFLAGS =
LDFLAGS  = -lasound -lm -lfftw3 -lpthread

INSTALL     = install
INSTALL_BIN = $(INSTALL) -D -m 755

PREFIX ?= /usr/local
BINDIR  = $(DESTDIR)/$(PREFIX)/bin

all: cava

cava: cava.c

install: all
	$(INSTALL_BIN) cava $(BINDIR)/cava

uninstall:
	$(RM) $(BINDIR)/cava

clean:
	$(RM) cava

.PHONY: all clean install uninstall
