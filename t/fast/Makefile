.PHONY: all clean allclean new

SRCS	:= Fast.c $(wildcard [a-z]*.c)
OBJS	:= $(patsubst %.c,%.o, $(SRCS))

all: Fast

Fast: $(OBJS)
	$(CC) $(OBJS) -o $@

clean:
	rm -f Fast *.o

allclean: clean
	rm -f *.c

new: allclean
	perl Fast.pl
