#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <package-X.Y.tar.gz> [64]" >&2
    exit 1
fi

set -e

srcpkg=$1
srcdir=`basename $srcpkg .tar.gz`
gunzip -c $srcpkg | tar -xf -

cd "$srcdir"

make clean all

case `uname` in
    SunOS*) echo "\nREMEMBER TO COPY libcdb64.a TO .../64/libcdb.a !!" ;;
esac
