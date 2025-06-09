#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <package-X.Y.tar.gz>" >&2
    exit 1
fi

set -e

srcpkg=$1

srcdir=$(basename $srcpkg .tar.gz)
tar -xzf $srcpkg

cd "$srcdir"

make -f Makefile.gcc CFLAGS=-fPIC clean libkaz.a
