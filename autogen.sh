#!/bin/sh

set -e

if [ -d .git ]; then
	git describe --always --tags --dirty >version # get version from git
else
	echo 1.0.0 >version # hard coded versions
fi

libtoolize
aclocal
autoconf
automake --add-missing
