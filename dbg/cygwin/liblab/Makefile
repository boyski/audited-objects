.PHONY: all clean test

CC := gcc
CFLAGS := -g -Wall -W
#SHLIBFLAGS := -shared -z defs -z interpose -z combreloc -z ignore -z lazyload
SHLIBFLAGS := -shared

all: liblab.so main.exe

liblab.so: liblab.c
	$(CC) $(CFLAGS) $(SHLIBFLAGS) $< -o $@

main.exe: main.c
	$(CC) $(CFLAGS) -o $@ $<

test: all
	LD_PRELOAD=./liblab.so ./main.exe

clean:
	-rm -f *.o *.so *.exe *.dll core
