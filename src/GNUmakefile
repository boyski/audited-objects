#!/usr/local/bin/gmake -f

############################################################################
# The command line "gcc -m32 -E -dM -xc /dev/null" will cause gcc
# to dump all the preprocessor macros it knows on that platform
# in 32-bit mode.
############################################################################

.PHONY: all debug release install i clean cleantargets realclean \
	roundtrip rt fi relink manpages htmlpages tags pkg

.SUFFIXES:

# On Windows, defer to msbuild/vcbuild in order to keep the solution current.
ifdef VSINSTALLDIR

MSBLDCMD	:= msbuild AO.sln /nologo
install all: ;	$(MSBLDCMD) /v:n /t:build /p:Configuration=Debug
install-pkg: ;	$(MSBLDCMD) /v:n /t:build /p:Configuration=Release
clean: ;	$(MSBLDCMD) /v:m /t:clean /p:Configuration=Debug
clean-pkg: ;	$(MSBLDCMD) /v:m /t:clean /p:Configuration=Release

CPUARCH	:= Windows_i386
TGTARCH	:= $(CPUARCH)

PKG	 := ao-$(TGTARCH).zip

PKGFILES := ao.exe tee.exe LibAO.dll CPCI.dll ao.properties-sample # AO

pkg: clean-pkg install-pkg
	$(RM) ..\rel\$(PKG)
	$(RM) -r .$(TGTARCH)\Release\AO
	cp ../etc/ao.properties-windows .$(TGTARCH)/Release/ao.properties-sample
	xcopy /Y/S/F/I pl\AO .$(TGTARCH)\Release\AO
	xcopy /Y/F .$(TGTARCH)\tee.exe .$(TGTARCH)\Release
	cd .$(TGTARCH)/Release && zip -rD ../../../rel/$(PKG) $(PKGFILES)

else

.POSIX:

# Derive the $OS_CPU env var which drives all OS/arch divisions.
uname_s		:= $(shell uname -s)
ifneq (,$(findstring CYGWIN,$(uname_s)))
export OS_CPU	:= Cygwin_i386
else ifneq (,$(findstring Linux,$(uname_s)))
ifneq (,$(findstring 64,$(shell uname -m)))
export OS_CPU	:= $(uname_s)_x86_64
else
export OS_CPU	:= $(uname_s)_i386
endif
else
export OS_CPU	:= $(uname_s)_$(shell uname -p)
endif
$(info OS_CPU=$(OS_CPU))

CPUARCH		:= $(OS_CPU)
TGTARCH		?= $(CPUARCH)
TGTDIR		:= .${TGTARCH}
TGTDIR64	:= $(TGTDIR)/64
SCRIPTDIR	:= pl

ifneq (,$(findstring 64,$(TGTARCH)))
BITS		:= 64
else
BITS		:= 32
endif

OPS		:= ../OPS

CC		:= gcc
ANAL		:= -Wcast-align -Wpointer-arith ## -Waggregate-return
#ANAL		+= -Wno-ignored-qualifiers ## requires gcc 4.3+
CSTYLE		:= -Wall -W $(ANAL)
SHLIBFLAGS	:= -shared
INST_PREFIX	:= ${HOME}/${TGTARCH}

# Source file data seems to be unavailable on OS X when using DWARF, and
# STABS seems to do no harm across the board so we make it the default.
# But it's really only for OS X and there may be a better answer there too.
UFLAGS		:= -g

#DFLAGS		:= $(shell getconf LFS_CFLAGS 2>/dev/null || echo $(DEF_DFLAGS))
DFLAGS		:= -D_REENTRANT -D_LARGEFILE_SOURCE -DTRIO_REPLACE_STDIO

CFLAGS		 = $(CSTYLE)

SHLIBDIR	:= /lib

ifneq (,$(findstring SunOS_,${TGTARCH}))
SOLARIS		:= 1
ifneq (,$(findstring _sparc,${TGTARCH}))
SOLARIS_SPARC	:= 1
endif
endif

ifneq (,$(findstring Linux,${TGTARCH}))
LINUX		:= 1
ifneq (,$(findstring _x86_64,${TGTARCH}))
SHLIBDIR	:= /lib64
LINUX_X86_64	:= 1
endif
endif

ifneq (,$(findstring Cygwin,${TGTARCH}))
CYGWIN		:= 1
endif

ifneq (,$(findstring HPUX_,${TGTARCH}))
HPUX		:= 1
endif

