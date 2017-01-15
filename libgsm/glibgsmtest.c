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

#include <config.h>

#include <stdlib.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>

#include "phonemgr-listener.h"

GMainLoop *loop;

static void
message (PhonemgrListener *listener, const char *sender,
		time_t timestamp, const char *message)
{
	g_message ("Got message %s | %ld | %s", sender, (long)timestamp, message);
	g_main_loop_quit (loop);
}

static const char *statuses[] = {
	"PHONEMGR_LISTENER_IDLE",
	"PHONEMGR_LISTENER_CONNECTING",
	"PHONEMGR_LISTENER_CONNECTED",
	"PHONEMGR_LISTENER_DISCONNECTING",
	"PHONEMGR_LISTENER_ERROR"
};

static const char *call_statutes[] = {
	"PHONEMGR_LISTENER_CALL_INCOMING",
	"PHONEMGR_LISTENER_CALL_ONGOING",
	"PHONEMGR_LISTENER_CALL_IDLE",
	"PHONEMGR_LISTENER_CALL_UNKNOWN"
};

static const char *report_statuses[] = {
	"PHONEMGR_LISTENER_REPORT_DELIVERED",
	"PHONEMGR_LISTENER_REPORT_PENDING",
	"PHONEMGR_LISTENER_REPORT_FAILED_TEMPORARY",
	"PHONEMGR_LISTENER_REPORT_FAILED_PERMANENT"
};

static void
call_status (PhonemgrListener *listener, PhonemgrListenerCallStatus status, const char *phone, const char *name, gpointer user_data)
{
	g_message ("Got call status %s from %s (%s)", call_statutes[status], phone, name);
}

static void
battery (PhonemgrListener *l, int percent, gboolean on_ac, gpointer user_data)
{
	g_message ("battery is %d%%, on %s", percent, on_ac ? "mains" : "battery");
}

static void
status (PhonemgrListener *listener, gint status, gpointer user_data)
{
	g_message ("Got status %s (%d)", statuses[status], status);
}

static void
report_status (PhonemgrListener *l, char *phone, time_t timestamp, PhonemgrListenerReportStatus status, gpointer user_data)
{
	g_message ("Received delivery report status %s for %s", report_statuses[status], phone);
}

static void
network_status (PhonemgrListener *l, int mcc, int mnc, int lac, int cid)
{
	g_message ("Updated network: mcc: %d mnc: %d lac: %d cid: %d", mcc, mnc, lac, cid);
}

static gboolean
send_message (PhonemgrListener *l)
{
	phonemgr_listener_queue_message (l, "+447736665138",
					 "test message XXX", TRUE);

	return FALSE;
}

static gboolean debug = FALSE;
static gboolean list_all = FALSE;
static const char *get_uuid = NULL;
static const char *delete_uuid = NULL;
static const char *put_card = NULL;
static gboolean send_test_msg = FALSE;
static gboolean enter_main_loop = FALSE;
static const char *bdaddr = NULL;
static const char *data_type = NULL;

static const GOptionEntry entries[] = {
	{ "address", 'a', 0, G_OPTION_ARG_STRING, &bdaddr, "Address of the device to connect to", "PORT" },
	{ "verbose", 'v', 0, G_OPTION_ARG_NONE, &debug, "Whether to enable verbose output from libgnokii", NULL },
	{ "list-all-data", 'l',  0, G_OPTION_ARG_NONE, &list_all, "List all the PIM data", NULL },
	{ "get-data", 'g', 0, G_OPTION_ARG_STRING, &get_uuid, "Retrieve the PIM data with the given UUID", "UUID" },
	{ "delete-data", 'd', 0, G_OPTION_ARG_STRING, &delete_uuid, "Delete the PIM data with the given UUID", "UUID" },
	{ "put-data", 'p', 0, G_OPTION_ARG_FILENAME, &put_card, "Upload the given vCard file", "FILENAME" },
	{ "type", 't', 0, G_OPTION_ARG_STRING, &data_type, "Data type for the above functions. One of \"contact\", \"calendar\" and \"todo\"", "TYPE" },
	{ "send-msg", 's', 0, G_OPTION_ARG_NONE, &send_test_msg, "Send a test message", NULL },
	{ "main-loop", 0, 0, G_OPTION_ARG_NONE, &enter_main_loop, "Wait for signals", NULL },
	{ NULL }
};

