# -*- Makefile -*-
#

TOP_DIR := $(shell cd .. && pwd)

CFLAGS = -std=gnu99 -I$(TOP_DIR)/build/install/opt/include \
	 -L$(TOP_DIR)/build

LIBS = $(TOP_DIR)/build/libmrss.a \
       $(TOP_DIR)/build/libnxml.a

selfoss_mupdate: selfoss_mupdate.o $(LIBS)
	$(CC) $(CFLAGS) -o $@ selfoss_mupdate.o -lmrss -lnxml -lcurl -ltidy

clean:
	rm *.o selfoss_mupdate
