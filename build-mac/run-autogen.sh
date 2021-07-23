#!/bin/sh

logfile="`pwd`/update.log"
echo preparing
pushd ..
    ./autogen.sh --without-openssl --without-gnutls --without-sasl --without-curl --without-expat --without-zlib --disable-dependency-tracking
    make clean
    make -j 4 > "$logfile" 2>&1
popd
