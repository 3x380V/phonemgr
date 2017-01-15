AC_PREREQ(2.63)
AC_INIT([GNOME Phone Manager], [0.69], [http://bugzilla.gnome.org/enter_bug.cgi?product=gnome-phone-manager], [gnome-phone-manager])

AC_CONFIG_HEADERS(config.h)
AC_CONFIG_SRCDIR(src/main.c)
AC_CONFIG_MACRO_DIR([m4])

AC_SUBST([PACKAGE_NAME], ["$PACKAGE_NAME"])
AC_SUBST([PACKAGE_VERSION], ["$PACKAGE_VERSION"])

AX_CHECK_ENABLE_DEBUG([yes],[GNOME_ENABLE_DEBUG])

AM_MAINTAINER_MODE
AM_INIT_AUTOMAKE([1.11 no-dist-gzip dist-xz check-news])
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

GNOKII_REQUIRED=0.6.28
GTK_REQUIRED=3.0
GLIB_REQUIRED=2.32.0
GNOME_ICON_THEME_REQUIRED=2.19.1
GNOME_BLUETOOTH_REQUIRED=3.3

# Initialize libtool
LT_PREREQ([2.2.6])
LT_INIT([disable-static])

GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
		   [GNOME phone manager gettext domain])

AX_REQUIRE_DEFINED([AM_GNU_GETTEXT_VERSION])
AM_GNU_GETTEXT_VERSION([0.19.8])
		
AX_REQUIRE_DEFINED([AM_GNU_GETTEXT])
AM_GNU_GETTEXT([external])

# ****
AC_PATH_PROG(GCONFTOOL, gconftool-2)

dnl Evolution address book
PKG_CHECK_MODULES(TMP_EVO, libebook-1.2,
		evo_pc_file="libebook-1.2",
		evo_pc_file="libebook-1.0")

PKG_CHECK_MODULES(LIBGSM, glib-2.0 gobject-2.0 gnokii >= $GNOKII_REQUIRED gthread-2.0 bluez $evo_pc_file)

PKG_CHECK_MODULES(PHONEMGR, gtk+-3.0 >= $GTK_REQUIRED glib-2.0 >= $GLIB_REQUIRED
	libcanberra-gtk3 gconf-2.0
	gnome-bluetooth-1.0 >= $GNOME_BLUETOOTH_REQUIRED $evo_pc_file
	gmodule-2.0 gthread-2.0 dbus-glib-1 gnome-icon-theme >= $GNOME_ICON_THEME_REQUIRED
)

DBUSLIBDIR="`$PKG_CONFIG dbus-glib-1 --variable=libdir`"
DBUSBINDIR="`echo $DBUSLIBDIR | sed -e s/lib/bin/`"
AC_PATH_PROG([DBUS_BIND], [dbus-binding-tool], [no], [$DBUSBINDIR:$PATH])
if test "x$DBUS_BIND" = "xno"; then
	AC_MSG_WARN([dbus-binding-tool not found])
fi

saved_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $TMP_EVO_CFLAGS"
AC_CHECK_TYPES([EContactPhotoType], , , [#include <libebook/e-contact.h>])
CPPFLAGS="$saved_CPPFLAGS"

AC_SUBST(PHONEMGR_CFLAGS)
AC_SUBST(PHONEMGR_LIBS)

dnl Check for telepathy property page build
AC_ARG_ENABLE(telepathy,
	AC_HELP_STRING([--enable-telepathy],[compile the telepathy SMS plugin]),
	[case "${enableval}" in
	yes) ENABLE_TELEPATHY=yes ;;
	no) ENABLE_TELEPATHY=no ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-telepathy) ;;
	esac],
	[ENABLE_TELEPATHY=yes]) dnl Default value

if test x$ENABLE_TELEPATHY = "xyes" ; then
	PKG_CHECK_MODULES(TELEPATHY, telepathy-glib dbus-glib-1,
			[HAVE_TELEPATHY=yes], [HAVE_TELEPATHY=no])
fi
if test x$HAVE_TELEPATHY = "xyes"; then
	AC_DEFINE(HAVE_TELEPATHY, 1, [defined if you build the telepathy SMS plugin])
fi
AM_CONDITIONAL(HAVE_TELEPATHY, test x$HAVE_TELEPATHY = "xyes")

dnl Check for gnome-bluetooth plugin build
AC_ARG_ENABLE(bluetooth-plugin,
	AC_HELP_STRING([--enable-bluetooth-plugin],[compile the gnome-bluetooth plugin]),
	[case "${enableval}" in
	yes) ENABLE_BLUETOOTH_PLUGIN=yes ;;
	no) ENABLE_BLUETOOTH_PLUGIN=no ;;
	*) AC_MSG_ERROR(bad value ${enableval} for --enable-bluetooth-plugin) ;;
	esac],
	[ENABLE_BLUETOOTH_PLUGIN=yes]) dnl Default value

if test x$ENABLE_BLUETOOTH_PLUGIN = "xyes" ; then
	PKG_CHECK_MODULES(BLUETOOTH_PLUGIN, gnome-bluetooth-1.0 >= 2.27.6 gnome-bluetooth-1.0 < 3.11.0 gconf-2.0,
			[HAVE_BLUETOOTH_PLUGIN=yes], [HAVE_BLUETOOTH_PLUGIN=no])
fi
if test x$HAVE_BLUETOOTH_PLUGIN = "xyes"; then
	AC_DEFINE(HAVE_BLUETOOTH_PLUGIN, 1, [Defined to 1 if you build the gnome-bluetooth plugin.])
fi
AM_CONDITIONAL(HAVE_BLUETOOTH_PLUGIN, test x$HAVE_BLUETOOTH_PLUGIN = "xyes")

AC_PATH_PROG(GLIB_GENMARSHAL, glib-genmarshal, no)
AC_SUBST(GLIB_GENMARSHAL)

AC_PATH_PROG(GCONFTOOL, gconftool-2, no)

if test x"$GCONFTOOL" = xno; then
  AC_MSG_ERROR([gconftool-2 executable not found in your path - should be installed with GConf])
fi

dnl ================================================================
dnl set up data dir
if test "x${datadir}" = 'x${prefix}/share'; then
  if test "x${prefix}" = "xNONE"; then
    AC_DEFINE_UNQUOTED(PHONEMGR_DATA_DIR, "${ac_default_prefix}/share/${PACKAGE}", [data dir])
  else
    AC_DEFINE_UNQUOTED(PHONEMGR_DATA_DIR, "${prefix}/share/${PACKAGE}", [data dir])
  fi
else
  AC_DEFINE_UNQUOTED(PHONEMGR_DATA_DIR, "${datadir}/${PACKAGE}", [data dir])
fi

AC_CONFIG_FILES([
Makefile
data/Makefile
data/icons/Makefile
data/icons/16x16/status/Makefile
data/icons/16x16/Makefile
data/icons/22x22/status/Makefile
data/icons/22x22/Makefile
data/icons/24x24/status/Makefile
data/icons/24x24/Makefile
libgsm/Makefile
cut-n-paste/Makefile
cut-n-paste/e-contact-entry/Makefile
cut-n-paste/gconf-bridge/Makefile
src/Makefile
gnome-bluetooth/Makefile
telepathy/Makefile
po/Makefile.in
])

AC_OUTPUT

