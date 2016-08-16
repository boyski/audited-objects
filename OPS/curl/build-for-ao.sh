#!/bin/bash

if [ -z "$2" ]; then
    echo "Usage: $0 <curl-X.Y.Z.tar.gz> <zlib-X.Y.tar.gz> [64]" >&2
    exit 1
fi

# We build zlib and libcurl together since they have a tight relationship.

set -e

: ${PWD:=`/bin/pwd`}
: ${OS_CPU:=`uname -m`_`uname -s`}

curlpkg=$1
zlibpkg=$2
bits=$3

gunzip -c $curlpkg | tar -xf -

curldir=$PWD/`basename $curlpkg .tar.gz`
zlibdir=$PWD/`basename $zlibpkg .tar.gz`
zlibinst=$PWD/zlib-inst
blddir=$curldir/${OS_CPU?}${bits:+_$bits}

case ${OS_CPU?} in
    Darwin_*)
	xflags="$xflags -mmacosx-version-min=10.5 -arch i386 -arch x86_64"
	conf_flags=--disable-dependency-tracking
	bitflag=
	;;
    *)
	bitflag=${bits:+-m$bits}
	case "$bits" in
	    64) xflags="-fPIC" ;;
	    *)  xflags="-fpic" ;;
	esac
	;;
esac

# First we have to build zlib to go with libcurl ...
(
    set -e
    rm -rf "$zlibdir"
    gunzip -c $zlibpkg | tar -xf -
    cd "$zlibdir"
    CFLAGS="-Os $bitflag $xflags" sh configure --prefix=$zlibinst
    make clean all
    make install
)

# ... then configure libcurl itself.
# Since libcurl is no longer used in the auditor, size and performance
# and static-ness are not so critical. We've removed -fpic and would
# allow --enable-debug and -g, using it as a shared library, allowing
# SSL, etc. None of these would do any good right now but there's no
# need to deprecate them either.

conf_flags="$conf_flags \
    --with-zlib="$zlibinst" \
    --without-ca-bundle \
    --enable-static --disable-shared \
    --without-ssl \
    --without-libidn \
    --disable-ftp \
    --disable-ldap \
    --disable-ldaps \
    --disable-telnet \
    --disable-dict \
    --disable-file \
    --disable-ipv6 \
    --disable-tftp \
    --disable-ares \
    --disable-manual \
    --disable-sspi \
    --disable-crypto-auth \
    --disable-debug"

mkdir -p "$blddir"
cd "$blddir"

# We have to do this awkward stuff to get a universal binary
# for Mac OS X because of the crazy way libcurl handles bitness
# and headers.
if [[ "$OS_CPU" = Darwin_* ]]; then
    CFLAGS="-Os $bitflag -m32" sh ../configure $conf_flags
    cp include/curl/curlbuild.h include/curl/curlbuild32.h
    make distclean >/dev/null 2>&1 ||:

    CFLAGS="-Os $bitflag -m64" sh ../configure $conf_flags
    cp include/curl/curlbuild.h include/curl/curlbuild64.h
    make distclean >/dev/null 2>&1 ||:
fi

CFLAGS="-Os $bitflag $xflags" sh ../configure $conf_flags
 
if [[ "$OS_CPU" = Darwin_* ]]; then
    cat > include/curl/curlbuild.h <<EOF
#ifdef __LP64__
#include "curlbuild64.h"
#else
#include "curlbuild32.h"
#endif 
EOF
fi

make
