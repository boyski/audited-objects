.PHONY: all clean

all: foo

foo: miniperl
	./miniperl > $@

miniperl:
	@echo "#!/bin/sh\ndate" > $@
	@chmod 755 $@

clean: ; rm -f foo miniperl
