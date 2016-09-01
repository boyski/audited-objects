.ONESHELL:

CC := gcc

.PHONY: all
all: hello.exe

hello.exe: hello.o
	$(CC) -o $@ $<

hello.o: hello.c
	$(CC) -c $<

.PHONY: clean
clean:
	-$(RM) hello.exe hello.o

.PHONY: rt
rt: clean all
