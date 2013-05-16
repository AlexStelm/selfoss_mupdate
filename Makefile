# -*- Makefile -*-
#

NXML_VER := "0.18.3"
NXML_URL := "http://www.autistici.org/bakunin/libnxml/libnxml-$(NXML_VER).tar.gz"

MRSS_VER := "0.19.2"
MRSS_URL := "http://www.autistici.org/bakunin/libmrss/libmrss-$(MRSS_VER).tar.gz"

TOP_DIR := "$(shell pwd)"
LIBINST_DIR := "$(TOP_DIR)/build/install"

all: build/libnxml.a build/libmrss.a build/libtidy.a src/selfoss_mupdater

dl/libnxml-$(NXML_VER).tar.gz:
	cd $(TOP_DIR)/dl && wget -c $(NXML_URL)

dl/libmrss-$(MRSS_VER).tar.gz:
	cd $(TOP_DIR)/dl && wget -c $(MRSS_URL)

#dl/tidy-html5:
#	git clone git://github.com/w3c/tidy-html5.git $@

build/%: dl/%.tar.gz
	cd $(TOP_DIR)/build && tar xf $(TOP_DIR)/$<

build/install/opt/lib/libnxml.a: build/libnxml-$(NXML_VER)
	cd $(TOP_DIR)/build/libnxml-$(NXML_VER) && ./configure --prefix=/opt
	make -C $(TOP_DIR)/build/libnxml-$(NXML_VER)
	make -C $(TOP_DIR)/build/libnxml-$(NXML_VER) install DESTDIR=$(LIBINST_DIR)
	sed "s#prefix=/opt#prefix=$(LIBINST_DIR)/opt#" -i \
		$(LIBINST_DIR)/opt/lib/pkgconfig/nxml.pc

build/install/opt/lib/libmrss.a: build/libmrss-$(MRSS_VER)
	cd $(TOP_DIR)/build/libmrss-$(MRSS_VER) && \
		PKG_CONFIG_PATH=$(LIBINST_DIR)/opt/lib/pkgconfig \
		./configure --prefix=/opt
	make -C $(TOP_DIR)/build/libmrss-$(MRSS_VER)
	make -C $(TOP_DIR)/build/libmrss-$(MRSS_VER) install DESTDIR=$(LIBINST_DIR)

build/libtidy:
	cp -r $(TOP_DIR)/dl/tidy-html5 $(TOP_DIR)/build/libtidy

build/install/opt/lib/libtidy.a: build/libtidy
	( cd $(TOP_DIR)/build/libtidy && sh build/gnuauto/setup.sh && ./configure --prefix=/opt )
	make -C $(TOP_DIR)/build/libtidy
	make -C $(TOP_DIR)/build/libtidy install DESTDIR=$(LIBINST_DIR)

#build/%.a: build/install/opt/lib/%.a
#	strip -o $@ $<

build/%.a: build/install/opt/lib/%.a
	cp $< $@

src/selfoss_mupdater:
	make -C $(TOP_DIR)/src

clean:
	rm -rf $(TOP_DIR)/build/lib*
	rm -rf $(TOP_DIR)/build/install/*
	make -C $(TOP_DIR)/src clean

