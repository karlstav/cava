#!/bin/sh

if [ -d .git ]; then
  git describe --always --tags --dirty > version # get version from git
else
  echo 0.4.2 > version # hard coded versions
fi

if libtoolize 2>/dev/null; then
  exit 1
elif glibtoolize 2>/dev/null; then
  exit 1
else
  echo 'Missing libtoolize'
  exit 1
fi
aclocal
autoconf
automake --add-missing

CONFIGDIR=$XDG_CONFIG_HOME/cava

if [ -z "$XDG_CONFIG_HOME" ]; then CONFIGDIR=$HOME/.config/cava; fi

mkdir -p $CONFIGDIR
cp -n example_files/config $CONFIGDIR/config

