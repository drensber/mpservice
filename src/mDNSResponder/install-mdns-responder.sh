#!/bin/sh
#
#
set -e

MDNS_BASE=mDNSResponder-107.6

if [ ! -f mDNSResponder-107.6 ]; then
    ln -s ${MDNS_BASE} mDNSResponder
fi

if [ ! -d ${MDNS_BASE} ]; then
    tar xzf ../../downloads/${MDNS_BASE}.tar.gz    
    patch -p0 -b -V simple < Makefile.patch
fi
