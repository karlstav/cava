#!/bin/sh
libtoolize
aclocal
autoconf
automake --add-missing

CONFIGDIR=$XDG_CONFIG_HOME/cava

if [ -z "$XDG_CONFIG_HOME" ]; then CONFIGDIR=$HOME/.config/cava; fi

mkdir -p $CONFIGDIR
cp -n example_files/config $CONFIGDIR/config

