.SUFFIXES:
.SUFFIXES: .java .class

JAVAC ?= javac

.java.class:
	rm -f $@
	$(JAVAC) $(JFLAGS) $<

all: Main.class

clean:
	rm -f *.class
