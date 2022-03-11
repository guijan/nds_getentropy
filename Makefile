ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>devkitPro)
endif

export TOPDIR	:=	$(CURDIR)

export LIBNDS_GETENTROPY_MAJOR	:= 0
export LIBNDS_GETENTROPY_MINOR	:= 0
export LIBNDS_GETENTROPY_PATCH	:= 0


VERSION	:=	$(LIBNDS_GETENTROPY_MAJOR).$(LIBNDS_GETENTROPY_MINOR).$(LIBNDS_GETENTROPY_PATCH)
PROJ := libnds_getentropy


.PHONY: release clean all

#-------------------------------------------------------------------------------
all:
#-------------------------------------------------------------------------------
	mkdir -p lib
	$(MAKE) -C arm7 PROJ="$(PROJ)" || { exit 1;}

#-------------------------------------------------------------------------------
clean:
#-------------------------------------------------------------------------------
	$(MAKE) -C arm7 clean

#-------------------------------------------------------------------------------
dist:
#-------------------------------------------------------------------------------
	@# The Make braindamage is strong on this one.
	@# The hack to avoid releasing tarballs with object files is to ignore
	@# every path that contains "build".
	pax -s '/.*build.*//'						\
	    -s '/^\./$(PROJ)-$(VERSION)/' -wx ustar 			\
	    ./arm7 ./include ./Makefile ./LICENSE.md ./README.md ./examples \
		| gzip -9c > "$(PROJ)-$(VERSION).tgz"

#-------------------------------------------------------------------------------
dist-bin: all
#-------------------------------------------------------------------------------
	@# Can't do multiple substitutions on the same file with pax so we
	@# create a temp dir.
	mkdir -p "$(PROJ)-bin-$(VERSION)/libnds"

	@# "$(PROJ)-bin-$(VERSION)" is a mirror of the root of the devkitpro
	@# directory. We move our repo files into a particular file layout
	@# inside it, imitating the official devkitPro libraries.
	pax -s '/.*build.*//'						\
	    -s '/LICENSE\.md/libnds_getentropy_license.txt/'		\
	    -s '/README\.md/libnds_getentropy_readme.txt/'		\
	    -rw include lib LICENSE.md README.md "$(PROJ)-bin-$(VERSION)/libnds"
	pax -s '/.*build.*//'						\
	    -s '|examples|examples/nds/libnds_getentropy|'		\
	    -rw examples "$(PROJ)-bin-$(VERSION)/"

	pax -wx ustar "$(PROJ)-bin-$(VERSION)"				\
		| gzip -9c > "$(PROJ)-bin-$(VERSION).tgz"

	rm -r "$(PROJ)-bin-$(VERSION)"

#-------------------------------------------------------------------------------
install: dist-bin
#-------------------------------------------------------------------------------
	mkdir -p "$(DESTDIR)$(DEVKITPRO)/libnds"
	gzip -dc "$(PROJ)-bin-$(VERSION).tgz"				\
		| (cd "$(DESTDIR)$(DEVKITPRO)" && pax -s '|[^/]*|.|' -r)

#-------------------------------------------------------------------------------
uninstall:
#-------------------------------------------------------------------------------
	rm -rf '$(DESTDIR)$(DEVKITPRO)/libnds/include/nds_getentropy.h' \
	    '$(DESTDIR)$(DEVKITPRO)/libnds/lib/libnds_getentropy7.a' \
	    '$(DESTDIR)$(DEVKITPRO)/libnds/libnds_getentropy_license.txt' \
	    '$(DESTDIR)$(DEVKITPRO)/libnds/libnds_getentropy_readme.txt'  \
	    '$(DESTDIR)$(DEVKITPRO)/examples/nds/libnds_getentropy'
