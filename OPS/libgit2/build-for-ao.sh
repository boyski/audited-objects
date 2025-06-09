#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <package-X.Y.Z.tar.gz>" >&2
    exit 1
fi

set -e
: ${OS_CPU:=$(uname -m)_$(uname -s)}
if type -t git >/dev/null && [[ -d repo/.git ]]; then
    echo "Skip automatic update in order to keep multiple platforms at same rev"
    # (cd repo && git pull)
else
    srcpkg=$1
    (set -x; tar -xzf $srcpkg)
    srcdir=$(basename $srcpkg .tar.gz)
fi
blddir=$srcdir/${OS_CPU?}
rm -rf $blddir
set -x
mkdir -p $blddir
cd $blddir
cmake .. \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=local \
    -DBUILD_TESTS=OFF \
    -DUSE_SSH=OFF \
    -DUSE_HTTPS=OFF \
    -DCMAKE_OSX_ARCHITECTURES=x86_64
cmake --build .
cmake --build . --target install
