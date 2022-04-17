#!/bin/sh

if [ -d .git ]; then
  git describe --always --tags --dirty > version # get version from git
else
  echo 0.8.1 > version # hard coded versions
fi

libtoolize
aclocal
autoconf
automake --add-missing

xxd -i example_files/config config_file.h # make a hex dump of default config file to be generated in home dir on first launch
