.PHONY: all clean fi zip

NAME	  := raclab

CC	  := gcc
CFLAGS32  := -m32 -g -W -Wall -fPIC -D_REENTRANT -DFEDORA=\"./fedora.so\"
CFLAGS64  := -m64 -g -W -Wall -fPIC -D_REENTRANT -DFEDORA=\"./fedora64.so\"

all: fedora.so lab ltest ttest

fedora.so: fedora.c
	$(CC) $(CFLAGS32) -shared -o $@ $< -lpthread -ldl

fedora64.so: fedora.c
	$(CC) $(CFLAGS64) -shared -o $@ $< -lpthread -ldl

ltest: ltest.c
	$(CC) $(CFLAGS32) -o $@ $< -lpthread -ldl

ttest: ttest.c
	$(CC) $(CFLAGS32) -o $@ $< -lpthread -ldl

lab: lab.c
	$(CC) $(CFLAGS32) -o $@ $< -ldl

lab64: lab.c
	$(CC) $(CFLAGS64) -o $@ $< -ldl

clean:
	-rm -f *.o *.so core core.* lab lab64 ltest ttest

test: all
	./lab
	LD_PRELOAD=./fedora.so ./lab

fi: clean all

zip: clean
	cd .. && zip -r /tmp/$(NAME).zip $(NAME)
