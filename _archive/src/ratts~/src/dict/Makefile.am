# File: ./dict/Makefile.am
# Package: ratts
# Description:
#   + src-level automake file for dictionary utilities
#
# Process this file with Automake to create Makefile.in.
#-----------------------------------------------------------------------

#-----------------------------------------------------------------------
# Options & Subdirectories
#-----------------------------------------------------------------------

#VPATH = $(top_srcdir)/src:../src

#-----------------------------------------------------------------------
# pd externals (hacked _PROGRAMS target)
#-----------------------------------------------------------------------

bin_PROGRAMS = mkdicttxt

#-----------------------------------------------------------------------
# sources
#-----------------------------------------------------------------------

mkdicttxt_SOURCES = \
	mkdicttxt.c \
	../src/trie.c ../src/trie.h \
	../src/darray.c ../src/darray.h \
	phones.c phones.h

#-----------------------------------------------------------------------
# flags
#-----------------------------------------------------------------------
#DEFS    = @DEFS@
#AFLAGS  = @AFLAGS@
#DFLAGS  = @DFLAGS@
IFLAGS  = @IFLAGS@
#LFLAGS  = @LFLAGS@
OFLAGS  = @OFLAGS@
WFLAGS  = -Wall -Winline

#GLIB_IFLAGS = @GLIB_IFLAGS@
#GLIB_LFLAGS = @GLIB_LFLAGS@

AM_CPPFLAGS = $(IFLAGS) -I@srcdir@/../src $(GLIB_IFLAGS)
AM_CFLAGS   = $(OFLAGS) $(WFLAGS)

mkdicttxt_LDFLAGS = $(DICT_LFLAGS)
mkdicttxt_LDADD   = $(GLIB_LFLAGS)

#-----------------------------------------------------------------------
# Variables: cleanup
#-----------------------------------------------------------------------
## --- mostlyclean: built by 'make' & commonly rebuilt
#MOSTLYCLEANFILES =

## --- clean: built by 'make'
#CLEANFILES =

## --- distclean: built by 'configure'
DISTCLEANFILES = \
	config.log	\
	config.cache	\
	config.status

## -- maintainerclean: built by maintainer / by hand
MAINTAINERCLEANFILES = *~ \
	$(PODS:.pod=.txt) \
	Makefile Makefile.in \
	aclocal.m4 \
	configure \
	install-sh \
	stamp-h.in \
	config.h.in

maintainer-clean-local:
	rm -rf autom4te.cache

#CVSCLEAN_SUBDIRS = $(SUBDIRS)

#CVSCLEANFILES = Makefile.in Makefile


#-----------------------------------------------------------------------
# Variables: distribution
#-----------------------------------------------------------------------

## --- extra distribution files
EXTRA_DIST = \
	phones.def


## --- recursion subdirectories for 'make dist'
#DIST_SUBDIRS = $(SUBDIRS)

## --- dist-hook: when another 'Makefile.am' is overkill
#DISTHOOK_DIRS = foo
#DISTHOOK_FILES = foo/bar.txt foo/baz.txt
#dist-hook:
#	for d in $(DISTHOOK_DIRS); do\
#	  mkdir -p $(distdir)/$$d ;\
#	done
#	for f in $(DISTHOOK_FILES); do\
#	  cp -p $(srcdir)/$$f $(distdir)/$$f ;\
#	done

#dist-bz2: dist-bzip2 ;


#-----------------------------------------------------------------------
# Rules: cleanup
#-----------------------------------------------------------------------
.PHONY: cvsclean cvsclean-hook

cvsclean: maintainer-clean ;

