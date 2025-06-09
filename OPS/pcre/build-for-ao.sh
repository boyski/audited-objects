#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <package-X.Y.tar.bz2>" >&2
    exit 1
fi

set -e

: ${PWD:=$(/bin/pwd)}
: ${OS_CPU:=$(uname -m)_$(uname -s)}

srcpkg=$1
bits=$2

srcdir=$(basename $srcpkg .tar.bz2)
tar -xjf $srcpkg

blddir=${OS_CPU?}${bits:+_$bits}

case ${OS_CPU?} in
    Darwin_*)
	xflags="-mmacosx-version-min=10.5 -arch i386 -arch x86_64"
	conf_flags=--disable-dependency-tracking
	;;
    *)
	xflags="-fPIC"
	;;
esac

mkdir -p "$srcdir/$blddir"
cd "$srcdir/$blddir"

CFLAGS="-Os ${bits:+-m$bits} $xflags -DNDEBUG" \
    sh ../configure --disable-cpp --enable-utf8 --enable-static $conf_flags

make clean all
