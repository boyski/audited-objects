.PHONY: all clean fi 

CC	:= gcc
CFLAGS := -m32 -g -W -Wall

all: atexit

atexit: atexit.c
	$(CC) $(CFLAGS) -o $@ $< -ldl

clean:
	-rm -f *.o *.so core atexit

fi: clean all
