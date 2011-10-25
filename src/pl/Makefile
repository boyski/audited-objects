.PHONY: all clean rt

.ONESHELL:

# Windows/MSVC compatibility.
ifdef VSINSTALLDIR
include NMakefile
else	#VSINSTALLDIR

CC	:= gcc

all: hello.exe

hello.o: /usr/include/stdio.h

hello.exe: hello.o
	$(CC) -o $@ $<

hello.o: hello.c
	$(CC) -c $<

clean:
	-rm -f hello.exe hello.o Hello.class

rt: clean all

endif	#VSINSTALLDIR
