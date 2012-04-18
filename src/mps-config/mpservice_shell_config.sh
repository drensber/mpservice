#!/bin/sh

# "source" the configuration parameters
TMPFILE=`mktemp /tmp/tmp.XXXXXX`
/mpservice/bin/mps-config flatten > ${TMPFILE}
. ${TMPFILE}
rm -f ${TMPFILE}
