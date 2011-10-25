.PHONY: all clean

all: rd

rd: rd.o
	$(CC) ../$^ -o ../$@
	cp -p ../rd.o ../rd .

clean: ; rm -f rd rd.o