int
main (int argc, char **argv)
{
	GOptionContext *context;
	GError *err = NULL;
	PhonemgrListener *listener;
	PhonemgrListenerDataType type;

	setlocale (LC_ALL, "");
	bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	context = g_option_context_new (_("- Manage your mobile phone"));
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

	g_type_init ();

	if (g_option_context_parse (context, &argc, &argv, &err) == FALSE) {
		g_printerr ("Couldn't parse command-line options: %s\n", err->message);
		g_error_free (err);
		return 1;
	}

	listener = phonemgr_listener_new (debug);

	g_signal_connect (G_OBJECT (listener), "message",
			G_CALLBACK (message), (gpointer) listener);
	g_signal_connect (G_OBJECT (listener), "status",
			G_CALLBACK (status), (gpointer) listener);
	g_signal_connect (G_OBJECT (listener), "call-status",
			  G_CALLBACK (call_status), (gpointer) listener);
	g_signal_connect (G_OBJECT (listener), "battery",
			  G_CALLBACK (battery), (gpointer) listener);
	g_signal_connect (G_OBJECT (listener), "report-status",
			  G_CALLBACK (report_status), (gpointer) listener);
	g_signal_connect (G_OBJECT (listener), "network",
			  G_CALLBACK (network_status), (gpointer) listener);

	if (bdaddr == NULL) {
		g_printerr ("Please use --address to pass a device address to connect to\n");
		return 1;
	}

	/* An action? Get the data type */
	if (list_all != FALSE || get_uuid != NULL || put_card != NULL || delete_uuid != NULL) {
		if (data_type == NULL) {
			g_printerr ("Please use --type to pass a type of data to manipulate\n");
			return 1;
		}
		if (g_str_equal (data_type, "contact") != FALSE) {
			type = PHONEMGR_LISTENER_DATA_CONTACT;
		} else if (g_str_equal (data_type, "calendar") != FALSE) {
			type = PHONEMGR_LISTENER_DATA_CALENDAR;
		} else if (g_str_equal (data_type, "todo") != FALSE) {
			type = PHONEMGR_LISTENER_DATA_TODO;
		} else {
			g_printerr ("Invalid data type passed. It must be one of \"contact\", \"calendar\" and \"todo\"\n");
			return 1;
		}
	}

	if (phonemgr_listener_connect (listener, bdaddr, &err)) {
		g_message ("Connected OK");

		if (send_test_msg != FALSE) {
			g_timeout_add_seconds (1, (GSourceFunc) send_message, listener);
			loop = g_main_loop_new (NULL, FALSE);
			g_main_loop_run (loop);
			g_main_loop_unref (loop);
		} else if (list_all != FALSE) {
			char **array;
			guint i;

			array = phonemgr_listener_list_all_data (listener, type);
			if (array == NULL) {
				g_printerr ("Failed to list all data of type '%s'\n", data_type);
				return 1;
			}
			for (i = 0; array[i] != NULL; i++)
				g_print ("UUID: %s\n", array[i]);

			g_strfreev (array);
		} else if (get_uuid != NULL) {
			char *vcard;

			vcard = phonemgr_listener_get_data (listener, type, get_uuid);
			if (vcard != NULL) {
				g_print ("%s\n", vcard);
				g_free (vcard);
			} else {
				g_printerr ("Failed to get data at location %s\n", get_uuid);
			}
		} else if (put_card != NULL) {
			char *contents;
			char *uuid;

			if (g_file_get_contents (put_card, &contents, NULL, &err) == FALSE) {
				g_printerr ("Getting the data from '%s' failed: %s\n", put_card, err->message);
				g_error_free (err);
				return 1;
			}

			uuid = phonemgr_listener_put_data (listener, type, contents);
			if (uuid != NULL) {
				g_print ("Added data at location '%s'\n", uuid);
				g_free (uuid);
			} else {
				g_printerr ("Failed to add data from '%s' to the device\n", put_card);
			}
		} else if (delete_uuid != NULL) {
			if (phonemgr_listener_delete_data (listener, type, delete_uuid) == FALSE) {
				g_printerr ("Failed to delete data at location '%s'\n", delete_uuid);
			}
		} else if (enter_main_loop != FALSE) {
			GMainLoop *loop;

			loop = g_main_loop_new (NULL, FALSE);
			g_main_loop_run (loop);
			g_main_loop_unref (loop);
		} else {
			g_message ("Nothing to do!");
		}

		phonemgr_listener_disconnect (listener);
	} else {
		g_printerr ("Couldn't connect to the phone: %s\n", err ? err->message : "No reason");
		if (err)
			g_error_free (err);
	}

	g_object_unref (listener);

	return 0;
}
