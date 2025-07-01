############################################################################
# Note: this makefile previously supported platforms like HP-UX,
# Solaris, BSD, Darwin, Cygwin, and native Windows and both 32-
# and 64-bit variants. However, it's been much simplified because
# only 64-bit Linux will be supported from now on. Code for HP, BSD,
# Solaris and Windows is completely removed. Logic for MacOS (Darwin)
# is left around for now but Darwin is untested and unsupported.
############################################################################

############################################################################
# The command line "gcc -m64 -E -dM -xc /dev/null" will cause gcc
# to dump all the preprocessor macros it knows on that platform.
############################################################################

.SECONDEXPANSION:
.SUFFIXES:

# Derive the $OS_CPU env var which drives all OS/arch divisions.
uname_s := $(shell uname -s)
ifneq (,$(findstring Linux,$(uname_s)))
  ifneq (,$(findstring 64,$(shell uname -m)))
    OS_CPU := $(uname_s)_x86_64
  else
    OS_CPU := $(uname_s)_i386
  endif
else
  OS_CPU := $(uname_s)_$(shell uname -p)
endif

CPUARCH := $(OS_CPU)
TGTARCH := $(CPUARCH)
TGTDIR := .$(TGTARCH)
BINDIR := $(TGTDIR)/bin
LIBDIR := $(TGTDIR)/lib64
SCRIPTDIR := py

OPS := ../OPS

CC := gcc

define FFLAGS :=
  -fPIC
  -fstack-protector-strong
  -m64
  -pedantic
  -std=c17
  #-fdiagnostics-show-option
endef

define WFLAGS :=
  -Wall
  -Wcast-align
  -Werror
  -Wextra
  -Wformat=2
  -Winit-self
  -Wmissing-prototypes
  -Wno-ignored-qualifiers
  -Wpointer-arith
  -Wshadow
  -Wstrict-overflow=2
  -Wswitch-default
  -Wundef
endef

SHLIBFLAGS := -shared
INST_PREFIX := ${HOME}/$(TGTARCH)

GFLAGS := -g

DFLAGS := -D_REENTRANT -DFILE_OFFSET_BITS=64 -DTRIO_REPLACE_STDIO
DFLAGS += -D_GNU_SOURCE

SHLIBDIR := /lib

ifneq (,$(findstring Linux,$(TGTARCH)))
  LINUX := 1
  ifneq (,$(findstring _x86_64,$(TGTARCH)))
    SHLIBDIR := /lib64
    LINUX_X86_64 := 1
  endif
endif

# Really just used by one target but passed to all for now.
BUILTON := $(shell uname -mnrs)
ifneq ($(LINUX),)
  LIBC := $(shell ldd /bin/sh | grep '\<libc\>' | cut -f3 -d' ')
  BUILTON += $(shell $(LIBC) |\
    perl -nle 'print("glibc ", (split m%[ ,]+%)[6]); last')
endif
DFLAGS += -DPUTIL_BUILTON='"$(BUILTON)"'

STRIP := :

MAN_HDR := User Documentation
POD2MAN_FLAGS := -r "$(MAN_RELEASE)" -c "$(MAN_HDR)"

%.1: %.pod
	pod2man $(POD2MAN_FLAGS) $< > $@

%.html: %.pod
	pod2html --infile=$< --outfile=$@
	@$(RM) *~~

#############################################################################

# The install target should only be default for an actual developer of AO.
ifneq ($(AODEVELOPER),)
  .PHONY: install
  install:
else
  .PHONY: all
  all:
endif

$(BINDIR) $(LIBDIR) $(INST_PREFIX)/bin $(INST_PREFIX)/lib64:
	mkdir -p $@

$(INST_PREFIX)/lib64/%: $(LIBDIR)/% | $$(@D)
	@chmod u+w $?
	@(set -x; cp -p $< $@)
	@$(STRIP) $@

$(INST_PREFIX)/bin/%: $(TGTDIR)/% | $$(@D)
	@chmod u+wx $?
	@test -L $@ || (set -x; cp -p $< $@)

$(INST_PREFIX)/bin/%: py/% | $$(@D)
	@chmod u+wx $?
	@test -L $@ || (set -x; cp -p $< $@)

# Compile in the version number if accessible.
include App.properties
ifneq ($(APPLICATION_VERSION),)
  DFLAGS += -DAPPLICATION_VERSION='"$(APPLICATION_VERSION)"'
else
  APPLICATION_VERSION := 0.0
endif

OBJS := \
  aotool.o bsd_getopt.o ca.o code.o down.o git.o http.o \
  make.o moment.o mon.o pn.o prefs.o prop.o pa.o ps.o \
  putil.o re.o sha1.o shop.o tee.o unix.o up.o util.o \
  vb.o zips.o

IFLAGS := -I. -I$(OPS)/include

