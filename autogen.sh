#!/bin/sh

set -e

if [ -d .git ]; then
  git describe --always --tags --dirty > version # get version from git
else
  echo 0.10.7 > version # hard coded versions
fi

libtoolize
mkdir -p m4
aclocal -I m4 # OpenGL (sdl_glsl) build support is now enabled by default when OpenGL is present, without requiring autoconf-archive
autoconf
automake --add-missing
