#!/usr/local/bin/gmake -f

############################################################################
# The command line "gcc -m32 -E -dM -xc /dev/null" will cause gcc
# to dump all the preprocessor macros it knows on that platform
# in 32-bit mode.
############################################################################

.PHONY: all debug release install i clean cleantargets realclean \
	roundtrip rt fi relink manpages htmlpages tags

.SUFFIXES:

# On Windows, defer to msbuild/vcbuild in order to keep the solution current.
ifdef VSINSTALLDIR

CFG		:= Debug
MSBLDCMD	:= msbuild AO.sln /nologo /p:Configuration=$(CFG)
install all: ;	$(MSBLDCMD) /v:n /t:build
clean: ;	$(MSBLDCMD) /v:m /t:clean

else

.POSIX:

TAR		:= gtar --owner=0 --group=0 --mode=u+rw --dereference

ifndef CPU
uname_s		:= $(shell uname -s)
ifneq (,$(findstring Linux,$(uname_s)))
CPU		:= $(uname_s)_i386
else
CPU		:= $(uname_s)_$(shell uname -p)
endif
endif

export CPU

CPUDIR		:= .${CPU}
CPUDIR64	:= $(CPUDIR)/64

OPS		:= ../OPS

CC		:= gcc
ANAL		:= -Wcast-align -Wpointer-arith ## -Waggregate-return
#ANAL		+= -Wno-ignored-qualifiers ## requires gcc 4.3+
CSTYLE		:= -W -Wall $(ANAL)
SHLIBFLAGS	:= -shared
INST_PREFIX	:= ${HOME}/${CPU}

# Source file data seems to be unavailable on OS X when using DWARF, and
# STABS seems to do no harm across the board so we make it the default.
# But it's really only for OS X and there may be a better answer there too.
UFLAGS		:= -g

#DFLAGS		:= $(shell getconf LFS_CFLAGS 2>/dev/null || echo $(DEF_DFLAGS))
DFLAGS		:= -D_REENTRANT -D_LARGEFILE_SOURCE -DTRIO_REPLACE_STDIO
DFLAGS		+= -DUSE_LIBCRYPTO # -DUSE_LIBCRYPTO_EVP

CFLAGS		:= $(CSTYLE)

ifneq (,$(findstring SunOS_,${CPU}))
SOLARIS		:= 1
ifneq (,$(findstring _sparc,${CPU}))
SOLARIS_SPARC	:= 1
endif
endif

ifneq (,$(findstring Linux,${CPU}))
LINUX		:= 1
ifneq (,$(findstring _x86_64,${CPU}))
LINUX_X86_64	:= 1
endif
endif

ifneq (,$(findstring HPUX_,${CPU}))
HPUX		:= 1
endif

ifneq (,$(findstring BSD,${CPU}))
BSD		:= 1
endif

ifneq (,$(findstring Darwin,${CPU}))
DARWIN		:= 1
endif

# Really just needed for one target but passed to all for now.
BUILTON		:= $(shell uname -mnrs)
ifdef LINUX
BUILTON		+= $(shell /lib/libc.so.6 | perl -nle 'print("glibc ", (split m%[ ,]+%)[6]); last')
endif
DFLAGS		+= -DPUTIL_BUILTON='"$(BUILTON)"'

CFLAGS		+= $(DFLAGS)

STRIP		:= :

MAN_HDR		:= User Documentation
POD2MAN_FLAGS	= -r "$(MAN_RELEASE)" -c "$(MAN_HDR)"

%.1: %.pod
	pod2man $(POD2MAN_FLAGS) $< > $@

%.html: %.pod
	pod2html --infile=$< --outfile=$@
	@rm -f *~~

#############################################################################

install:

# The Kazlib code overloads -DNDEBUG to turn off its own debugging mode.
debug release: all
release: UFLAGS := -Os -DNDEBUG
release: STRIP := strip

$(INST_PREFIX)/lib/%: $(CPUDIR)/%
	@chmod u+w $?
	@[ -d $(@D) ] || mkdir -p $(@D)
	@(set -x; cp $< $@)
	@$(STRIP) $@

$(INST_PREFIX)/lib/64/%: $(CPUDIR64)/%
	@chmod u+w $?
	@[ -d $(@D) ] || mkdir -p $(@D)
	@(set -x; cp $< $@)
	@$(STRIP) $@

