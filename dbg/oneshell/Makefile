.PHONY: all clean

.ONESHELL:

all: stdio.h rmt-command.h foobar

stdio.h: stdio.in.h
	rm -f stdio.h-t stdio.h
	{ echo '/* DO NOT EDIT! GENERATED AUTOMATICALLY! */' && \
	  sed -e 's/@''INCLUDE_NEXT''@/include_next/g' \
	      -e 's|@''REPLACE_GETLINE''@|1|g' \
	      < ./stdio.in.h; \
	} > stdio.h-t
	mv stdio.h-t stdio.h

rmt-command.h:
	rm -f $@-t $@
	echo "#ifndef RMT_COMMAND" >> $@-t
	echo "# define RMT_COMMAND \"`echo rmt | sed 's,x,x,'`\"" >> $@-t
	echo "#endif" >> $@-t
	mv $@-t $@

foobar:
		@echo Making $@
		@touch $@

clean: ; rm -f stdio.h rmt-command.h foobar
