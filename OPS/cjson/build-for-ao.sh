#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 cJSON-master.zip" >&2
    exit 1
fi

set -e

srcpkg=$1
srcdir=$(basename $srcpkg .zip)
unzip $srcpkg

cd "$srcdir"

make clean all