ifneq (,$(findstring BSD,${TGTARCH}))
BSD		:= 1
endif

ifneq (,$(findstring Darwin,${TGTARCH}))
DARWIN		:= 1
endif

# Really just needed for one target but passed to all for now.
BUILTON		:= $(shell uname -mnrs)
ifdef LINUX
LIBC		:= $(shell ldd /bin/sh | grep '\<libc\>' | cut -f3 -d' ')
BUILTON		+= $(shell $(LIBC) | perl -nle 'print("glibc ", (split m%[ ,]+%)[6]); last')
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

# The install target should only be default for an actual developer of AO.
ifdef AODEVELOPER
install:
else
all:
endif

# The Kazlib code overloads -DNDEBUG to turn off its own debugging mode.
debug release: all
release: UFLAGS := -Os -DNDEBUG
release: STRIP := strip

$(INST_PREFIX)/lib/%: $(TGTDIR)/%
	@chmod u+w $?
	@[ -d $(@D) ] || mkdir -p $(@D)
	@(set -x; cp $< $@)
	@$(STRIP) $@

$(INST_PREFIX)/lib/64/%: $(TGTDIR64)/%
	@chmod u+w $?
	@[ -d $(@D) ] || mkdir -p $(@D)
	@(set -x; cp $< $@)
	@$(STRIP) $@

$(INST_PREFIX)/lib64/%: $(TGTDIR64)/%
	@chmod u+w $?
	@[ -d $(@D) ] || mkdir -p $(@D)
	@(set -x; cp $< $@)
	@$(STRIP) $@

$(INST_PREFIX)/bin/%: $(TGTDIR)/%
	@chmod u+wx $?
	@[ -d $(@D) ] || mkdir -p $(@D)
	@test -L $@ || (set -x; cp $< $@)

$(INST_PREFIX)/bin/%: pl/%
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

OBJS		:= aotool.o bsd_getopt.o ca.o code.o down.o git.o http.o \
		   make.o moment.o mon.o pn.o prefs.o prop.o pa.o ps.o \
		   putil.o re.o shop.o unix.o up.o util.o vb.o

CFLAGS		+= -D_LARGEFILE64_SOURCE

CFLAGS		+= -I. $(SYSINCS) -I$(OPS)/include

AUDITOR		:= libAO.so
LIBAO32		:= $(addprefix $(TGTDIR)/, $(AUDITOR))
LIBAO64		:= $(addprefix $(TGTDIR64)/, $(AUDITOR))
SHLIBS		:= $(LIBAO32)
INST_LIBS	:= $(addprefix $(INST_PREFIX)/lib/, $(notdir $(LIBAO32)))

AOTOOL		:= $(TGTDIR)/ao
BINS		:= $(AOTOOL)

# In general it's believed best to link the auditor with static libs
# for faster startup time, whereas the driver program starts only
# once so it doesn't matter. Though linking them both statically
# may be logistically simpler.

DLL_LIBS	:= -lkaz -lpcre -lgit2 -lz -ltrio

OPSLIBDIR	:= $(OPS)/${TGTARCH}/lib

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
#SYSINCS			:= -I/opt/csw/include
#SYSLIBS			:= -L/opt/csw/lib -Wl,-R,/opt/csw/lib
#$(LIBAO64): SYSLIBS	:= -L/opt/csw/lib/64 -Wl,-R,/opt/csw/lib/64
all: $(TGTDIR64)
SHLIBS		+= $(LIBAO64)
$(LIBAO64):     OPSLIBDIR := $(OPSLIBDIR)/64
INST_LIBS	+= $(addprefix $(INST_PREFIX)/lib/64/, $(notdir $(LIBAO64)))
endif	#SOLARIS

ifdef LINUX
CFLAGS		+= -fpic -D_GNU_SOURCE
# These require gcc 4.x or so.
#CFLAGS		+= -D_FORTIFY_SOURCE=2
#CFLAGS		+= -fstack-protector --param=ssp-buffer-size=4
SHLIBFLAGS	+= -z defs -z interpose -z combreloc -z lazy
LINKMAP32	:= -Wl,--version-script=Linux32.linkmap
PLDLIBS		:= -lm -lrt -ldl
SYSINCS		:= -I/usr/include
all: $(TGTDIR64)
SHLIBS		+= $(LIBAO64)
$(LIBAO32):     OPSLIBDIR := $(OPS)/Linux_i386/lib
$(LIBAO64):     OPSLIBDIR := $(OPS)/Linux_x86_64/lib
INST_LIBS	+= $(addprefix $(INST_PREFIX)/lib64/, $(notdir $(LIBAO64)))
endif

