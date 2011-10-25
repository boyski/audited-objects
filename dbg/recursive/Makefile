.PHONY: all clean SUBDIRS

all: foo.o SUBDIRS
	$(MAKE) -C A all-am;
	$(MAKE) -C B all-am;

foo.o: foo.c
	gcc -c foo.c

clean:
	rm -f foo.o core
	$(MAKE) -C A clean
	$(MAKE) -C B clean
