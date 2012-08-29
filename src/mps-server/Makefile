# Makefile for mps-server
#
include ../../makefile.inc

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
STRIP=$(CROSS_COMPILE)strip

MPG123_OBJ_DIR=mpg123/src

SOURCES=mps-server.c  
OBJECTS=$(SOURCES:.c=.o)

SERVERLIBSOURCES=libmpsserver-player.c libmpsserver-playlist.c libmpsserver-library.c \
		 libmpsserver-config.c libmpsserver-shared.c

SERVERLIBOBJECTS=$(SERVERLIBSOURCES:.c=.o)

MPG123_OBJECTS=$(MPG123_OBJ_DIR)/audio.o $(MPG123_OBJ_DIR)/buffer.o $(MPG123_OBJ_DIR)/common.o \
	$(MPG123_OBJ_DIR)/compat.o $(MPG123_OBJ_DIR)/control_generic.o $(MPG123_OBJ_DIR)/equalizer.o \
	$(MPG123_OBJ_DIR)/getlopt.o $(MPG123_OBJ_DIR)/httpget.o $(MPG123_OBJ_DIR)/resolver.o \
	$(MPG123_OBJ_DIR)/genre.o $(MPG123_OBJ_DIR)/metaprint.o \
	$(MPG123_OBJ_DIR)/local.o $(MPG123_OBJ_DIR)/playlist.o $(MPG123_OBJ_DIR)/term.o \
	$(MPG123_OBJ_DIR)/wav.o $(MPG123_OBJ_DIR)/xfermem.o $(MPG123_OBJ_DIR)/legacy_module.o

ifeq ($(SOUND_ARCH),alsa)
	MPG123_OBJECTS+=$(MPG123_OBJ_DIR)/output/alsa.o
else
	MPG123_OBJECTS+=$(MPG123_OBJ_DIR)/output/oss.o
endif

HEADERS=libmpsserver.h libmpsserver-private.h 

MPGS_LIBS= -lmpg123 -lm -lmpsserver -lrt -lsqlite3 -lmxml -lpthread

ifeq ($(SOUND_ARCH),alsa)
	MPGS_LIBS+=-lasound
endif

MPGS_LDFLAGS= -L. $(MPSERVICE_LIB_FLAGS)
MPGS_CFLAGS= -I$(MPSERVICE_INCLUDE)/libmpg123 $(MPSERVICE_INCLUDE_FLAGS) -g -DDEBUG_ENABLED

all: mps-server

# generic rule to build a .o from any .c
%.o: %.c $(HEADERS)
	$(CC) $(MPGS_CFLAGS) -c $< -o $@

# libmpsserver
# "server-lib" files don't use the generic rule, since they must be PIC for the SO
libmpsserver-player.o: libmpsserver-player.c $(HEADERS)
	$(CC) -Wall -fPIC $(MPGS_CFLAGS) -c $< -o $@

libmpsserver-playlist.o: libmpsserver-playlist.c $(HEADERS)
	$(CC) -Wall -fPIC $(MPGS_CFLAGS) -c $< -o $@

libmpsserver-library.o: libmpsserver-library.c $(HEADERS)
	$(CC) -Wall -fPIC $(MPGS_CFLAGS) -c $< -o $@

libmpsserver.so: $(SERVERLIBOBJECTS)
	$(CC) -shared -W1,-soname,libmpsserver.so.1 -o libmpsserver.so.1.0 $(SERVERLIBOBJECTS) 
	ln -sf libmpsserver.so.1.0 libmpsserver.so
	ln -sf libmpsserver.so.1.0 libmpsserver.so.1
	cp -d libmpsserver.*o* $(MPSERVICE_ROOT)/lib
	cp -f libmpsserver.h $(MPSERVICE_ROOT)/include
	cp -f libmpsserver-private.h $(MPSERVICE_ROOT)/include

# build the generic mpg123+mpg123lib code
.PHONY: mpg123
mpg123: .mpg123
.mpg123:
	./install-mpg123.sh
	make -C mpg123
	if [ "${MPSERVICE_BUILD_PLATFORM}" == "x86-macos-standalone" ]; then \
	  cp -R ./mpg123/src/libmpg123/.libs/libmpg123*dylib* $(MPSERVICE_ROOT)/lib; \
	else \
	  cp -d ./mpg123/src/libmpg123/.libs/libmpg123*.so* $(MPSERVICE_ROOT)/lib; \
	fi
	[ -d $(MPSERVICE_ROOT)/include/libmpg123 ] || mkdir $(MPSERVICE_ROOT)/include/libmpg123 
	cp -f ./mpg123/src/libmpg123/*.h $(MPSERVICE_ROOT)/include/libmpg123
	cp -f ./mpg123/src/*.h $(MPSERVICE_ROOT)/include/libmpg123
	touch .mpg123

# build the Mpservice specific utilities
mps-server: .mpg123 mps-server.o libmpsserver.so
	$(CC) $(MPGS_CFLAGS) $(MPGS_LDFLAGS) -o $@ $(MPG123_OBJECTS) mps-server.o $(MPGS_LIBS)

.PHONY: install
install: mps-server
	cp mps-server $(MPSERVICE_BIN_DIR)
	cp -d libmpsserver.*o* $(MPSERVICE_LIB_DIR)
	cp -d mpg123/src/libmpg123/.libs/libmpg123.*so* $(MPSERVICE_LIB_DIR)
	$(STRIP) $(MPSERVICE_LIB_DIR)/libmpg123.so.0.22.1 $(MPSERVICE_LIB_DIR)/libmpsserver.so.1.0 \
		$(MPSERVICE_BIN_DIR)/mps-server
	#cp mplayer/mplayer $(MPSERVICE_BIN_DIR)
	#$(STRIP) $(MPSERVICE_BIN_DIR)/mplayer 

.PHONY: clean
clean:
	rm -f mps-server libmpsserver.so* $(OBJECTS) $(SERVERLIBOBJECTS)
	rm -f *~

.PHONY: clean_all
clean_all: clean
	rm -rf mpg123-1.10.0 mpg123 $(MPSERVICE_ROOT)/include/libmpg123 $(MPSERVICE_ROOT)/include/libmpsserver*.h \
	 $(MPSERVICE_ROOT)/lib/libmpg123.*o* $(MPSERVICE_ROOT)/lib/libmpsserver.*o* .mpg123 
