# Makefile for mps-librarydbd
#
include ../../makefile.inc

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
STRIP=$(CROSS_COMPILE)strip

SOURCES=mps-librarydbd.c mps-librarydbd-traverse.c
OBJECTS=$(SOURCES:.c=.o)

HEADERS=mps-librarydbd.h

LIBS= -lmpsserver -lmxml -lpthread -lsqlite3 -lrt -ltag -ltag_c

LDFLAGS +=$(MPSERVICE_LIB_FLAGS)
#CFLAGS +=$(MPSERVICE_INCLUDE_FLAGS) -g -DDEBUG_ENABLED -DMPG123_NO_CONFIGURE
CFLAGS +=$(MPSERVICE_INCLUDE_FLAGS) -DMPG123_NO_CONFIGURE

all: mps-librarydbd

# generic rule to build a .o from any .c
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

mps-librarydbd: $(OBJECTS) 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJECTS) $(LIBS)

learn: learnlibmpg123.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ learnlibmpg123.o -lmpg123

.PHONY: install
install: mps-librarydbd
	cp mps-librarydbd $(MPSERVICE_BIN_DIR)
	$(STRIP) $(MPSERVICE_BIN_DIR)/mps-librarydbd

.PHONY: clean
clean:
	rm -f mps-librarydbd $(OBJECTS)
	rm -f *~

.PHONY: clean_all
clean_all: clean
