#!/bin/sh

if [ -d .git ]; then
  git describe --always --tags --dirty > version # get version from git
else
  echo 0.6.0 > version # hard coded versions
fi

libtoolize
aclocal
autoconf
automake --add-missing

CONFIGDIR=$XDG_CONFIG_HOME/cava

if [ -z "$XDG_CONFIG_HOME" ]; then CONFIGDIR=$HOME/.config/cava; fi

mkdir -p $CONFIGDIR
cp -n example_files/config $CONFIGDIR/config

