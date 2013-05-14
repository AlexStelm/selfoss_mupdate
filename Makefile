# -*- Makefile -*-
#

NXML_VER := "0.18.3"
NXML_URL := "http://www.autistici.org/bakunin/libnxml/libnxml-$(NXML_VER).tar.gz"

MRSS_VER := "0.19.2"
MRSS_URL := "http://www.autistici.org/bakunin/libmrss/libmrss-$(MRSS_VER).tar.gz"

TOP_DIR := "$(shell pwd)"

all: build/libnxml.a build/libmrss.a

dl/libnxml-$(NXML_VER).tar.gz:
	cd $(TOP_DIR)/dl && wget -c $(NXML_URL)

dl/libmrss-$(MRSS_VER).tar.gz:
	cd $(TOP_DIR)/dl && wget -c $(MRSS_URL)

build/%: dl/%.tar.gz
	cd $(TOP_DIR)/build && tar xf $(TOP_DIR)/$<

build/install/opt/lib/libnxml.a: build/libnxml-$(NXML_VER)
	cd $(TOP_DIR)/build/libnxml-$(NXML_VER) && ./configure --prefix=/opt
	make -C $(TOP_DIR)/build/libnxml-$(NXML_VER)
	make -C $(TOP_DIR)/build/libnxml-$(NXML_VER) install DESTDIR=$(TOP_DIR)/build/install
	sed "s#prefix=/opt#prefix=$(TOP_DIR)/build/install/opt#" -i \
		$(TOP_DIR)/build/install/opt/lib/pkgconfig/nxml.pc

build/install/opt/lib/libmrss.a: build/libmrss-$(MRSS_VER)
	cd $(TOP_DIR)/build/libmrss-$(MRSS_VER) && \
		PKG_CONFIG_PATH=$(TOP_DIR)/build/install/opt/lib/pkgconfig \
		./configure --prefix=/opt
	make -C $(TOP_DIR)/build/libmrss-$(MRSS_VER)
	make -C $(TOP_DIR)/build/libmrss-$(MRSS_VER) install DESTDIR=$(TOP_DIR)/build/install

#build/%.a: build/install/opt/lib/%.a
#	strip -o $@ $<

build/%.a: build/install/opt/lib/%.a
	cp $< $@

clean:
	rm -rf $(TOP_DIR)/build/lib*
	rm -rf $(TOP_DIR)/build/install/*

