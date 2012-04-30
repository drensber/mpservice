#!/bin/sh
#
#
set -e

MPG123_BASE=mpg123-1.10.0

if [ ! -e mpg123 ]; then
    ln -s ${MPG123_BASE} mpg123
fi

if [ ! -d ${MPG123_BASE} ]; then
    tar xjf ../../downloads/${MPG123_BASE}.tar.bz2
    cd ${MPG123_BASE}
        if [ "${MPSERVICE_BUILD_PLATFORM}" = "x86-uClibc-linux" ]; then
	    ./configure --host=${ARCH}-linux CC=${CROSS_COMPILE}gcc
	elif [ "${MPSERVICE_BUILD_PLATFORM}" = "x86-macos-standalone" ]; then 
	    ./configure --with-cpu=x86-64
	else
	    ./configure
	fi
    cd -
fi
