# Makefile for mps-shmtool
#
include ../../makefile.inc

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
STRIP=$(CROSS_COMPILE)strip

SOURCES=mps-shmtool.c
OBJECTS=$(SOURCES:.c=.o)

HEADERS= 

LIBS= -lmpsserver -lmxml -lpthread -lrt -lsqlite3

LDFLAGS +=$(MPSERVICE_LIB_FLAGS)
CFLAGS +=$(MPSERVICE_INCLUDE_FLAGS) -DDEBUG_ENABLED

all: mps-shmtool

# generic rule to build a .o from any .c
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# build the mps-shmtool executable
mps-shmtool: mps-shmtool.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ mps-shmtool.o $(LIBS)

.PHONY: install
install: mps-shmtool
	cp mps-shmtool $(MPSERVICE_BIN_DIR)  
	$(STRIP) $(MPSERVICE_BIN_DIR)/mps-shmtool

.PHONY: clean
clean:
	rm -f mps-shmtool $(OBJECTS)
	rm -f *~

.PHONY: clean_all
clean_all: clean
