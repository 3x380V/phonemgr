
AM_CPPFLAGS = \
	$(TELEPATHY_CFLAGS)				\
	-DGNOMELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATA_DIR="\"$(pkgdatadir)\""			\
	-DVERSION="\"$(VERSION)\""			\
	-DPACKAGE="\"$(PACKAGE)\""			\
	-I$(srcdir)/../libgsm				\
	-I$(builddir)/../libgsm				\
	$(WARN_CFLAGS)

BACKEND_FILES =				\
	connection.c			\
	connection.h			\
	connection-manager.c		\
	connection-manager.h		\
	debug.c				\
	debug.h				\
	im-channel.c			\
	im-channel-factory.c		\
	im-channel-factory.h		\
	im-channel.h			\
	main.c

MANAGER_FILE = phoney.manager
service_in_files = org.freedesktop.Telepathy.ConnectionManager.phoney.service.in

BUILT_FILES = org.freedesktop.Telepathy.ConnectionManager.phoney.service
EXTRA_DIST = $(BACKEND_FILES) $(MANAGER_FILE) $(service_in_files) sms.profile
CLEANFILES = $(BUILT_FILES)

if HAVE_TELEPATHY
libexec_PROGRAMS = telepathy-phoney

telepathy_phoney_SOURCES = $(BACKEND_FILES)

telepathy_phoney_LDADD =		\
	$(TELEPATHY_LIBS)		\
	$(PHONEMGR_LIBS)		\
	../libgsm/libgsmwrap.la

# Manager file
managerdir = $(datadir)/telepathy/managers
manager_DATA = $(MANAGER_FILE)

# D-Bus file
servicedir = $(datadir)/dbus-1/services
service_DATA = $(service_in_files:.service.in=.service)

# Rule to make the service file with libexecdir expanded
$(service_DATA): $(service_in_files) Makefile
	@sed -e "s|\@libexecdir\@|$(libexecdir)|" $< > $@

endif

