#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <package-X.Y.Z.tar.gz> [64]" >&2
    exit 1
fi

set -e

cd ..

: ${CWD=`/bin/pwd`}

: ${PREFIX=`/bin/pwd`}

cd ${CWD?}

srcpkg=$1
bits=$2

srcdir=`basename $srcpkg .tar.gz`
gunzip -c $srcpkg | tar -xf -

case `uname -s` in
    Sun*) platform=solaris ;;
    Linux*) platform=linux ;;
esac

arch=`uname -p`
case $arch in
    i386*)  arch=x86 ;;
esac

case "$platform-$arch-${bits:-32}" in
    solaris-x86-32) cmd="./Configure ${platform}-x86-gcc" ;;
    solaris-x86-64) cmd="./Configure ${platform}64-x86_64-gcc" ;;
    solaris-sparc-32) cmd="./Configure ${platform}-sparcv8-gcc" ;;
    solaris-sparc-64) cmd="./Configure ${platform}64-sparcv9-gcc" ;;
    *) cmd=./config ;;
esac

cd "$srcdir"

# Need the 'shared' here even though we use the static lib because
# that's the only way it builds PIC.
$cmd --prefix=${PREFIX?} shared no-zlib no-threads no-asm no-idea no-rc5 no-mdc2 -DNDEBUG

make clean all
