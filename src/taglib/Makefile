# Makefile for taglib package
#
include ../../makefile.inc

PKGNAME:=taglib
PKGTAR:=$(shell \ls $(MPSERVICE_THIRDPARTY)/$(PKGNAME)*gz)
PKGDIR:=$(shell \basename $(PKGTAR) | sed -e 's/.tar.gz//')

UNPACKAGED_FLAG=.unpackaged
CONFIGURED_FLAG=.configured
TAGLIB_BINARY=$(PKGDIR)/taglib/libtag.so

all: taglib

.PHONY: taglib
taglib: $(TAGLIB_BINARY)
$(TAGLIB_BINARY): $(CONFIGURED_FLAG)
	make -C $(PKGDIR)
	if [ "${MPSERVICE_BUILD_PLATFORM}" = "x86-macos-standalone" ]; then \
	  cp -R $(PKGDIR)/taglib/libtag*dylib* $(MPSERVICE_ROOT)/lib; \
	  cp -R $(PKGDIR)/bindings/c/libtag_c*dylib* $(MPSERVICE_ROOT)/lib; \
	else \
	  cp -d $(PKGDIR)/taglib/libtag.*so* $(MPSERVICE_ROOT)/lib; \
	  cp -d $(PKGDIR)/bindings/c/libtag_c*so* $(MPSERVICE_ROOT)/lib; \
	fi
	cp $(PKGDIR)/bindings/c/tag_c.h $(MPSERVICE_ROOT)/include

.PHONY: unpackage
unpackage: $(UNPACKAGED_FLAG)
$(UNPACKAGED_FLAG):
	tar -xzf $(PKGTAR)
	cp CMakeLists.txt $(PKGDIR)
	patch -p0 -b -V simple < tag_c.h.patch
	patch -p0 -b -V simple < tag_c.cpp.patch
	patch -p0 -b -V simple < bindings_c_CMakeLists.txt.patch 
	touch $(UNPACKAGED_FLAG)

.PHONY: configure
configure: $(CONFIGURED_FLAG)
$(CONFIGURED_FLAG): $(UNPACKAGED_FLAG)
	(cd ${PKGDIR}; cmake -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_BUILD_TYPE=MinSizeRel -DCMAKE_C_COMPILER="${CROSS_COMPILE}gcc" -DCMAKE_CXX_COMPILER="${CROSS_COMPILE}g++" .)
	touch $(CONFIGURED_FLAG)

.PHONY: install
install: $(TAGLIB_BINARY)
	cp -d $(TAGLIB_BINARY)* $(MPSERVICE_LIB_DIR)
	cp -d $(PKGDIR)/bindings/c/libtag_c.so* $(MPSERVICE_LIB_DIR)
	cp $(PKGDIR)/examples/tagreader $(MPSERVICE_BIN_DIR)
	cp $(PKGDIR)/examples/tagreader_c $(MPSERVICE_BIN_DIR) 
	$(STRIP) $(MPSERVICE_LIB_DIR)/libtag*

clean:
	@if [ -r $(PKGDIR) ]; then \
	  $(MAKE) -C $(PKGDIR)/src $@ ; \
	fi

clean_all:
	rm -rf $(PKGDIR) $(UNPACKAGED_FLAG) $(CONFIGURED_FLAG) *~

