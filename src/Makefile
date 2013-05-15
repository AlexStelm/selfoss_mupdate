# -*- Makefile -*-
#

TOP_DIR := $(shell cd .. && pwd)

CFLAGS = -std=gnu99 -I$(TOP_DIR)/build/install/opt/include \
	 -L$(TOP_DIR)/build

LIBS_a := $(TOP_DIR)/build/libmrss.a \
	$(TOP_DIR)/build/libnxml.a \
	$(TOP_DIR)/build/libtidy.a

LIBS := -lmrss -lnxml -lcurl -ltidy

OBJS := selfoss_mupdate.o \
	hash_md5_sha.o

selfoss_mupdate: $(OBJS) $(LIBS_a)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean:
	rm *.o selfoss_mupdate
