.PHONY: all clean fi rt test test64 t

CC	 := gcc
COMMON	 := -g -W -Wall -fPIC -D_REENTRANT -D_LARGEFILE_SOURCE -I../../src -I../../OPS/include
COMMON	 += -Wcast-align -Wshadow -Wpointer-arith
COMMON	 += -DTRIO_REPLACE_STDIO
CFLAGS32 := -m32 -DLIB=\"./lib32.so\" -L../../OPS/SunOS_i386/lib $(COMMON)
CFLAGS64 := -m64 -DLIB=\"./lib64.so\" -L../../OPS/SunOS_i386/lib/64 $(COMMON) -D_LARGEFILE64_SOURCE

SHLIBFLAGS := -shared -nodefaultlibs -Bdirect -z interpose -z ignore
SHLIBFLAGS += -z defs -z text -z combreloc -z lazyload
SHLIBS     := -lgcc -lsocket -lnsl -lm -ldl -lrt -lc
SHLIBS32   := -L../../OPS/SunOS_i386/lib/32 -Wl,-Bstatic -lkaz -lpcre -lz -ltrio -lsha2 -Wl,-Bdynamic $(SHLIBS)
SHLIBS64   := -L../../OPS/SunOS_i386/lib/64 -Wl,-Bstatic -lkaz -lpcre -lz -ltrio -lsha2 -Wl,-Bdynamic $(SHLIBS)

all: lib32.so lab32
all: lib64.so lab64

lib32.so: lib.c
	$(CC) -o $@ $< $(CFLAGS32) $(SHLIBFLAGS) $(SHLIBS32)

lib64.so: lib.c
	$(CC) -o $@ $< $(CFLAGS64) $(SHLIBFLAGS) $(SHLIBS64)

lab32: lab.c
	$(CC) $(CFLAGS32) -o $@ $< -ldl

lab64: lab.c
	$(CC) $(CFLAGS64) -o $@ $< -ldl

clean:
	-rm -f *.o *.so core lab32 lab64

test: all
	@echo ======================================
	./lab32
	@echo ======================================
	LD_PRELOAD=./lib32.so ./lab32
	@echo ======================================

test64: all
	@echo ======================================
	./lab64
	@echo ======================================
	LD_PRELOAD=./lib64.so ./lab64
	@echo ======================================

t: rt
	ldd lib32.so
	LD_PRELOAD_32=./lib32.so ../topen32 /etc/motd
	ldd lib64.so
	LD_PRELOAD_64=./lib64.so ../topen64 /etc/motd

fi rt: clean all
