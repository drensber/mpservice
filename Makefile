# Top level mpservice makefile
MAKEINC_FILE=makefile.inc
THIRDPARTY_DOWNLOADED_FLAG=downloads/.downloaded
STANDALONE_BASE_NAME=mpservice_package
STANDALONE_STAGING_DIR=$(MPSERVICE_ROOT)/$(STANDALONE_BASE_NAME)
STANDALONE_WWW_DIR=$(STANDALONE_STAGING_DIR)/www
STANDALONE_CGI_DIR=$(STANDALONE_STAGING_DIR)/cgi-bin
STANDALONE_BIN_DIR=$(STANDALONE_STAGING_DIR)/bin
STANDALONE_LIB_DIR=$(STANDALONE_STAGING_DIR)/lib
STANDALONE_MISC_DIR=$(STANDALONE_STAGING_DIR)/misc
STANDALONE_PACKAGE=$(STANDALONE_BASE_NAME).tar.bz

all: mpservice

default: all

.PHONY: help
help:
	@echo
	@echo "Top level makefile for mpservice."
	@echo
	@echo "Usage:"
	@echo "  make help - Display this help screen."
	@echo "  make standalone - Make an package that can be run on a"
	@echo "                    regular linux PC."
	@echo "  make mpservice - Build all mpservice software (default)."
	@echo "  make download - Download 3rd party dependencies from net"
	@echo "  make download-local - Set up build system to use source"
	@echo "                        packages that have already been" 
	@echo "                        downloaded to the directory specified"
	@echo "                        by environment variable"
	@echo "                        THIRDPARTY_PACKAGE_DIR."
	@echo "  make clean-downloads - Clean the downloads directory"
	@echo "                         (effectively undoes 'make download'"
	@echo "                         or 'make download-local')."
	@echo "  make clean - "
	@echo "  make clean_all - "
	@echo

.PHONY: configure
configure: $(MAKEINC_FILE)
$(MAKEINC_FILE): 
	./configure.sh

.PHONY: download
download: $(THIRDPARTY_DOWNLOADED_FLAG)
$(THIRDPARTY_DOWNLOADED_FLAG): 
	./install_thirdparty.sh
	touch $(THIRDPARTY_DOWNLOADED_FLAG)

.PHONY: download-local
download-local:
	./install_thirdparty.sh local
	touch $(THIRDPARTY_DOWNLOADED_FLAG)

.PHONY: clean-downloads
clean-downloads:
	rm -f downloads/* $(THIRDPARTY_DOWNLOADED_FLAG)

.PHONY: standalone
standalone: $(STANDALONE_PACKAGE)
$(STANDALONE_PACKAGE): mpservice
	rm -rf $(STANDALONE_STAGING_DIR)
	[ -d $(STANDALONE_STAGING_DIR) ] || mkdir -p $(STANDALONE_STAGING_DIR)
	[ -d $(STANDALONE_WWW_DIR) ] || mkdir -p $(STANDALONE_WWW_DIR)
	[ -d $(STANDALONE_CGI_DIR) ] || mkdir -p $(STANDALONE_CGI_DIR)
	[ -d $(STANDALONE_BIN_DIR) ] || mkdir -p $(STANDALONE_BIN_DIR)
	[ -d $(STANDALONE_LIB_DIR) ] || mkdir -p $(STANDALONE_LIB_DIR)
	[ -d $(STANDALONE_MISC_DIR) ] || mkdir -p $(STANDALONE_MISC_DIR)
	make install \
		MPSERVICE_BIN_DIR=$(STANDALONE_BIN_DIR) \
		MPSERVICE_WWW_DIR=$(STANDALONE_WWW_DIR) \
		MPSERVICE_CGI_DIR=$(STANDALONE_CGI_DIR) \
		MPSERVICE_LIB_DIR=$(STANDALONE_LIB_DIR) \
		MPSERVICE_MISC_DIR=$(STANDALONE_MISC_DIR)
	cp $(STANDALONE_STAGING_DIR)/misc/standalone_default_configuration.xml \
		 $(STANDALONE_STAGING_DIR)/mpservice_configuration.xml
	cp standalone_specific/setup.sh $(STANDALONE_STAGING_DIR) 
	tar cjf $(STANDALONE_PACKAGE) $(STANDALONE_BASE_NAME) 

mpservice: $(MAKEINC_FILE) $(THIRDPARTY_DOWNLOADED_FLAG)
	make -C src

.PHONY: install
install: mpservice
	[ -d $(MPSERVICE_WWW_DIR) ] || mkdir -p $(MPSERVICE_WWW_DIR)
	[ -d $(MPSERVICE_CGI_DIR) ] || mkdir -p $(MPSERVICE_CGI_DIR)
	[ -d $(MPSERVICE_BIN_DIR) ] || mkdir -p $(MPSERVICE_BIN_DIR)
	[ -d $(MPSERVICE_LIB_DIR) ] || mkdir -p $(MPSERVICE_LIB_DIR)
	[ -d $(MPSERVICE_MISC_DIR) ] || mkdir -p $(MPSERVICE_MISC_DIR)
	ln -sf /mpservice/bin/mps-player $(MPSERVICE_CGI_DIR)/mps-player
	ln -sf /mpservice/bin/mps-playlist $(MPSERVICE_CGI_DIR)/mps-playlist
	ln -sf /mpservice/bin/mps-library $(MPSERVICE_CGI_DIR)/mps-library
	ln -sf /mpservice/bin/mps-config $(MPSERVICE_CGI_DIR)/mps-config
	ln -sf /mpservice/bin/mps-config $(MPSERVICE_CGI_DIR)/mps-constants
	make -C www install
	make -C src install

clean:
	touch $(MAKEINC_FILE)
	make -C src clean
	rm -f $(MAKEINC_FILE)
	rm -rf $(STANDALONE_STAGING_DIR) $(STANDALONE_PACKAGE)

clean_all:
	touch $(MAKEINC_FILE)
	make -C src clean_all
	rm -f $(MAKEINC_FILE)
	rm -rf $(STANDALONE_STAGING_DIR) $(STANDALONE_PACKAGE)
