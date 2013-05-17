# -*- Makefile -*-
#

TOP_DIR := $(shell cd .. && pwd)

CFLAGS = -O2 -flto -std=gnu99 -g -ggdb -I$(TOP_DIR)/build/install/opt/include \
	 -L$(TOP_DIR)/build

LIBS_a := $(TOP_DIR)/build/libmrss.a \
	$(TOP_DIR)/build/libnxml.a \
	$(TOP_DIR)/build/libtidy.a

LIBS := -lmrss -lnxml -ltidy -lcurl -lsqlite3 -ljson

OBJS := selfoss_mupdate.o \
	hash_md5_sha.o \
	sanitize.o \
	database.o \
	entities.o

all: selfoss_mupdate

selfoss_mupdate: $(OBJS) $(LIBS_a)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

# for sanitize we deed internal headers from tidy
sanitize.o: sanitize.c
	$(CC) $(CFLAGS) -I$(TOP_DIR)/dl/tidy-html5/src -c -o $@ $<

clean:
	rm *.o selfoss_mupdate

