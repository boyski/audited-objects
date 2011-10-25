.PHONY: all clean test datetest cksumtest

SO		:= so

LIBDL		:= -ldl

CFLAGS		:= -g -W -Wall

ifneq (,$(findstring Linux,${PLATFORM}))
DLFLAGS		:= -Wl,--export-dynamic
endif

ifneq (,$(findstring BSD,${PLATFORM}))
DLFLAGS		:= -Wl,--export-dynamic
LIBDL		:=
endif

ifneq (,$(findstring Darwin,${PLATFORM}))
SOFLAGS		:= -dynamiclib
else
SOFLAGS		:= -fpic -shared -static-libgcc
endif

ifneq (,$(findstring Windows,${PLATFORM}))
SOFLAGS		:= -shared
LIBDL		:=
X		:= .exe
SO		:= dll
endif

all: oodtst$X libckood.$(SO)

oodtst$X: oodtst.c
	gcc -o $@ $(CFLAGS) $(DLFLAGS) oodtst.c $(LIBDL)

libckood.$(SO): libckood.c
	gcc $(CFLAGS) -o $@ $(SOFLAGS) $<

test: all datetest cksumtest

datetest:
	./oodtst

ifneq (,$(findstring Windows,${PLATFORM}))
cksumtest:; set MAKEOODLIB=.\libckood.dll && .\oodtst
else
cksumtest:; MAKEOODLIB=./libckood.so ./oodtst
endif

clean:
	-rm -f *.$(SO) oodtst$X
