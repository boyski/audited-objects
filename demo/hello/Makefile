.ONESHELL:

CC := gcc

.PHONY: all
all: hello.exe

hello.exe: hello.o goodbye.o
	$(CC) -o $@ $^

%.o: %.c
	$(CC) -c $<

.PHONY: clean
clean: cleanups := $(wildcard hello.exe *.o *.d *.class)
clean:
	$(if $(cleanups),@set -x; $(RM) $(cleanups))

.PHONY: rt
rt: clean all

# vim: filetype=make sw=2 noet