$(INST_PREFIX)/bin/%: $(CPUDIR)/%
	@chmod u+wx $?
	@[ -d $(@D) ] || mkdir -p $(@D)
	@test -L $@ || (set -x; cp $< $@)

# Compile in the version number if accessible.
include App.properties
ifdef APPLICATION_VERSION
CFLAGS		+= -DAPPLICATION_VERSION='"$(APPLICATION_VERSION)"'
else
APPLICATION_VERSION	:= 0.0
endif

OBJS		:= aotool.o bsd_getopt.o ca.o code.o down.o http.o \
		   make.o moment.o mon.o pn.o prefs.o prop.o pa.o ps.o \
		   putil.o re.o shop.o unix.o up.o util.o vb.o

CFLAGS		+= -D_LARGEFILE64_SOURCE

CFLAGS		+= -I. -I$(OPS)/include

LIBAO32		:= $(addprefix $(CPUDIR)/, libao.so)
LIBAO64		:= $(addprefix $(CPUDIR64)/, libao.so)
SHLIBS		:= $(LIBAO32)
INST_LIBS	:= $(addprefix $(INST_PREFIX)/lib/, $(notdir $(SHLIBS)))

AOTOOL		:= $(CPUDIR)/ao
BINS		:= $(AOTOOL)
INST_BINS	:= $(addprefix $(INST_PREFIX)/bin/, $(notdir $(BINS)))

AO2MAKE		:= $(CPUDIR)/ao2make
SCRIPTS		:= $(AO2MAKE)
INST_SCRIPTS	:= $(addprefix $(INST_PREFIX)/bin/, $(notdir $(SCRIPTS)))

# In general it's believed best to link the auditor with static libs
# for faster startup time, whereas the driver program starts only
# once so it doesn't matter. Though linking them both statically
# may be logistically simpler.

DLL_LIBS	:= -lkaz -lpcre -lz -ltrio -lcrypto

OPSLIBDIR	:= $(OPS)/${CPU}/lib

# On Solaris we can get some malloc debugging by linking with -lumem.
# But we must use the vanilla malloc in production so as not to
# complicate things for the host process.
ifdef SOLARIS
CFLAGS		+= -fpic
SHLIBFLAGS	+= -nodefaultlibs -Bdirect -z defs -z text -z interpose -z combreloc -z ignore -z lazyload
## I worked out a link map for Solaris which cut the number of exported
## symbols from 685 to ~60 in hopes that this would speed up loading
## of the audit library. Unfortunately, though it works fine, it has no
## measurable effect on performance. We might still want to do this for
## other reasons so the files and this note are left in place.
#LINKMAP32	:= -Wl,-M,Sol32.linkmap
#LINKMAP64	:= -Wl,-M,Sol64.linkmap
# For some reason the -lgcc is required on Solaris; otherwise we get
# "fatal: relocation error: symbol __floatdidf: referenced symbol not found"
# It's *possible* that after upgrading to libcurl 7.19.x with LFS support
# turned on that -lgcc would not be needed. Strangely, ldd shows no
# dependency even though we do not use -static-libgcc.
ifdef SOLARIS_SPARC
PLDLIBS		:= -lgcc -lsocket -lnsl -lm -ldl -lc # -lumem
else
PLDLIBS		:= -lgcc -lsocket -lnsl -lm -ldl -lrt -lc
endif	#SOLARIS_SPARC
all: $(CPUDIR64)
SHLIBS		+= $(LIBAO64)
INST_LIBS	+= $(addprefix $(INST_PREFIX)/lib/64/, $(notdir $(LIBAO64)))
endif	#SOLARIS

ifdef LINUX
CFLAGS		+= -fpic -D_GNU_SOURCE
SHLIBFLAGS	+= -z defs -z interpose -z combreloc -z ignore -z lazyload
LINKMAP32	:= -Wl,--version-script=Linux32.linkmap
ifdef LINUX_X86_64
DLL_LIBS	+= -lrt
PLDLIBS		:= -ldl -lm
else
DLL_LIBS	+= -lrt -lm
PLDLIBS		:= -ldl
endif
endif

ifdef BSD
CFLAGS		+= -fpic
SHLIBFLAGS	+= -static-libgcc
PLDLIBS		:=
endif

