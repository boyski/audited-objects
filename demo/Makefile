.PHONY: all release clean

TAR		:= gtar --owner=0 --group=0 --mode=u+rw --dereference

all: release

clean:
	cd hello && $(MAKE) clean

release: clean
	cd .. &&\
	    rm -f demo.tar.gz &&\
	    cleartool ls -vob -r -s -nxn demo/hello |\
	    egrep -v 'hello/VC|hello/ide\.' |\
	    $(TAR) --files-from - --no-recursion -czf demo.tar.gz
