.PHONY: all clean fi

CC	:= gcc
#CFLAGS	+= -m64

all: multilink

multilink: multilink.o
	$(CC) $(CFLAGS) -o $@ $<

multilink.o: multilink.c
	$(CC) $(CFLAGS) -c $<

clean:
	-rm -f multilink multilink.o*

fi: clean all
