## Requires GNU Make.

.PHONY: all clean rt t

CC	 := gcc
COMMON	 := -g -W -Wall -fPIC -D_GNU_SOURCE
CFLAGS32 := -m32 $(COMMON)
CFLAGS64 := -m64 $(COMMON)

SHLIBFLAGS := -shared -nodefaultlibs -z interpose -z ignore
#SHLIBFLAGS += -z defs -z text -z combreloc -z lazyload
SHLIBS     := -lgcc -lsocket -lnsl -lm -ldl -lrt -lc
SHLIBS32   := $(SHLIBS)
SHLIBS64   := $(SHLIBS)

all: bad64.so topen64 bad32.so topen32 good64.so

%32.so: %.c
	$(CC) -o $@ $< $(CFLAGS32) $(SHLIBFLAGS) $(SHLIBS32)

%64.so: %.c
	$(CC) -o $@ $< $(CFLAGS64) $(SHLIBFLAGS) $(SHLIBS64)

topen32: topen.c
	$(CC) $(CFLAGS32) -o $@ $<

topen64: topen.c
	$(CC) $(CFLAGS64) -o $@ $<

clean:
	-rm -f *.o *.so core topen32 topen64

rt: clean all

t: rt
	ldd bad32.so
	LD_PRELOAD_32=./bad32.so ./topen32 /etc/motd
	ldd good64.so
	LD_PRELOAD_64=./good64.so ./topen64 /etc/motd
	ldd bad64.so
	LD_PRELOAD_64=./bad64.so ./topen64 /etc/motd