ifdef CYGWIN
CFLAGS		+= -D_GNU_SOURCE
SHLIBFLAGS	+= -Wl,--enable-auto-image-base
endif

ifdef BSD
CFLAGS		+= -fpic
SHLIBFLAGS	+= -static-libgcc
PLDLIBS		:=
endif

ifdef HPUX
CFLAGS		+= -D_LARGEFILE64_SOURCE
PLDLIBS		:= -lm
endif

ifdef DARWIN
CFLAGS		+= -fPIC -fno-common # Q: add -dynamic too?
CFLAGS		+= -arch i386 -arch x86_64
UFLAGS		:= -gstabs+
SHLIBFLAGS	:= -dynamiclib -flat_namespace -multiply_defined suppress
SHLIBFLAGS	+= -compatibility_version 1.0 -current_version $(APPLICATION_VERSION)
PLDLIBS		:=
endif

ifdef DARWIN
DLL_LIBS	:= -Wl,-search_paths_first -Wl,-Z $(DLL_LIBS)
else
ifndef HPUX
DLL_LIBS	:= -Wl,-Bstatic $(DLL_LIBS) -Wl,-Bdynamic
endif
endif

ifndef DARWIN
$(AOTOOL):	CFLAGS += -m$(BITS)
$(LIBAO32):	CFLAGS += -m32
$(LIBAO64):	CFLAGS += -m64
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

TARGETS		:= $(BINS) $(SHLIBS)

# The list of source files included by libunix.c
COMMINCS	:= libcommon.c ca.c code.c moment.c \
		   pa.c pn.c prefs.c prop.c ps.c \
		   re.c util.c vb.c

INTERPOSERS	:= $(wildcard Interposer/*.h)

INST_BINS	:= $(addprefix $(INST_PREFIX)/bin/, $(notdir $(BINS)))
INST_SCRIPTS	:= $(addprefix $(INST_PREFIX)/bin/, ao2make aoquery)

#############################################################################

$(TGTDIR) $(TGTDIR64):
	mkdir -p $@

all: $(TGTDIR) $(TARGETS)

#############################################################################

.PHONY: docs dox
docs dox:
	rm -rf HTML ${HOME}/HTML
	doxygen DOXYFILE
	cp -r HTML ${HOME}

#############################################################################

$(TGTDIR)/putil.o: Putil/putil.h

$(TGTDIR)/aotool.o: About/about.c

$(TGTDIR)/%.o: %.c AO.h App.properties
	$(CC) -c $(UFLAGS) $(CFLAGS) -o $@ $<

#############################################################################

$(AOTOOL): $(addprefix $(TGTDIR)/, $(OBJS))
	$(CC) -o $@ $(UFLAGS) $(CFLAGS) $(addprefix $(TGTDIR)/, $(OBJS)) -L$(OPSLIBDIR) $(SYSLIBS) $(EXE_LIBS) $(PLDLIBS)

$(LIBAO32): libunix.c AO.h libcommon.c $(COMMINCS) $(INTERPOSERS)
	$(CC) -o $@ $(UFLAGS) $(CFLAGS) $(SHLIBFLAGS) $(LINKMAP32) $< -L$(OPSLIBDIR) $(SYSLIBS) $(DLL_LIBS) $(PLDLIBS)

$(LIBAO64): libunix.c AO.h libcommon.c $(COMMINCS) $(INTERPOSERS)
	$(CC) -o $@ $(UFLAGS) $(CFLAGS) $(SHLIBFLAGS) $(LINKMAP64) $< -L$(OPSLIBDIR) $(SYSLIBS) $(DLL_LIBS) $(PLDLIBS)

cleantargets:
	-rm -f $(TARGETS)

clean: cleantargets
	-$(RM) $(TGTDIR)/*.o core *.1 *.html HTML tags
	-$(RM) -r HTML

tags:
	$(RM) tags && ectags *.h *.c

realclean: clean
	-$(RM) *.user .*/*.o .*/*.obj 2>/dev/null

ifdef SOLARIS
ifndef SOLARIS_SPARC
roundtrip rt fi: tags
endif
endif

relink: cleantargets install

install: all $(INST_LIBS) $(INST_BINS) $(INST_SCRIPTS)

pkg:
	$(MAKE) --no-print-directory CPUARCH=$(OS_CPU) -f Makefile.pkg pkg

endif

roundtrip rt fi: clean install