ifdef DARWIN
CFLAGS		+= -fPIC -fno-common # Q: add -dynamic too?
UFLAGS		:= -gstabs+
SHLIBFLAGS	:= -dynamiclib -flat_namespace -multiply_defined suppress
PLDLIBS		:=
endif

ifdef HPUX
CFLAGS		+= -D_LARGEFILE64_SOURCE
PLDLIBS		:= -lm
endif

ifdef DARWIN
DLL_LIBS	:= -Wl,-search_paths_first -Wl,-Z $(DLL_LIBS)
else
ifndef HPUX
DLL_LIBS	:= -Wl,-Bstatic $(DLL_LIBS) -Wl,-Bdynamic
endif
endif

EXE_LIBS	:= -lcurl -lcdb $(DLL_LIBS)

## NOTE: the native HP-UX compiler/linker (aCC) uses +init/+fini to specify
## static constructors. This also seems to differ between 32- and 64-bit
## and PA and Itanium (boo).

ifneq (,$(MEMDBG))
# Likely values are MEMDBG=-lumem, MEMDBG=-ldmalloc, and MEMDBG=-lwatchmalloc
# These may need EV's to turn them on:
# UMEM_DEBUG=default,verbose or MALLOC_DEBUG=WATCH
PLDLIBS		+= $(MEMDBG)
ifneq (,$(findstring dmalloc,${MEMDBG}))
CFLAGS		+= -DDMALLOC
endif	#dmalloc
endif	#MEMDBG

TARGETS		:= $(BINS) $(SHLIBS) $(SCRIPTS)

# The list of source files included by libunix.c
COMMINCS	:= libcommon.c ca.c code.c moment.c \
		   pa.c pn.c prefs.c prop.c ps.c \
		   re.c util.c vb.c

INTERPOSERS	:= $(wildcard Interposer/*.h)

#############################################################################

$(CPUDIR) $(CPUDIR64):
	mkdir -p $@

all: $(CPUDIR) $(TARGETS)

#############################################################################

.PHONY: docs dox
docs dox:
	rm -rf HTML ${HOME}/HTML
	doxygen DOXYFILE
	cp -r HTML ${HOME}

#############################################################################

$(CPUDIR)/putil.o: Putil/putil.h

$(CPUDIR)/aotool.o: About/about.c

$(CPUDIR)/%.o: %.c AO.h App.properties
	$(CC) -c $(UFLAGS) $(CFLAGS) -o $@ $<

#############################################################################

$(AOTOOL): $(addprefix $(CPUDIR)/, $(OBJS))
	$(CC) -o $@ $(UFLAGS) $(addprefix $(CPUDIR)/, $(OBJS)) -L$(OPSLIBDIR) $(EXE_LIBS) $(PLDLIBS)

$(LIBAO32): libunix.c AO.h libcommon.c $(COMMINCS) $(INTERPOSERS)
	$(CC) -o $@ $(UFLAGS) $(CFLAGS) $(SHLIBFLAGS) $(LINKMAP32) $< -L$(OPSLIBDIR) $(DLL_LIBS) $(PLDLIBS)

$(LIBAO64): libunix.c AO.h libcommon.c $(COMMINCS) $(INTERPOSERS)
	$(CC) -m64 -o $@ $(UFLAGS) $(CFLAGS) $(SHLIBFLAGS) $(LINKMAP64) $< -L$(OPSLIBDIR)/64 $(DLL_LIBS) $(PLDLIBS)

$(AO2MAKE): $(addprefix pl/, $(notdir $(AO2MAKE)))
	@(set -x; cp $< $@) && chmod u+w,a+x $@

cleantargets:
	-rm -f $(TARGETS)

clean: cleantargets
	-rm -rf $(CPUDIR)/*.o core *.1 *.html HTML tags

tags:
	rm -f tags && ectags *.h *.H *.c *.cpp

realclean: clean
	-rm -rf *.user .*/*.o .Windows_i386 CPCI 2>/dev/null

ifdef SOLARIS
ifndef SOLARIS_SPARC
roundtrip rt fi: tags
endif
endif

relink: cleantargets install

install: all $(INST_LIBS) $(INST_BINS) $(INST_SCRIPTS)

endif

roundtrip rt fi: clean install
