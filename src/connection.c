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

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "app.h"

static gboolean
set_connection_device (MyApp *app)
{
	int ctype;
	char *dev = NULL;
	gboolean changed;

	ctype = gconf_client_get_int (app->client,
				      CONFBASE"/connection_type", NULL);
	changed = FALSE;

	switch (ctype) {
		case CONNECTION_BLUETOOTH:
			dev = gconf_client_get_string (app->client,
						       CONFBASE"/bluetooth_addr",
						       NULL);
			if (!dev || strlen (dev) != 17) {
				g_free (dev);
				dev = NULL;
			}
			break;
		case CONNECTION_SERIAL1:
			dev = g_strdup ("/dev/ttyS0");
			break;
		case CONNECTION_SERIAL2:
			dev = g_strdup ("/dev/ttyS1");
			break;
		case CONNECTION_IRCOMM:
			dev = g_strdup ("/dev/ircomm0");
			break;
		case CONNECTION_OTHER:
			dev = gconf_client_get_string (app->client,
					CONFBASE"/other_serial", NULL);
			break;
	}

	if (dev == NULL && app->devname != NULL)
		changed = TRUE;
	else if (dev != NULL && app->devname == NULL)
		changed = TRUE;
	else if (dev != NULL && app->devname != NULL && strcmp (dev, app->devname) != 0)
		changed = TRUE;

	g_free (app->devname);
	app->devname = dev;

	g_message ("New connection device is %s (%s)",
		   dev ? dev : "empty",
		   changed ? "changed" : "not changed");

	return changed;
}

static gboolean
attempt_reconnect (MyApp *app)
{
	if (gconf_client_get_bool (app->client,
				CONFBASE"/auto_retry", NULL) &&
			phonemgr_listener_connected (app->listener) == FALSE &&
			app->connecting == FALSE) {
		g_message ("Auto-retrying the connection");
		reconnect_phone (app);
	}
	return TRUE;
}

static gboolean
sync_clock (MyApp *app)
{
	if (gconf_client_get_bool (app->client,
				CONFBASE"/sync_clock", NULL)) {
		g_message ("Syncing phone clock");
		phonemgr_listener_set_time (app->listener,
					    time(NULL));
	}
	return TRUE;
}

static gpointer
connect_phone_thread (gpointer data)
{
	MyApp *app = (MyApp *)data;
	GError *err = NULL;

	gdk_threads_enter ();
	set_icon_state (app);
	gdk_threads_leave ();
	set_connection_device (app);
	app->reconnect = FALSE;
	if (app->devname) {
		g_message ("Connecting...");
		if (phonemgr_listener_connect (app->listener, app->devname, &err)) {
			/* translators: the '%s' will be substituted with '/dev/ttyS0'
			   or similar */
			g_message (_("Connected to device on %s"), app->devname);
		} else {
			/* the ERROR signal will have been emitted, so we don't
			   bother changing the icon ourselves at this point */
			/* translators: the '%s' will be substituted with '/dev/ttyS0'
			   or similar */
			g_message (_("Failed connection to device on %s"), app->devname);
			if (err != NULL)
				g_message ("Error message is %s", err->message);
		}
	} else {
		g_message ("No device!");
	}
	g_mutex_lock (&app->connecting_mutex);
	app->connecting = FALSE;
	g_mutex_unlock (&app->connecting_mutex);

	gdk_threads_enter ();
	set_icon_state (app);
	gdk_threads_leave ();

	g_message ("Exiting connect thread");
	return NULL;
}

static void
connect_phone (MyApp *app)
{
	g_mutex_lock (&app->connecting_mutex);
	if (phonemgr_listener_connected (app->listener) == FALSE
	    && app->connecting == FALSE) {
		app->connecting = TRUE;
		g_mutex_unlock (&app->connecting_mutex);
		/* we're neither connected, nor connecting */
		app->connecting_thread = g_thread_create (connect_phone_thread,
							  (gpointer) app,
							  FALSE, NULL);
	} else {
		g_message ("Can't connect twice");
		g_mutex_unlock (&app->connecting_mutex);
	}
}

static gboolean
idle_connect_phone (MyApp *app)
{
	connect_phone (app);
	return FALSE;
}

