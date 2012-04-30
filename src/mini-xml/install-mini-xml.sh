#!/bin/sh
#
#
set -e

MXML_BASE=mxml-2.7

if [ ! -f mxml ]; then
    ln -s ${MXML_BASE} mxml
fi

if [ ! -d ${MXML_BASE} ]; then
    tar xzf ../../downloads/${MXML_BASE}.tar.gz
    cd ${MXML_BASE}
        if [ "${MPSERVICE_BUILD_PLATFORM}" = "x86-uClibc-linux" ]; then
           ./configure --enable-shared --host=${ARCH}-linux CC=${CROSS_COMPILE}gcc
	else
           ./configure --enable-shared
	fi
    cd -
    patch -p0 -b -V simple < Makefile.patch
fi