CFLAGS := $(filter-out #%,$(strip $(FFLAGS) $(WFLAGS) $(DFLAGS) $(IFLAGS)))

AUDITOR	:= libAO.so
LIBAO := $(addprefix $(LIBDIR)/,$(AUDITOR))
INST_LIBS += $(addprefix $(INST_PREFIX)/lib64/,$(notdir $(LIBAO)))

AOTOOL := $(BINDIR)/ao
BINS := $(AOTOOL)

# In general it's believed best to link the auditor with static libs
# for faster startup time, whereas the driver program starts only
# once so it doesn't matter. Though linking them both statically
# may be logistically simpler.

DLL_LIBS := -lkaz -lpcre -lz -ltrio

OPSLIBDIR := $(OPS)/Linux_x86_64/lib

ifneq ($(LINUX),)
  SHLIBFLAGS += -z defs -z interpose -z combreloc -z lazy
  LINKMAP := -Wl,--version-script=Linux.linkmap
  LINKMAP32 := $(LINKMAP)
  LINKMAP64 := $(LINKMAP)
  PLDLIBS := -lm -lrt -ldl
else ifneq ($(DARWIN),)
  CFLAGS += -fno-common # Q: add -dynamic too?
  CFLAGS += -arch x86_64
  GFLAGS :=
  SYSLIBS := -L/usr/lib
  SHLIBFLAGS := -dynamiclib -flat_namespace -multiply_defined suppress
  SHLIBFLAGS += -compatibility_version 1.0 -current_version $(APPLICATION_VERSION)
  PLDLIBS :=
  DLL_LIBS := -Wl,-search_paths_first -Wl,-Z $(DLL_LIBS)
endif

EXE_LIBS := -lcurl -lcdb $(DLL_LIBS)

ifneq (,$(MEMDBG))
  # Likely values are MEMDBG=-lumem, MEMDBG=-ldmalloc, and MEMDBG=-lwatchmalloc
  # These may need EV's to turn them on:
  # UMEM_DEBUG=default,verbose or MALLOC_DEBUG=WATCH
  PLDLIBS += $(MEMDBG)
  ifneq (,$(findstring dmalloc,$(MEMDBG)))
    CFLAGS += -DDMALLOC
  endif   #dmalloc
endif   #MEMDBG

TARGETS := $(BINS) $(LIBAO)

# The list of source files included by libunix.c
COMMINCS := \
  libcommon.c \
  ca.c \
  code.c \
  moment.c \
  pa.c \
  pn.c \
  prefs.c \
  prop.c \
  ps.c \
  re.c \
  sha1.c \
  util.c \
  vb.c \
  zips.c

INTERPOSERS := $(wildcard Interposer/*.h)

INST_BINS := $(addprefix $(INST_PREFIX)/bin/,$(notdir $(BINS)))
INST_SCRIPTS := $(addprefix $(INST_PREFIX)/bin/,ao2make aoquery)

#############################################################################

.PHONY: all
all: $(TARGETS) | $(TGTDIR) $(LIBDIR)

#############################################################################

.PHONY: docs dox
docs dox:
	$(RM) -r HTML ${HOME}/HTML
	doxygen DOXYFILE
	cp -pr HTML ${HOME}

#############################################################################

$(TGTDIR)/putil.o: Putil/putil.h

$(TGTDIR)/aotool.o: About/about.c

$(TGTDIR)/%.o: %.c AO.h App.properties
	$(CC) -c $(GFLAGS) $(CFLAGS) -o $@ $<

#############################################################################

$(AOTOOL): $(addprefix $(TGTDIR)/,$(OBJS)) | $$(@D)
	$(strip $(CC) -o $@ $(GFLAGS) $(CFLAGS) $(addprefix $(TGTDIR)/,$(OBJS)) \
	  -L$(OPSLIBDIR) $(SYSLIBS) $(EXE_LIBS) $(PLDLIBS))

$(LIBAO): libunix.c AO.h libcommon.c $(COMMINCS) $(INTERPOSERS) | $$(@D)
	$(strip $(CC) -o $@ $(GFLAGS) $(CFLAGS) $(SHLIBFLAGS) $(LINKMAP64) $< \
	  -L$(OPSLIBDIR) $(SYSLIBS) $(DLL_LIBS) $(PLDLIBS))

.PHONY: tags
tags:
	$(RM) tags && ectags *.h *.c

.PHONY: cleantargets
cleantargets:
	-$(RM) $(TARGETS)

.PHONY: clean
clean: cleantargets
	-$(RM) $(TGTDIR)/*.o core *.1 *.html HTML tags
	-$(RM) -r HTML

.PHONY: realclean
realclean: clean
	-$(RM) *.user .*/*.o .*/*.obj 2>/dev/null

.PHONY: relink
relink: cleantargets install

.PHONY: install
install: all $(INST_LIBS) $(INST_BINS) $(INST_SCRIPTS)

.PHONY: release
release:
	$(MAKE) --no-print-directory CPUARCH=$(OS_CPU) -f package.mk release

.PHONY: roundtrip rt
roundtrip rt: clean install

# This comment block must stay at the bottom of the file.
# Local Variables:
# mode: makefile-gmake
# fill-column: 100
# End:
#
# vim: filetype=make sw=2 tw=100 cc=+1 noet
