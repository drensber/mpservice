# Makefile for mps commands
#
include ../../makefile.inc

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
STRIP=$(CROSS_COMPILE)strip

SOURCES=mps-player.c mps-library.c mps-playlist.c mps-command-shared.c  
OBJECTS=$(SOURCES:.c=.o)

HEADERS=mps-command-shared.h 

LIBS= -lmpg123 -lm -lmpsserver -lmxml -lpthread -lrt -lsqlite3

LDFLAGS +=$(MPSERVICE_LIB_FLAGS)
CFLAGS +=$(MPSERVICE_INCLUDE_FLAGS) -DDEBUG_ENABLED

EXES= mps-player mps-playlist mps-library
all: $(EXES)

# generic rule to build a .o from any .c
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# build the Mpservice specific utilities
mps-player: mps-player.o mps-command-shared.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ mps-player.o mps-command-shared.o $(LIBS)

mps-playlist: mps-playlist.o mps-command-shared.o 
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ mps-playlist.o mps-command-shared.o $(LIBS)

mps-library: mps-library.o mps-command-shared.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ mps-library.o mps-command-shared.o $(LIBS)

# Static link... more convenient for testing
#mps-library: mps-library.o mps-command-shared.o $(SERVERLIBOBJECTS)
#	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ mps-library.o mps-command-shared.o $(LIBS) $(SERVERLIBOBJECTS) 

.PHONY: install
install: $(EXES)
	cp $(EXES) $(MPSERVICE_BIN_DIR)
	cd $(MPSERVICE_BIN_DIR); $(STRIP) $(EXES)

.PHONY: clean
clean:
	rm -f $(EXES) $(OBJECTS)
	rm -f *~

.PHONY: clean_all
clean_all: clean
