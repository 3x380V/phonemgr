/*
 *
 *  Phone Manager plugin for GNOME Bluetooth
 *
 *  Copyright (C) 2009  Bastien Nocera <hadess@hadess.net>
 *  Copyright (C) 2011  Daniele Forsi <daniele@forsi.it>
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <gconf/gconf-client.h>

#include <bluetooth-plugin.h>
#include <bluetooth-client.h>

#include "phonemgr-conf.h"

static BluetoothType
get_type (const char *address)
{
	BluetoothClient *client;
	BluetoothType type = BLUETOOTH_TYPE_ANY;
	GtkTreeModel *model;
	GtkTreeIter iter;
	gboolean cont;

	client = bluetooth_client_new ();
	model = bluetooth_client_get_device_model (client);
	if (model == NULL) {
		g_object_unref (client);
		return type;
	}

	for (cont = gtk_tree_model_get_iter_first (model, &iter);
	     cont != FALSE;
	     cont = gtk_tree_model_iter_next (model, &iter)) {
		char *bdaddr;

		gtk_tree_model_get (model, &iter,
				    BLUETOOTH_COLUMN_ADDRESS, &bdaddr,
				    -1);
		if (g_strcmp0 (bdaddr, address) == 0) {
			g_free (bdaddr);
			gtk_tree_model_get (model, &iter,
					    BLUETOOTH_COLUMN_TYPE, &type,
					    -1);
			break;
		}
		g_free (bdaddr);
	}

	g_object_unref (model);
	g_object_unref (client);

	return type;
}

static gboolean
has_config_widget (const char *bdaddr, const char **uuids)
{
	gboolean has_sp;
	BluetoothType type;
	guint i;

	if (uuids == NULL)
		return FALSE;
	for (i = 0; uuids[i] != NULL; i++) {
		if (g_str_equal (uuids[i], "SerialPort")) {
			has_sp = TRUE;
			break;
		}
	}
	if (has_sp == FALSE)
		return FALSE;

	type = get_type (bdaddr);

	return type & PHONEMGR_DEVICE_TYPE_FILTER;
}

static void
toggle_button (GtkToggleButton *button, gpointer user_data)
{
	gboolean state;
	GConfClient *client;
	const char *bdaddr;

	client = g_object_get_data (G_OBJECT (button), "client");
	bdaddr = g_object_get_data (G_OBJECT (button), "bdaddr");

	state = gtk_toggle_button_get_active (button);
	if (state == FALSE) {
		gconf_client_set_string (client, CONFBASE"/bluetooth_addr", "", NULL);
		gconf_client_set_int (client, CONFBASE"/connection_type", CONNECTION_NONE, NULL);
	} else {
		gconf_client_set_string (client, CONFBASE"/bluetooth_addr", bdaddr, NULL);
		gconf_client_set_int (client, CONFBASE"/connection_type", CONNECTION_BLUETOOTH, NULL);
	}
}

static GtkWidget *
get_config_widgets (const char *bdaddr, const char **uuids)
{
	GtkWidget *button;
	GConfClient *client;
	char *old_bdaddr;
	int connection_type;

	client = gconf_client_get_default ();
	if (client == NULL)
		return NULL;

	/* Translators: "device" is a phone or a modem */
	button = gtk_check_button_new_with_label (_("Use this device with Phone Manager"));
	g_object_set_data_full (G_OBJECT (button), "bdaddr", g_strdup (bdaddr), g_free);
	g_object_set_data_full (G_OBJECT (button), "client", client, g_object_unref);

	/* Is it already setup? */
	old_bdaddr = gconf_client_get_string (client, CONFBASE"/bluetooth_addr", NULL);
	connection_type = gconf_client_get_int (client, CONFBASE"/connection_type", NULL);
	if (connection_type == CONNECTION_BLUETOOTH && old_bdaddr && g_strcmp0 (old_bdaddr, bdaddr) == 0) {
		gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		g_object_set_data (G_OBJECT (button), "bdaddr", old_bdaddr);
	} else {
		g_free (old_bdaddr);
	}

	/* And set the signal */
	g_signal_connect (G_OBJECT (button), "toggled",
			  G_CALLBACK (toggle_button), NULL);

	return button;
}

static void
device_removed (const char *bdaddr)
{
	GConfClient *client;
	char *str;

	client = gconf_client_get_default ();
	if (client == NULL)
		return;

	str = gconf_client_get_string (client, CONFBASE"/bluetooth_addr", NULL);
	if (g_strcmp0 (str, bdaddr) == 0) {
		gconf_client_set_string (client, CONFBASE"/bluetooth_addr", "", NULL);
		if (gconf_client_get_int (client, CONFBASE"/connection_type", NULL) == CONNECTION_BLUETOOTH) {
			gconf_client_set_int (client, CONFBASE"/connection_type", CONNECTION_NONE, NULL);
		}
		g_debug ("Device '%s' got disabled for use with Phone Manager", bdaddr);
	}

	g_free (str);
	g_object_unref (client);
}

static GbtPluginInfo plugin_info = {
	"phonemgr",
	has_config_widget,
	get_config_widgets,
	device_removed
};

GBT_INIT_PLUGIN(plugin_info)

