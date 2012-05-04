# Makefile for mini-xml
#
include ../../makefile.inc

BUILT_FLAG=.mini-xml

all:	$(BUILT_FLAG)

.PHONY: mini-xml
mini-xml: $(BUILT_FLAG)
$(BUILT_FLAG):
	./install-mini-xml.sh
	make -C mxml
	if [ "${MPSERVICE_BUILD_PLATFORM}" = "x86-macos-standalone" ]; then \
	  cp -R mxml/libmxml*dylib* $(MPSERVICE_ROOT)/lib; \
	else \
	  cp -d mxml/libmxml.*so* $(MPSERVICE_ROOT)/lib; \
	fi
	cp mxml/mxml.h $(MPSERVICE_ROOT)/include
	touch $(BUILT_FLAG)

.PHONY: install
install: $(BUILT_FLAG)
	if [ "${MPSERVICE_BUILD_PLATFORM}" = "x86-macos-standalone" ]; then \
	  cp -R mxml/libmxml*dylib* $(MPSERVICE_LIB_DIR); \
	  $(STRIP) $(MPSERVICE_LIB_DIR)/libmxml.1.dylib; \
	else \
	  cp -d mxml/libmxml.*so* $(MPSERVICE_LIB_DIR); \
	  $(STRIP) $(MPSERVICE_LIB_DIR)/libmxml.so.1.5; \
	fi 


.PHONY: clean
clean:

clean_all: clean
	rm -rf `readlink mxml` mxml $(BUILT_FLAG) \
		$(MPSERVICE_ROOT)/lib/libmxml* \
		$(MPSERVICE_ROOT)/include/mxml.h
