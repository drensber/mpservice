# Makefile for boa package
#
include ../../makefile.inc

PKGNAME:=boa
PKGTAR:=$(shell \ls $(MPSERVICE_THIRDPARTY)/$(PKGNAME)*gz)
PKGDIR:=$(shell \basename $(PKGTAR) | sed -e 's/.tar.gz//')
EXENAME_BOA:=boa

UNPACKAGED_FLAG=.unpackaged
CONFIGURED_FLAG=.configured
BOA_BINARY=$(PKGDIR)/src/boa

all: boa

.PHONY: boa
boa: $(BOA_BINARY)
$(BOA_BINARY): $(CONFIGURED_FLAG)
	make -C $(PKGDIR)/src

.PHONY: unpackage
unpackage: $(UNPACKAGED_FLAG)
$(UNPACKAGED_FLAG):
	tar -xzf $(PKGTAR)
	patch -p0 -b -V simple < compat.h.patch
	patch -p0 -b -V simple < no_drop_privs.patch
	cp configure.new $(PKGDIR)/src/configure 
	touch $(UNPACKAGED_FLAG)

.PHONY: configure
configure: $(CONFIGURED_FLAG)
$(CONFIGURED_FLAG): $(UNPACKAGED_FLAG)
	if [ "${CC}" = "gcc" ]; then \
	  cd ${PKGDIR}/src; ./configure; \
	else \
	  cd ${PKGDIR}/src; CFLAGS="-g -O2 -pipe -Wall" CC=${CROSS_COMPILE}gcc ./configure --host=${ARCH}-linux; \
	fi
	touch $(CONFIGURED_FLAG)

.PHONY: install
install: $(BOA_BINARY)
	cp boa.conf $(MPSERVICE_MISC_DIR)
	cp $(BOA_BINARY) $(MPSERVICE_BIN_DIR)
	$(STRIP) $(MPSERVICE_BIN_DIR)/boa

clean:
	@if [ -r $(PKGDIR) ]; then \
	  $(MAKE) -C $(PKGDIR)/src $@ ; \
	fi

clean_all:
	rm -rf $(PKGDIR) $(UNPACKAGED_FLAG) $(CONFIGURED_FLAG) *~

