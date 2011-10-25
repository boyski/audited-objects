.PHONY: all clean

ALL	:= largefile linkops

all: $(ALL)

.PHONY: largefile
largefile:
	perl -w largefile.pl 31 4

.PHONY: linkops
linkops:
	perl -w linkops.pl

clean:
	rm -f *.a *.o *.X core