static void
on_status (PhonemgrListener *listener, int status, MyApp *app)
{
	gdk_threads_enter ();

	app->status = status;

	switch (status) {
		case PHONEMGR_LISTENER_IDLE:
			g_message ("Disconnect complete");
			set_icon_state (app);
			if (app->reconnect)
				g_idle_add ((GSourceFunc)idle_connect_phone,
						(gpointer)app);
			break;
		case PHONEMGR_LISTENER_CONNECTING:
			g_message ("Making serial port connection");
			break;
		case PHONEMGR_LISTENER_CONNECTED:
			g_message ("Serial port connected");
			phonemgr_object_emit_number_batteries_changed (app->object, 1);
			sync_clock (app);
			break;
		case PHONEMGR_LISTENER_DISCONNECTING:
			g_message ("Closing serial port connection");
			phonemgr_object_emit_number_batteries_changed (app->object, 0);
			phonemgr_object_emit_network_registration_changed (app->object, 0, 0, 0, 0);
			break;
		case PHONEMGR_LISTENER_ERROR:
			set_icon_state (app);
			g_message ("Connection error occurred");
			break;
		default:
			g_message ("Unhandled status %d", status);
			break;
	}

	gdk_threads_leave ();
}

static void
on_message (PhonemgrListener *listener, char *sender,
		time_t timestamp, char *message, MyApp *app)
{
	Message *msg;

	/* this handler called from a normal poll: happens
	   on the main gtk thread */
	g_message ("Got message %s | %ld | %s", sender, (long)timestamp, message);
	msg = g_new0 (Message, 1);
	msg->sender = g_strdup (sender);
	msg->timestamp = timestamp;
	msg->message = g_strdup (message);

	g_mutex_lock (&app->message_mutex);
	app->messages = g_list_append (app->messages, (gpointer) msg);
	g_mutex_unlock (&app->message_mutex);

	set_icon_state (app);
	play_alert (app);
}

static void
on_battery (PhonemgrListener *listener, int percent, gboolean on_ac, MyApp *app)
{
	phonemgr_object_emit_battery_state_changed (app->object, 0, percent, on_ac);
}

static void
on_network (PhonemgrListener *listener, int mcc, int mnc, int lac, int cid, MyApp *app)
{
	phonemgr_object_emit_network_registration_changed (app->object, mcc, mnc, lac, cid);
}

void
reconnect_phone (MyApp *app)
{
	gboolean changed;

	changed = set_connection_device (app);

	if (changed && phonemgr_listener_connected (app->listener) != FALSE) {
		g_message ("Disconnecting...");
		app->reconnect = TRUE;
		app->disconnecting_thread =
			g_thread_create ((GThreadFunc) phonemgr_listener_disconnect,
				(gpointer) app->listener, TRUE, NULL);
	} else {
		connect_phone (app);
	}
}

void
free_connection (MyApp *app)
{
	g_source_remove (app->reconnector);
	if (phonemgr_listener_connected (app->listener))
		phonemgr_listener_disconnect (app->listener);
}

void
disconnect_signal_handlers (MyApp *app)
{
	/* disconnect signal handlers, or they'll become blocked on
	gdk_threads_enter: called when menu-item Quit is activated */
	g_signal_handler_disconnect ((gpointer) app->listener,
			app->status_cb);
	g_signal_handler_disconnect ((gpointer) app->listener,
			app->message_cb);
}

void
initialise_connection (MyApp *app)
{
	app->status_cb = g_signal_connect (G_OBJECT (app->listener), "status",
			G_CALLBACK (on_status), (gpointer) app);
	app->message_cb = g_signal_connect (G_OBJECT (app->listener), "message",
			G_CALLBACK (on_message), (gpointer) app);
	app->battery_cb = g_signal_connect (G_OBJECT (app->listener), "battery",
						G_CALLBACK (on_battery), (gpointer) app);
	app->network_cb = g_signal_connect (G_OBJECT (app->listener), "network",
					    G_CALLBACK (on_network), app);
	app->reconnector = g_timeout_add_seconds (20, (GSourceFunc)attempt_reconnect,
						  (gpointer)app);

	gdk_threads_init ();
	g_mutex_init (&app->connecting_mutex);
	g_mutex_init (&app->message_mutex);
}

