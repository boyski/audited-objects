.PHONY: all

.ONESHELL:

all: include

include:
	ln -sf readline $@

lib/libfoo.so:
	cd lib && gcc -W -fpic -shared -o libfoo.so.1 ../libfoo.c &&\
	rm -f libfoo.so && ln -s libfoo.so.1 libfoo.so

progfoo: lib/libfoo.so
	gcc -o $@ -g -W -Wall progfoo.c -L./lib -lfoo

clean: ; rm -f include progfoo lib/libfoo*
