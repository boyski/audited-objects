.PHONY: all clean subdirs

all: rd subdirs

rd: rd.o
	$(CC) $^ -o $@

subdirs:
	cd rel && $(MAKE)
	cd dotdot && $(MAKE)
	cd double && $(MAKE)
	cd double2 && $(MAKE)

clean: ; rm -f rd rd.o */rd */rd.o
