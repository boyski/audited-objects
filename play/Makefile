.SECONDEXPANSION:
.SUFFIXES:

OPS := ../OPS/

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
  #-Wmissing-prototypes
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
  -g
  -fPIC
  -fstack-protector-strong
  -pedantic
  -std=c2x
endef

define DFLAGS :=
  -D_GNU_SOURCE
endef

IFLAGS := -I$(OPS)include

hash := \#
CFLAGS := $(filter-out $(hash)%,$(strip $(WFLAGS) $(FFLAGS) $(DFLAGS) $(IFLAGS)))

CJSON := $(patsubst %.c,%,$(wildcard cj*.c))

.PHONY: all
all: $(CJSON)

cjson: cjson.c
	$(CC) -o $@ $(CFLAGS) $< -L$(OPS)Linux_x86_64/lib -lcjson

cj%: cj%.c
	$(CC) -o $@ $(CFLAGS) $< -L$(OPS)Linux_x86_64/lib -lcjson

.PHONY: clean
clean: cleanups := $(wildcard $(basename $(CJSON)))
clean:
	$(if $(cleanups),@set -x; $(RM) -r $(cleanups))

# vim: filetype=make sw=2 noet
