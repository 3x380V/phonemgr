/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2002-2004 Edd Dumbill <edd@usefulinc.com>
 * Copyright (C) 2005-2007 Bastien Nocera <hadess@hadess.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "phonemgr-object.h"
#include "phonemgr-utils.h"
#include "app.h"

static char *bdaddr_ident = NULL;
static char *bdaddr_config = NULL;
static gboolean debug = FALSE;
static gboolean version = FALSE;

const GOptionEntry options[] = {
	{ "identify", '\0', 0, G_OPTION_ARG_STRING, &bdaddr_ident, N_("Show model name of a specific device"), N_("PORT") },
	{ "config", '\0', 0, G_OPTION_ARG_STRING, &bdaddr_config, N_("Write the configuration file for gnokii debugging"), N_("PORT") },
	{ "debug", 'd', 0, G_OPTION_ARG_NONE, &debug, N_("Enable debug"), NULL},
	{ "version", 'v', 0, G_OPTION_ARG_NONE, &version, N_("Show version information and exit"), NULL },
	{ NULL }
};

int
main (int argc, char **argv)
{
	MyApp *app;
	GOptionContext *context;
	GError *error = NULL;

	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (N_("- Manage your mobile phone"));
	g_option_context_add_main_entries (context, options, GETTEXT_PACKAGE);
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);

	g_option_context_add_group (context, gtk_get_option_group (TRUE));
	if (g_option_context_parse (context, &argc, &argv, &error) == FALSE) {
		g_error ("Couldn't parse options: %s", error->message);
		g_error_free (error);
		g_option_context_free (context);
		return 1;
	}
	g_option_context_free (context);

	if (version != FALSE) {
		g_print (_("gnome-phone-manager version %s\n"), VERSION);
		return 0;
	}

	if (bdaddr_ident != NULL) {
		phonemgr_utils_tell_driver (bdaddr_ident);
		return 0;
	}
	if (bdaddr_config != NULL) {
		phonemgr_utils_write_gnokii_config (bdaddr_config);
		return 0;
	}

	gconf_init (argc, argv, NULL);

	app = g_new0 (MyApp, 1);

	/* Setup the D-Bus object */
	app->object = g_object_new (phonemgr_object_get_type (), NULL);

	app->client = gconf_client_get_default ();
	gconf_client_add_dir (app->client, CONFBASE,
			      GCONF_CLIENT_PRELOAD_ONELEVEL, NULL);

	/* Setup the listener */
	app->listener = phonemgr_listener_new (debug);

	initialise_connection (app);

	ui_init (app);
	construct_menu (app);
	tray_icon_init (app);

	reconnect_phone (app);
	gtk_main ();

	tray_icon_hide (app);
	ui_hide (app);
	free_connection (app);
	g_object_unref (app->listener);
	g_object_unref (app->client);
	g_object_unref (app->object);
	g_free (app);

	return 0;
}

