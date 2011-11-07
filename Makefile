.PHONY: all pkg clean realclean

src	:= src

include $(src)/App.properties

all: pkg

# On Windows, build using msbuild/vcbuild in order to keep the solution current.
ifdef VSINSTALLDIR

CFG	 := Release

CPUARCH	:= Windows_i386
TGTARCH	:= $(CPUARCH)

PKG	 := ao-$(TGTARCH).zip

FILES	 := ao.exe tee.exe LibAO.dll CPCI.dll ao.properties-sample # AO

pkg:
	rm -f rel/$(PKG)
	cd $(src) && $(MAKE) CFG=$(CFG) $@
	xcopy /Y etc\ao.properties-windows $(src)\.$(TGTARCH)\$(CFG)\ao.properties-sample
	del /F/Q/S $(src)\.$(TGTARCH)\$(CFG)\AO
	xcopy /Y/S/F/I $(src)\pl\AO $(src)\.$(TGTARCH)\$(CFG)\AO
	xcopy /Y/F $(src)\.$(TGTARCH)\tee.exe $(src)\.$(TGTARCH)\$(CFG)
	cd $(src)\.$(TGTARCH)\$(CFG) && zip -rD ..\..\..\rel\$(PKG) $(FILES)

else	#VSINSTALLDIR

GTAR	 := gtar --owner=0 --group=0 --mode=u+rw --dereference

SRCS	 := $(src)/*.[ch] \
	    $(src)/*.linkmap \
	    $(src)/About/about.c \
	    $(src)/Interposer/*.* \
	    $(src)/Putil/*.* \
	    $(src)/Zip \
	    $(src)/pl \
	    $(src)/Makefile \
	    $(src)/DOXYFILE \
	    $(src)/.indent.pro \
	    $(src)/App.properties

CPUARCH		:= $(CPU)
TGTARCH		?= $(CPUARCH)

SPKG	:= rel/ao-client-src.tar.gz
$(SPKG): $(wildcard $(SRCS))
	$(GTAR) --exclude=win.c --exclude='*/Windows*' \
		--exclude='/zlib/*' \
		--exclude-vcs --exclude-backups \
		--transform='s%^%AO-$(APPLICATION_VERSION)/%' \
		-czf $@ \
	    OPS/README \
	    OPS/*/build-for-ao* \
	    OPS/kazlib/*.tar.gz \
	    OPS/tinycdb/*.tar.gz \
	    OPS/trio/*.tar.gz \
	    $(src)/tst \
	    $(SRCS)

ifneq (,$(findstring Linux,${TGTARCH}))	## LINUX

ifneq (,$(findstring _x86_64,${TGTARCH}))
OSVER	:= $(shell /lib64/libc.so.6 | perl -nle 'print((split m%[ ,]+%)[6]); last')
else
OSVER	:= $(shell /lib/libc.so.6 | perl -nle 'print((split m%[ ,]+%)[6]); last')
endif
PKGS	:= rel/ao-Linux_i386-${OSVER}.tar.gz rel/ao-Linux_x86_64-${OSVER}.tar.gz
rel/ao-Linux_i386-${OSVER}.tar.gz: TGTARCH := Linux_i386
rel/ao-Linux_x86_64-${OSVER}.tar.gz: TGTARCH := Linux_x86_64
pkg:

else					## ELSE NOT LINUX

OSVER	:= $(shell uname -r)
PKGS	:= rel/ao-${TGTARCH}-${OSVER}.tar.gz

endif					## LINUX

pkg: $(PKGS)

FILES	:= lib/libAO.so bin/ao bin/ao2make bin/aoquery lib/perl5/AO \
	   man/man1/ao.1 etc/ao.properties-sample etc/ao.mk
ifeq (SunOS_,$(findstring SunOS_,${TGTARCH}))
FILES	:= lib/64/libAO.so $(FILES)
else ifeq (Linux_,$(findstring Linux_,${TGTARCH}))
FILES	:= lib64/libAO.so $(FILES)
endif

$(PKGS): $(wildcard $(SRCS))
	cd $(src) && $(MAKE) -s rt TGTARCH=$(TGTARCH)
	mkdir -p ${HOME}/${TGTARCH}/etc ${HOME}/${TGTARCH}/man/man1 &&\
	    cp etc/ao.properties-unix ${HOME}/${TGTARCH}/etc/ao.properties-sample &&\
	    chmod 644 ${HOME}/${TGTARCH}/etc/ao.* &&\
	    cp etc/ao.mk ${HOME}/${TGTARCH}/etc/ao.mk &&\
	    mkdir -p ${HOME}/${TGTARCH}/man/man1 &&\
	    pod2man --center="Audited Objects Client" --release="AO" \
		man/ao.pod ${HOME}/${TGTARCH}/man/man1/ao.1 &&\
	    cd ${HOME}/${TGTARCH} && \
	    $(GTAR) -czf $(CURDIR)/$@ $(FILES)

clean:
	rm -f $(PKGS)

realclean:
	$(RM) rel/*.gz rel/*.zip

endif	#VSINSTALLDIR
