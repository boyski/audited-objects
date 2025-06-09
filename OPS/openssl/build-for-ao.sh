#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <package-X.Y.Z.tar.gz>" >&2
    exit 1
fi

set -e

: ${PREFIX=$(/bin/pwd)}

srcpkg=$1

srcdir=$(basename $srcpkg .tar.gz)
tar -xzf $srcpkg

arch=$(uname -p)
cmd=./config

cd "$srcdir"

# Need the 'shared' here even though we use the static lib because
# that's the only way it builds PIC.
$cmd --prefix=${PREFIX?} shared no-zlib no-threads no-asm no-idea no-rc5 no-mdc2 -DNDEBUG

make clean all
