# Makefile for mDNSResponder
#
include ../../makefile.inc

BUILT_FLAG=.mdns-responder

all: $(BUILT_FLAG)

ifeq ($(CC),gcc)
  TARGET_OS=linux
else
  TARGET_OS=uclinux
endif

.PHONY: mdns-responder
mdns-responder: $(BUILT_FLAG)
$(BUILT_FLAG):
	./install-mdns-responder.sh
	make os=$(TARGET_OS) -C mDNSResponder/mDNSPosix SAResponder
	touch $(BUILT_FLAG)

.PHONY: install
install: mdns-responder
	cp mDNSResponder/mDNSPosix/build/prod/mDNSResponderPosix \
		$(MPSERVICE_BIN_DIR)
	$(STRIP) $(MPSERVICE_BIN_DIR)/mDNSResponderPosix

.PHONY: clean
clean:

clean_all:
	rm -rf `readlink mDNSResponder` mDNSResponder $(BUILT_FLAG) 

