#!/bin/bash
set -e

MPSERVICE_PACKAGES="http://opensource.apple.com/tarballs/mDNSResponder/mDNSResponder-107.6.tar.gz http://ftp.easysw.com/pub/mxml/2.7/mxml-2.7.tar.gz http://sourceforge.net/projects/mpg123/files/mpg123/1.10.0/mpg123-1.10.0.tar.bz2 http://www.boa.org/boa-0.94.13.tar.gz"

if [ ${#} == "1" ] && [ ${@} == "local" ]; then
  if [ "${THIRDPARTY_PACKAGE_DIR}" == "" ]; then
    echo "THIRDPARTY_PACKAGE_DIR environment variable must be set to use \"local\" option."
    exit 1
  fi

  cd downloads 
    for i in ${MPSERVICE_PACKAGES}
    do
      ln -sf ${THIRDPARTY_PACKAGE_DIR}/`basename ${i}`
    done
  cd - 
elif [ ${#} == "0" ]; then
  cd downloads
    for i in ${MPSERVICE_PACKAGES}
    do
      wget ${i}
    done
  cd -
else
  echo "usage: install_thirdparty.sh [local]"
fi
