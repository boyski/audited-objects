.PHONY: all test clean

all: test

test: lib/ldlab.so lib64/ldlab.so hello32 hello64
	@echo "=============================="
	LD_PRELOAD='$$LIB/ldlab.so' ./hello32
	@echo "=============================="
	LD_PRELOAD='lib/ldlab.so' ./hello32
	@echo "=============================="
	LD_PRELOAD='$$LIB/ldlab.so' ./hello64
	@echo "=============================="
	LD_PRELOAD='lib64/ldlab.so' ./hello64

lib/ldlab.so:
	mkdir -p $(@D) &&\
	$(CC) -m32 -o $@ -g -W -Wall -fpic -shared -D_GNU_SOURCE -D_REENTRANT -z defs -z interpose -z combreloc -z lazy ldlab.c -lm -ldl

lib64/ldlab.so:
	mkdir -p $(@D) &&\
	$(CC) -m64 -o $@ -g -W -Wall -fpic -shared -D_GNU_SOURCE -D_REENTRANT -z defs -z interpose -z combreloc -z lazy ldlab.c -lm -ldl

hello32: hello.c
	$(CC) -m32 -o $@ -g $<

hello64: hello.c
	$(CC) -m64 -o $@ -g $<

clean:
	$(RM) -r hello32 hello64 lib lib64
