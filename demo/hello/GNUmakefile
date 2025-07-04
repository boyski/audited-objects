.ONESHELL:

CC	:= gcc

.PHONY: all
all: hello.exe

hello.exe: hello.o goodbye.o
	$(CC) -o $@ $^

%.o: %.c
	$(CC) -c $<

.PHONY: clean
clean:
	-rm -f hello.exe *.o *.class

.PHONY: rt
rt: clean all
