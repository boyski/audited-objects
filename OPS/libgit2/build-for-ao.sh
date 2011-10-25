#!/bin/bash

set -e
if type -t git >/dev/null; then
    if [[ -d repo/.git ]]; then
	(cd repo && git pull)
    else
	git clone git://github.com/libgit2/libgit2.git repo
    fi
fi
rm -rf ${OS_CPU?}
mkdir ${OS_CPU?}
cd ${OS_CPU?}
cmake -DBUILD_SHARED_LIBS=NO \
    -DCMAKE_OSX_ARCHITECTURES="i386;x86_64" \
    -DCMAKE_BUILD_TYPE=Release \
    ../repo
make VERBOSE=1
