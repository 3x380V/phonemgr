
AM_CPPFLAGS = \
	$(PHONEMGR_CFLAGS) \
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\" \
	-DDATA_DIR="\"$(pkgdatadir)\"" \
	-DVERSION="\"$(VERSION)\"" \
	-DPACKAGE="\"$(PACKAGE)\"" \
	-I$(srcdir)/../libgsm \
	-I$(builddir)/../libgsm \
	-I$(srcdir)/../cut-n-paste/e-contact-entry      \
	-I$(builddir)/../cut-n-paste/e-contact-entry    \
	-I$(srcdir)/../cut-n-paste/gconf-bridge         \
	-I$(builddir)/../cut-n-paste/gconf-bridge       \
	-I$(builddir)/src				\
	$(WARN_CFLAGS)

bin_PROGRAMS = gnome-phone-manager

gnome_phone_manager_SOURCES =		\
	phone-manager-interface.h	\
	main.c				\
	icon.c				\
	app.h				\
	phonemgr-conf.h			\
	menu.c				\
	ui.c				\
	connection.c			\
	e-phone-entry.c			\
	e-phone-entry.h			\
	phonemgr-object.c		\
	phonemgr-object.h

gnome_phone_manager_LDADD = \
	../cut-n-paste/e-contact-entry/libecontact-entry.la	\
	$(PHONEMGR_LIBS) \
	../libgsm/libgsmwrap.la		\
	../cut-n-paste/gconf-bridge/libgconf-bridge.la

BUILT_SOURCES = phone-manager-interface.h

phone-manager-interface.h: org_gnome_phone_Manager.xml
	$(DBUS_BIND) --prefix=phonemgr_object --mode=glib-server $< > $@

# The test program
noinst_PROGRAMS = test-entry
test_entry_SOURCES = test-entry.c e-phone-entry.c e-phone-entry.h 
test_entry_LDADD = \
		   ../cut-n-paste/e-contact-entry/libecontact-entry.la	\
		   $(PHONEMGR_LIBS) \
		   ../cut-n-paste/gconf-bridge/libgconf-bridge.la

BUILT_SOURCES += $(MARSHALFILES)

EXTRA_DIST = org_gnome_phone_Manager.xml
CLEANFILES = $(BUILT_SOURCES)
