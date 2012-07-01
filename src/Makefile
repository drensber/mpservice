# Makefile for all the stuff in mpservice/src
#
include ../makefile.inc

LIB_DIRS=mini-xml mps-server taglib
EXE_DIRS=boa mDNSResponder mps-commands \
	mps-librarydbd mps-shmtool mps-config
ALL_DIRS=$(LIB_DIRS) $(EXE_DIRS)

all:
	make libraries
	make executables

.PHONY: libraries
libraries:
	@for DIR in $(LIB_DIRS) ; do \
	make -C $$DIR ; done

.PHONY: executables
executables:
	@for DIR in $(EXE_DIRS) ; do \
	make -C $$DIR ; done

.PHONY: install
install:
	@for DIR in $(ALL_DIRS) ; do \
	make -C $$DIR install; done

clean:
	@for DIR in $(ALL_DIRS) ; do \
	make -C $$DIR clean ; done

clean_all:
	@for DIR in $(ALL_DIRS) ; do \
	make -C $$DIR clean_all ; done

