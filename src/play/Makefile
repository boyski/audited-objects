.SECONDEXPANSION:
.SUFFIXES:

OPS := ../../OPS/

CC := gcc

define WFLAGS :=
  -Wall
  -Wcast-align
  -Wcast-qual
  -Wconversion
  -Werror
  -Wextra
  -Wformat=2
  -Winit-self
  -Wmissing-prototypes
  -Wno-ignored-qualifiers
  -Wpointer-arith
  -Wshadow
  -Wstrict-overflow=2
  -Wstrict-prototypes
  -Wswitch-default
  -Wundef
  -Wwrite-strings
endef

define FFLAGS :=
  -fPIC
  -fstack-protector-strong
  -pedantic
  -std=c17
endef

IFLAGS := -I$(OPS)include

CFLAGS := $(strip $(WFLAGS) $(DFLAGS) $(IFLAGS))

.PHONY: all
all: cjson

cjson: cjson.c
	$(CC) -o $@ $(CFLAGS) $< -L$(OPS)Linux_x86_64/lib -lcjson

.PHONY: clean
clean:
	$(RM) cjson

# This comment block must stay at the bottom of the file.
# Local Variables:
# mode: makefile-gmake
# fill-column: 100
# End:
#
# vim: filetype=make sw=2 tw=100 cc=+1 noet
