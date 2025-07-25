# Unclear whether this is useful or used. Seems redundant with ../Makefile.

src := .
rel := ../rel
ops := ../OPS

include $(src)/App.properties

.PHONY: all
all: release

GTAR := gtar --owner=0 --group=0 --mode=u+rw --dereference

SRCS := \
  $(src)/*.[ch] \
  $(src)/*.linkmap \
  $(src)/Interposer/*.* \
  $(src)/Putil/*.* \
  $(src)/Zip \
  $(src)/pl \
  $(src)/Makefile \
  $(src)/DOXYFILE \
  $(src)/.indent.pro \
  $(src)/App.properties

TGTARCH := $(CPUARCH)

ifneq (,$(findstring Linux,$(TGTARCH))) ## LINUX

  ifneq (,$(findstring _x86_64,$(TGTARCH)))
    OSVER := $(shell /lib64/libc.so.6 | \
      perl -nle 'print((split m%[ ,]+%)[6]); last')
  else
    OSVER := $(shell /lib/libc.so.6 |\
      perl -nle 'print((split m%[ ,]+%)[6]); last')
  endif
  PKGS := $(rel)/ao-Linux_i386-${OSVER}.tar.gz $(rel)/ao-Linux_x86_64-${OSVER}.tar.gz
  $(rel)/ao-Linux_i386-${OSVER}.tar.gz: TGTARCH := Linux_i386
  $(rel)/ao-Linux_x86_64-${OSVER}.tar.gz: TGTARCH := Linux_x86_64

else ## ELSE NOT LINUX

  OSVER := $(shell uname -r)
  PKGS := $(rel)/ao-$(TGTARCH)-${OSVER}.tar.gz

endif ## LINUX

.PHONY: release
release: $(PKGS)

PKGFILES := lib/libAO.so bin/ao bin/ao2make bin/aoquery lib/perl5/AO \
  man/man1/ao.1 etc/ao.properties-sample etc/ao.mk
PKGFILES := lib64/libAO.so $(PKGFILES)

$(PKGS): $(wildcard $(SRCS))
	$(MAKE) -C $(src) -s rt TGTARCH=$(TGTARCH)
	mkdir -p ${HOME}/$(TGTARCH)/etc ${HOME}/$(TGTARCH)/man/man1 &&\
	  cp ../etc/ao.properties-unix ${HOME}/$(TGTARCH)/etc/ao.properties-sample &&\
	  chmod 644 ${HOME}/$(TGTARCH)/etc/ao.* &&\
	  cp ../etc/ao.mk ${HOME}/$(TGTARCH)/etc/ao.mk &&\
	  pod2man --center="Audited Objects Client" --release="AO" \
	    ../man/ao.pod ${HOME}/$(TGTARCH)/man/man1/ao.1 &&\
	  cd ${HOME}/$(TGTARCH) && $(GTAR) -czf $(CURDIR)/../rel/$@ $(PKGFILES)

.PHONY: clean
clean:
	$(RM) $(PKGS)

.PHONY: realclean
realclean:
	$(RM) $(rel)/*.gz $(rel)/*.zip

# This comment block must stay at the bottom of the file.
# Local Variables:
# mode: makefile-gmake
# fill-column: 80
# End:
#
# vim: filetype=make sw=2 tw=80 cc=+1 noet
