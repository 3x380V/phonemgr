## Process this file with automake to produce Makefile.in

SUBDIRS = libgsm cut-n-paste src gnome-bluetooth telepathy data po
ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = \
	AUTHORS \
	NEWS \
	README \
	TODO \
	autogen.sh

# Build ChangeLog from GIT history
ChangeLog:
	@if test -f $(top_srcdir)/.git/HEAD; then \
		git log --stat V_0_65.. > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog
