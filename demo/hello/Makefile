.PHONY: all clean rt

.ONESHELL:

# Windows/MSVC compatibility.
ifdef VSINSTALLDIR
include NMakefile
else	#VSINSTALLDIR

CC	:= gcc

all: hello.exe

hello.exe: hello.o goodbye.o
	$(CC) -o $@ $^

%.o: %.c
	$(CC) -c $<

clean:
	-rm -f hello.exe *.o *.class

rt: clean all

endif	#VSINSTALLDIR
