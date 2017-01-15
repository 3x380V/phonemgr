SUBDIRS = icons

man_MANS = gnome-phone-manager.1

uidir = $(pkgdatadir)
ui_DATA = phonemgr.ui

desktop_in_files = gnome-phone-manager.desktop.in
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)

#schema_in_files = gnome-phone-manager.schemas.in
#schemadir = $(GCONF_SCHEMA_FILE_DIR)
#schema_DATA = $(schema_in_files:.schemas.in=.schemas)

appdata_in_files = gnome-phone-manager.appdata.xml.in
appdatadir = $(datadir)/appdata
appdata_DATA = $(appdata_in_files:.xml.in=.xml)

CLEANFILES = \
	$(schema_DATA) \
	$(desktop_DATA) \
	$(appdata_DATA)

EXTRA_DIST = \
	$(ui_DATA) \
	$(schema_in_files) \
	$(desktop_in_files) \
	$(appdata_in_files) \
	$(man_MANS)

DISTCHECK_CONFIGURE_FLAGS == --enable-appdata-check

gnome-phone-manager.desktop: gnome-phone-manager.desktop.in
	$(AM_V_GEN)$(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

gnome-phone-manager.appdata.xml: gnome-phone-manager.appdata.xml.in
	$(AM_V_GEN)$(MSGFMT) --xml --template $< -d $(top_srcdir)/po -o $@


