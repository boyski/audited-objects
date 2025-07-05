# This is a packaging/distribution makefile. You may not want it.
# Simpler to build from the ./src dir.

src := src

include $(src)/App.properties

.PHONY: all
all: pkg

GTAR := tar --owner=0 --group=0 --mode=u+rw --dereference

SRCS := \
  $(src)/*.[ch] \
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

CPUARCH	:= $(OS_CPU)
TGTARCH	:= $(CPUARCH)

rdir := java/WebContent/client

SPKG := $(rdir)/ao-client-src.tar.gz
$(SPKG): $(wildcard $(SRCS))
	$(strip $(GTAR) \
	  --exclude=win.c --exclude='*/Windows*' \
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
	    $(SRCS))

PKGFILES := bin/ao bin/ao2make bin/aoquery man/man1/ao.1 etc/ao.properties-sample etc/ao.mk

ifneq (,$(findstring Linux,$(TGTARCH)))	## LINUX
  OSVER := $(shell /lib64/libc.so.6 | \
    perl -nle 'print((split m%[ ,]+%)[6]); last')
  PKGS := $(rdir)/ao-Linux_x86_64-$(OSVER).tar.gz

  $(rdir)/ao-Linux_x86_64-$(OSVER).tar.gz: TGTARCH := Linux_x86_64

  .PHONY: pkg release
  pkg release: $(PKGS)

  PKGFILES := lib64/libAO.so $(PKGFILES)
else # !Linux
  OSVER := $(shell uname -r)
  PKGS := $(rdir)/ao-$(TGTARCH)-$(OSVER).tar.gz

  .PHONY: pkg release
  pkg release: $(PKGS)
endif # Linux

PKGFILES := $(addprefix $(TGTARCH)/,$(PKGFILES))

${HOME}/$(TGTARCH)/etc ${HOME}/$(TGTARCH)/man/man1:
	mkdir -p $@

$(PKGS): $(wildcard $(SRCS)) | ${HOME}/$(TGTARCH)/etc ${HOME}/$(TGTARCH)/man/man1
	$(MAKE) -C $(src) -s TGTARCH=$(TGTARCH) clean all
	cp -p etc/ao.properties-unix ${HOME}/$(TGTARCH)/etc/ao.properties-sample && \
	chmod 644 ${HOME}/$(TGTARCH)/etc/ao.* && \
	cp -p etc/ao.mk ${HOME}/$(TGTARCH)/etc/ao.mk && \
	pod2man --center="Audited Objects" --release="AO" \
	  man/ao.pod ${HOME}/$(TGTARCH)/man/man1/ao.1 && \
	cd ${HOME} && $(GTAR) -czf $(CURDIR)/$@ $(PKGFILES)

.PHONY: clean
clean: cleanups := $(wildcard $(PKGS))
clean:
	$(if $(cleanups),@set -x; $(RM) -r $(cleanups))

.PHONY: realclean
clean: cleanups := $(wildcard $(rdir)/*.gz $(rdir)/*.zip)
realclean:
	$(if $(cleanups),@set -x; $(RM) -r $(cleanups))

# vim: filetype=make sw=2 noet
