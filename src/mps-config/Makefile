# Makefile for mps-config
#
include ../../makefile.inc

CC=$(CROSS_COMPILE)gcc
CXX=$(CROSS_COMPILE)g++
STRIP=$(CROSS_COMPILE)strip

SOURCES=mps-config.c
OBJECTS=$(SOURCES:.c=.o)

HEADERS=mps-config.h

LIBS= -lmpsserver -lmxml -lpthread -lrt -lsqlite3

LDFLAGS +=$(MPSERVICE_LIB_FLAGS)
CFLAGS +=$(MPSERVICE_INCLUDE_FLAGS) -DDEBUG_ENABLED

all: mps-config

# generic rule to build a .o from any .c
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

mps-config: mps-config.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ mps-config.o $(LIBS)

.PHONY: install
install: mps-config
	cp mps-config $(MPSERVICE_BIN_DIR)
	ln -sf mps-config $(MPSERVICE_BIN_DIR)/mps-constants
	$(STRIP) $(MPSERVICE_BIN_DIR)/mps-config
	cp mpservice_shell_config.sh $(MPSERVICE_BIN_DIR)
	cp mpbox_default_configuration.xml $(MPSERVICE_MISC_DIR)
	cp standalone_default_configuration.xml $(MPSERVICE_MISC_DIR)
	cp mpservice_constants.xml $(MPSERVICE_MISC_DIR)

.PHONY: clean
clean:
	rm -f mps-config $(OBJECTS)
	rm -f *~

.PHONY: clean_all
clean_all: clean
