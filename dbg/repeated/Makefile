.PHONY: all clean

all: repeated

repeated:
	echo LINE >> $@
	@cksum $@
	echo LINE >> $@
	@cksum $@
	echo LINE >> $@
	@cksum $@

clean:
	rm -f repeated
