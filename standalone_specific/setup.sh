#!/bin/bash

TOP_DIR=`pwd`

#
# Create a shell source-able file that contains all of the environment
# settings that are necessary for the standalone environment. 
#
cat > ${TOP_DIR}/env.sh <<EOF
export LD_LIBRARY_PATH=${TOP_DIR}/lib

export MPSERVICE_CONFIGURATION_FILE=${TOP_DIR}/mpservice_configuration.xml
export MPSERVICE_CONSTANTS_FILE=${TOP_DIR}/misc/mpservice_constants.xml
EOF

. env.sh

#
# Create shell scripts to start client programs with the necessary 
# environment variable settings.
#
rm -f ${TOP_DIR}/cgi-bin/*
[ -e ${TOP_DIR}/cgi-bin/mps-library ] || cat > ${TOP_DIR}/cgi-bin/mps-library <<EOF
#!/bin/sh

. ${TOP_DIR}/env.sh
exec ${TOP_DIR}/bin/mps-library "\$@"
EOF

[ -e ${TOP_DIR}/cgi-bin/mps-player ] || cat > ${TOP_DIR}/cgi-bin/mps-player <<EOF
#!/bin/sh

. ${TOP_DIR}/env.sh
exec ${TOP_DIR}/bin/mps-player "\$@"
EOF

[ -e ${TOP_DIR}/cgi-bin/mps-playlist ] || cat > ${TOP_DIR}/cgi-bin/mps-playlist <<EOF
#!/bin/sh

. ${TOP_DIR}/env.sh
exec ${TOP_DIR}/bin/mps-playlist "\$@"
EOF

[ -e ${TOP_DIR}/cgi-bin/mps-config ] || cat > ${TOP_DIR}/cgi-bin/mps-config <<EOF
#!/bin/sh

. ${TOP_DIR}/env.sh
exec ${TOP_DIR}/bin/mps-config "\$@"
EOF

[ -e ${TOP_DIR}/cgi-bin/mps-constants ] || cat > ${TOP_DIR}/cgi-bin/mps-constants <<EOF
#!/bin/sh

. ${TOP_DIR}/env.sh
exec ${TOP_DIR}/bin/mps-constants "\$@"
EOF

chmod 755 ${TOP_DIR}/cgi-bin/mps-library ${TOP_DIR}/cgi-bin/mps-player ${TOP_DIR}/cgi-bin/mps-playlist ${TOP_DIR}/cgi-bin/mps-config ${TOP_DIR}/cgi-bin/mps-constants

#
# Configure some of the parameters in mpservice_constants.xml for the
# install location.
#
${TOP_DIR}/bin/mps-constants set platform "standalone"
${TOP_DIR}/bin/mps-constants set live_librarydb_file "/tmp/live-library.db" 
${TOP_DIR}/bin/mps-constants set persistent_librarydb_file "${TOP_DIR}/misc/persistent-library.db"
${TOP_DIR}/bin/mps-constants set live_playlist_file "/tmp/live-playlist.db"
${TOP_DIR}/bin/mps-constants set shm_file "/mpsserver" 
${TOP_DIR}/bin/mps-constants set www_dir "${TOP_DIR}/mpservice/www"
${TOP_DIR}/bin/mps-constants set cgi_dir "${TOP_DIR}/mpservice/cgi-bin"

#
# Create a boa.conf with some settings configured from the 
# mpservice_configuration.xml and other settings determined from the 
# installation location.
#

WEBSERVER_PORT=`${TOP_DIR}/bin/mps-config get -t webserver_port`
USER_NAME=`whoami`
GROUP_NAME=`groups | awk '{ print $1 }'`
HOST_NAME=`hostname`

cat > ${TOP_DIR}/misc/boa.conf <<EOF
# Boa v0.94 configuration file
# File format has not changed from 0.93
# File format has changed little from 0.92
# version changes are noted in the comments
#
# The Boa configuration file is parsed with a lex/yacc or flex/bison
# generated parser.  If it reports an error, the line number will be
# provided; it should be easy to spot.  The syntax of each of these
# rules is very simple, and they can occur in any order.  Where possible
# these directives mimic those of NCSA httpd 1.3; I saw no reason to 
# introduce gratuitous differences.

# $Id: boa.conf,v 1.25 2002/03/22 04:33:09 jnelson Exp $

# The "ServerRoot" is not in this configuration file.  It can be compiled
# into the server (see defines.h) or specified on the command line with
# the -c option, for example:
#
# boa -c /usr/local/boa


# Port: The port Boa runs on.  The default port for http servers is 80.
# If it is less than 1024, the server must be started as root.

Port ${WEBSERVER_PORT}

# Listen: the Internet address to bind(2) to.  If you leave it out,
# it takes the behavior before 0.93.17.2, which is to bind to all
# addresses (INADDR_ANY).  You only get one "Listen" directive,
# if you want service on multiple IP addresses, you have three choices:
#    1. Run boa without a "Listen" directive
#       a. All addresses are treated the same; makes sense if the addresses
#          are localhost, ppp, and eth0.
#       b. Use the VirtualHost directive below to point requests to different
#          files.  Should be good for a very large number of addresses (web
#          hosting clients).
#    2. Run one copy of boa per IP address, each has its own configuration
#       with a "Listen" directive.  No big deal up to a few tens of addresses.
#       Nice separation between clients.
# The name you provide gets run through inet_aton(3), so you have to use dotted
# quad notation.  This configuration is too important to trust some DNS.

#Listen 172.16.187.153

#  User: The name or UID the server should run as.
# Group: The group name or GID the server should run as.

User ${USER_NAME}
Group ${GROUP_NAME}

# ServerAdmin: The email address where server problems should be sent.
# Note: this is not currently used, except as an environment variable
# for CGIs.

#ServerAdmin root@localhost

# ErrorLog: The location of the error log file. If this does not start
# with /, it is considered relative to the server root.
# Set to /dev/null if you don't want errors logged.
# If unset, defaults to /dev/stderr

ErrorLog ${TOP_DIR}/misc/boa_error_log

# AccessLog: The location of the access log file. If this does not
# start with /, it is considered relative to the server root.
# Comment out or set to /dev/null (less effective) to disable 
# Access logging.

#AccessLog /var/log/boa_access_log

# UseLocaltime: Logical switch.  Uncomment to use localtime 
# instead of UTC time
#UseLocaltime

# VerboseCGILogs: this is just a logical switch.
#  It simply notes the start and stop times of cgis in the error log
# Comment out to disable.
#VerboseCGILogs

# ServerName: the name of this server that should be sent back to 
# clients if different than that returned by gethostname + gethostbyname 

ServerName ${HOST_NAME}

# VirtualHost: a logical switch.
# Comment out to disable.
# Given DocumentRoot /var/www, requests on interface 'A' or IP 'IP-A'
# become /var/www/IP-A.
# Example: http://localhost/ becomes /var/www/127.0.0.1
#
# Not used until version 0.93.17.2.  This "feature" also breaks commonlog
# output rules, it prepends the interface number to each access_log line.
# You are expected to fix that problem with a postprocessing script.

#VirtualHost 

# DocumentRoot: The root directory of the HTML documents.
# Comment out to disable server non user files.

DocumentRoot ${TOP_DIR}/www

# UserDir: The name of the directory which is appended onto a user's home
# directory if a ~user request is recieved.

#UserDir public_html

# DirectoryIndex: Name of the file to use as a pre-written HTML
# directory index.  Please MAKE AND USE THESE FILES.  On the
# fly creation of directory indexes can be _slow_.
# Comment out to always use DirectoryMaker

DirectoryIndex index.html

# DirectoryMaker: Name of program used to create a directory listing.
# Comment out to disable directory listings.  If both this and
# DirectoryIndex are commented out, accessing a directory will give
# an error (though accessing files in the directory are still ok).

#DirectoryMaker /usr/lib/boa/boa_indexer

# DirectoryCache: If DirectoryIndex doesn't exist, and DirectoryMaker
# has been commented out, the the on-the-fly indexing of Boa can be used
# to generate indexes of directories. Be warned that the output is 
# extremely minimal and can cause delays when slow disks are used.
# Note: The DirectoryCache must be writable by the same user/group that 
# Boa runs as.

# DirectoryCache /var/spool/boa/dircache

# KeepAliveMax: Number of KeepAlive requests to allow per connection
# Comment out, or set to 0 to disable keepalive processing

KeepAliveMax 1000

# KeepAliveTimeout: seconds to wait before keepalive connection times out

KeepAliveTimeout 10

# MimeTypes: This is the file that is used to generate mime type pairs
# and Content-Type fields for boa.
# Set to /dev/null if you do not want to load a mime types file.
# Do *not* comment out (better use AddType!)

MimeTypes /dev/null

# DefaultType: MIME type used if the file extension is unknown, or there
# is no file extension.

DefaultType text/html

# CGIPath: The value of the $PATH environment variable given to CGI progs.

CGIPath ${TOP_DIR}/cgi-bin/

# SinglePostLimit: The maximum allowable number of bytes in 
# a single POST.  Default is normally 1MB.

# AddType: adds types without editing mime.types
# Example: AddType type extension [extension ...]

# Uncomment the next line if you want .cgi files to execute from anywhere
AddType application/x-httpd-cgi cgi
AddType application/x-javascript js
AddType image/png png

# Redirect, Alias, and ScriptAlias all have the same semantics -- they
# match the beginning of a request and take appropriate action.  Use
# Redirect for other servers, Alias for the same server, and ScriptAlias
# to enable directories for script execution.

# Redirect allows you to tell clients about documents which used to exist in
# your server's namespace, but do not anymore. This allows you to tell the
# clients where to look for the relocated document.
# Example: Redirect /bar http://elsewhere/feh/bar

# Aliases: Aliases one path to another.
# Example: Alias /path1/bar /path2/foo

#Alias /doc /usr/doc

# ScriptAlias: Maps a virtual path to a directory for serving scripts
# Example: ScriptAlias /htbin/ /www/htbin/

ScriptAlias /cgi-bin/ ${TOP_DIR}/cgi-bin/

EOF

cat > ${TOP_DIR}/misc/mdns.conf <<EOF
mpservice audio player (${HOST_NAME})
_http._tcp.
${WEBSERVER_PORT}
EOF

cat > ${TOP_DIR}/startup.sh <<EOF
#!/bin/bash

. ${TOP_DIR}/env.sh

MEDIA_DIR=\`${TOP_DIR}/bin/mps-config get -t media_dir__0\`
if [ ! -e \${MEDIA_DIR} ]; then
  echo "Please set \"media_dir__0\" in mpservice_configuration.xml to a valid directory!"
  echo
  echo "exiting!"
  exit 1
fi 

echo "Starting mps-server"
${TOP_DIR}/bin/mps-server >> ${TOP_DIR}/misc/mps-server.output 2>&1 &
MPS_SERVER_PID=\$!

echo "Starting mps-librarydbd"
${TOP_DIR}/bin/mps-librarydbd >> ${TOP_DIR}/misc/mps-librarydbd.output 2>&1 &
MPS_LIBRARYDBD_PID=\$!

echo "Starting boa"
${TOP_DIR}/bin/boa -c ${TOP_DIR}/misc
BOA_PID=\$!

#echo "Starting mDNSResponder"
#${TOP_DIR}/bin/mDNSResponderPosix -f ${TOP_DIR}/misc/mdns.conf &
#MDNS_PID=\$!

on_die()
{
    kill \${MPS_SERVER_PID} \${MPS_LIBRARYDBD_PID} \${BOA_PID} #\${MDNS_PID}
    rm -f /dev/shm/mpsserver /tmp/live-library.db /tmp/live-playlist.db
    rm -f 
    exit 0
}

trap 'on_die' INT
trap 'on_die' TERM
trap 'on_die' QUIT

while [ 1 ]
do
  sleep 100
done

EOF
chmod 755 startup.sh

echo "mpservice setup is now complete."
echo 
echo "You may now run mpservice by invoking:"
echo "  ${TOP_DIR}/startup.sh"
echo 
echo "You may access the mpservice web interface locally at:"
echo "  http://localhost:${WEBSERVER_PORT}"
echo
