#!/bin/sh
#
#
set -e

MPLAYER_BASE=MPlayer-1.1

if [ ! -e mplayer ]; then
    ln -s ${MPLAYER_BASE} mplayer
fi

if [ ! -d ${MPLAYER_BASE} ]; then
    tar xzf ../../downloads/${MPLAYER_BASE}.tar.gz
    cd ${MPLAYER_BASE}
        if [ "${MPSERVICE_BUILD_PLATFORM}" = "x86-uClibc-linux" ]; then
#            ./configure --cc=${CROSS_COMPILE}gcc --as=${CROSS_COMPILE}as --yasm= --target=i586-linux --charset=UTF-8 --extra-ldflags= --enable-cross-compile --enable-mad --enable-fbdev --disable-alsa --disable-big-endian --disable-sdl --enable-mplayer --disable-mencoder --disable-mad --disable-live --enable-cross-compile --disable-ivtv --enable-dynamic-plugins --disable-vm --disable-xf86keysym --disable-tv --disable-tv-v4l1 --disable-tv-v4l2 --disable-tv-bsdbt848 --disable-pvr --disable-winsock2_h --disable-smb --disable-vcd --disable-bluray --disable-dvdnav --disable-dvdread --disable-dvdread-internal --disable-libdvdcss-internal --disable-cdparanoia --disable-unrarexec --disable-sortsub --disable-fribidi --disable-enca --disable-maemo --disable-gethostbyname2 --disable-ftp --disable-vstream --disable-langinfo --disable-lirc --disable-lircc --disable-joystick --disable-apple-remote --disable-apple-ir --disable-radio --disable-radio-capture --disable-radio-v4l2 --disable-radio-bsdbt848 --disable-rtc --disable-networking --disable-nemesi --disable-librtmp --disable-cddb --disable-menu --disable-macosx-finder --disable-macosx-bundle --disable-ass-internal --disable-ass --disable-win32dll --disable-libcdio --disable-qtx --disable-xanim --disable-real --disable-xvid --disable-speex --disable-faac --disable-toolame --disable-twolame --disable-xmms --disable-x264 --disable-x264-lavc --disable-libdirac-lavc --disable-libschroedinger-lavc --disable-libvpx-lavc --disable-libgsm --disable-theora --disable-mp3lame --disable-mp3lame-lavc --disable-liba52 --disable-libmpeg2 --disable-libmpeg2-internal --disable-libopencore_amrnb --disable-libopencore_amrwb --disable-libopenjpeg --disable-crystalhd --disable-vidix --disable-vidix-pcidb --disable-matrixview --disable-xss --disable-tga --disable-pnm --disable-md5sum --disable-yuv4mpeg --disable-corevideo --disable-quartz
            ./configure --cc=${CROSS_COMPILE}gcc --as=${CROSS_COMPILE}as --yasm= --target=i586-linux --charset=UTF-8 --extra-ldflags= --enable-cross-compile --enable-mad --enable-fbdev --disable-alsa --disable-big-endian --disable-sdl --enable-mplayer --disable-mencoder --disable-mad --disable-live --enable-cross-compile --disable-ivtv --enable-dynamic-plugins --disable-vm --disable-xf86keysym --disable-tv --disable-tv-v4l1 --disable-tv-v4l2 --disable-tv-bsdbt848 --disable-pvr --disable-winsock2_h --disable-smb --disable-vcd --disable-bluray --disable-dvdnav --disable-dvdread --disable-dvdread-internal --disable-libdvdcss-internal --disable-cdparanoia --disable-unrarexec --disable-sortsub --disable-fribidi --disable-enca --disable-maemo --disable-gethostbyname2 --disable-ftp --disable-vstream --disable-langinfo --disable-lirc --disable-lircc --disable-joystick --disable-apple-remote --disable-apple-ir --disable-radio --disable-radio-capture --disable-radio-v4l2 --disable-radio-bsdbt848 --disable-rtc --disable-networking --disable-nemesi --disable-librtmp --disable-cddb --disable-menu --disable-macosx-finder --disable-macosx-bundle --disable-ass-internal --disable-ass --disable-win32dll --disable-libcdio --disable-qtx --disable-xanim --disable-real --disable-xvid --disable-speex --disable-faac --disable-toolame --disable-twolame --disable-xmms --disable-x264 --disable-x264-lavc --disable-libdirac-lavc --disable-libschroedinger-lavc --disable-libvpx-lavc --disable-libgsm --disable-theora --disable-mp3lame --disable-mp3lame-lavc --disable-liba52 --disable-libmpeg2 --disable-libmpeg2-internal --disable-libopencore_amrnb --disable-libopencore_amrwb --disable-libopenjpeg --disable-crystalhd --disable-vidix --disable-vidix-pcidb --disable-matrixview --disable-xss --disable-tga --disable-pnm --disable-md5sum --disable-yuv4mpeg --disable-corevideo --disable-quartz --disable-gui --disable-encoder=mp3 --disable-encoder=aac --disable-encoder=flac       
	elif [ "${MPSERVICE_BUILD_PLATFORM}" = "x86-macos-standalone" ]; then 
	    ./configure --with-cpu=x86-64
	else
	    ./configure
	fi
    cd -
fi
