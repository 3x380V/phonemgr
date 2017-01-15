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

#include <glib/gi18n.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <canberra-gtk.h>
#include <time.h>
#include <string.h>
#include <bluetooth-chooser.h>
#include <bluetooth-chooser-button.h>

#include "app.h"
#include "e-phone-entry.h"
#include "gconf-bridge.h"
#include "phonemgr-utils.h"

#define MAX_MESSAGE_LENGTH 160

static char *
get_resource (MyApp *app, const char *uiresname)
{
	char *fname;

	fname = g_build_filename ("..", "data", uiresname, NULL);

	if (g_file_test (fname, G_FILE_TEST_EXISTS) == FALSE) {
		g_free (fname);
		fname = g_build_filename (DATA_DIR, uiresname, NULL);
	}

	if (g_file_test (fname, G_FILE_TEST_EXISTS) == FALSE) {
		g_free (fname);
		fname = NULL;
	}

	return fname;
}

static void
chooser_created (BluetoothChooserButton *button, BluetoothChooser *chooser, gpointer data)
{
	g_object_set(chooser,
		     "show-searching", FALSE,
		     "show-pairing", FALSE,
		     "show-device-type", FALSE,
		     "device-type-filter", PHONEMGR_DEVICE_TYPE_FILTER,
		     "show-device-category", FALSE,
		     "device-category-filter", BLUETOOTH_CATEGORY_PAIRED,
		     NULL);
}

static GtkBuilder *
get_ui (MyApp *app, const char *widget)
{
	char *fname;
	GError* error = NULL;
	GtkBuilder *ui;

	fname = get_resource (app, "phonemgr.ui");
	g_return_val_if_fail (fname, NULL);

	ui = gtk_builder_new ();

	if (widget != NULL) {
		char *widgets[] = { (char *)widget, NULL };

		if (!gtk_builder_add_objects_from_file (ui, fname, widgets, &error)) {
			g_warning ("Couldn't load builder file: %s", error->message);
			g_error_free (error);
		}
	} else {
		if (!gtk_builder_add_from_file (ui, fname, &error)) {
			g_warning ("Couldn't load builder file: %s", error->message);
			g_error_free (error);
		}
	}

	g_free (fname);

	return ui;
}

static gboolean
idle_play_alert (MyApp *app)
{
	if (gconf_client_get_bool (app->client, CONFBASE"/sound_alert", NULL)) {
		ca_context *ctx;
		ctx = ca_gtk_context_get ();
		ca_context_play (ctx, 0,
				 CA_PROP_EVENT_ID, "message-new-instant",
				 CA_PROP_EVENT_DESCRIPTION, _("New text message received"),
				 NULL);
	}
	return FALSE;
}

void
play_alert (MyApp *app)
{
	g_idle_add ((GSourceFunc) idle_play_alert, (gpointer) app);
}

static void
is_available_changed (BluetoothChooserButton *button, BluetoothChooser *chooser, gpointer data)
{
	gboolean is_sensitive;
	GtkWidget *btdevice = GTK_WIDGET (data);

	/* Make it sensitive only if the corresponding radio button is selected */
	is_sensitive = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (btdevice)) &&
	               phonemgr_utils_connection_is_supported (PHONEMGR_CONNECTION_BLUETOOTH) &&
	               bluetooth_chooser_button_available (button);
	gtk_widget_set_sensitive (GTK_WIDGET (button), is_sensitive);
}

static void
widget_set_dependent_sensitive (GtkWidget *widget, int conn_type, gboolean active)
{
	GtkWidget *dependent;

	dependent = GTK_WIDGET (g_object_get_data (G_OBJECT (widget), "dependent"));
	if (dependent == NULL)
		return;

	switch (conn_type) {
		case CONNECTION_BLUETOOTH:
			is_available_changed (BLUETOOTH_CHOOSER_BUTTON (dependent), NULL, widget);
			return;
		default:
			gtk_widget_set_sensitive (dependent, active);
			return;
	}
}

static void
on_conn_port_change (GtkWidget *widget, MyApp *app)
{
	gboolean active = gtk_toggle_button_get_active (
				GTK_TOGGLE_BUTTON (widget));
	int port;

	port = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (widget), "port"));

	widget_set_dependent_sensitive (widget, port, active);
	if (active) {
		gconf_client_set_int (app->client,
				      CONFBASE"/connection_type",
				      port, NULL);
	}
}

static gboolean
message_dialog_reply (GtkBuilder *ui)
{
	MyApp *app;
	GtkWidget *dialog = GTK_WIDGET (gtk_builder_get_object (ui, "sms_dialog"));
	GtkLabel *sender = GTK_LABEL (
			gtk_builder_get_object (ui, "senderlabel"));

	app = (MyApp *) g_object_get_data (G_OBJECT (dialog),
			"app");

	create_send_dialog (app, GTK_DIALOG (dialog),
			gtk_label_get_text (sender));

	return TRUE;
}

static gulong
s_connect (MyApp *app, const gchar *wname, const gchar *dependentname, gint port)
{
	GtkWidget *w, *dependent;

	w = GTK_WIDGET (gtk_builder_get_object (app->ui, wname));
	g_object_set_data (G_OBJECT (w), "port", GINT_TO_POINTER (port));
	if (dependentname) {
		dependent = GTK_WIDGET (gtk_builder_get_object (app->ui, dependentname));
		g_object_set_data (G_OBJECT (w), "dependent", dependent);
	}
	return g_signal_connect (G_OBJECT (w), "toggled",
				 G_CALLBACK (on_conn_port_change),
				 (gpointer)app);
}

static void
populate_prefs (MyApp *app)
{
	GtkWidget *w;
	const char *widgetname;
	int ctype;

	ctype = gconf_client_get_int (app->client,
				      CONFBASE"/connection_type", NULL);
	switch (ctype) {
	case CONNECTION_BLUETOOTH:
		widgetname = "btdevice";
		break;
	case CONNECTION_SERIAL1:
		widgetname = "serialport1";
		break;
	case CONNECTION_SERIAL2:
		widgetname = "serialport2";
		break;
	case CONNECTION_IRCOMM:
		widgetname = "irdaport";
		break;
	case CONNECTION_OTHER:
		/* fall through */
	default:
		widgetname = "otherport";
		break;
	}
	w = GTK_WIDGET (gtk_builder_get_object (app->ui, widgetname));
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (w), TRUE);
}

static void
apply_prefs (MyApp *app)
{
	reconnect_phone (app);
}

static gboolean
prefs_dialog_response (GtkWidget *dialog,
		       int response,
		       MyApp *app)
{
	apply_prefs (app);
	gtk_widget_hide (dialog);
	return TRUE;
}

static gboolean
destroy_send_dialog (GtkDialog *dialog, GtkBuilder *ui)
{
	g_object_unref (ui);
	gtk_widget_destroy (GTK_WIDGET (dialog));
	return TRUE;
}

static void
set_send_sensitivity (GtkWidget *w, GtkBuilder *ui)
{
	EPhoneEntry *entry;
	GtkTextView *view;
	GtkTextBuffer *buf;
	GtkWidget *sendbutton;
	GtkLabel *left;
	int l;
	char *work;
	char *text;

	left = GTK_LABEL (gtk_builder_get_object (ui, "charsleft"));
	view = GTK_TEXT_VIEW (gtk_builder_get_object (ui, "messagebody"));
	buf = gtk_text_view_get_buffer (view);
	entry = E_PHONE_ENTRY (gtk_builder_get_object (ui, "recipient"));
	sendbutton = GTK_WIDGET (gtk_builder_get_object (ui, "sendbutton"));

	l = gtk_text_buffer_get_char_count (buf);
	text = e_phone_entry_get_number (entry);

	gtk_widget_set_sensitive (sendbutton,
			l > 0 && l <= MAX_MESSAGE_LENGTH && text != NULL);
	g_free (text);

	if (l > MAX_MESSAGE_LENGTH) {
		gtk_label_set_text (left, _("Message too long!"));
	} else {
		int cl = MAX_MESSAGE_LENGTH - l;
		work = g_strdup_printf ("%d", cl);
		gtk_label_set_text (left, work);
		g_free (work);
	}
}

static void
phone_number_changed (GtkWidget *entry, char *phone_number, GtkBuilder *ui)
{
	set_send_sensitivity (entry, ui);
}

static void
send_message (GtkWidget *w, GtkBuilder *ui)
{
	GtkTextBuffer *buf;
	GtkTextView *view;
	EPhoneEntry *entry;
	GtkToggleButton *delivery_report;
	GtkDialog *dialog;
	GtkTextIter s, e;
	char *number;

	MyApp *app = (MyApp *) g_object_get_data (G_OBJECT (ui), "app");

	dialog = GTK_DIALOG (gtk_builder_get_object (ui, "send_dialog"));
	view = GTK_TEXT_VIEW (gtk_builder_get_object (ui, "messagebody"));
	buf = gtk_text_view_get_buffer (view);
	entry = E_PHONE_ENTRY (gtk_builder_get_object (ui, "recipient"));
	delivery_report = GTK_TOGGLE_BUTTON (gtk_builder_get_object (ui, "delivery_report"));

	gtk_text_buffer_get_start_iter (buf, &s);
	gtk_text_buffer_get_end_iter (buf, &e);
	number = e_phone_entry_get_number (entry);
	phonemgr_listener_queue_message (app->listener, number,
					 gtk_text_buffer_get_text (buf, &s, &e, FALSE),
					 gtk_toggle_button_get_active (delivery_report));
	g_free (number);

	gtk_widget_hide (GTK_WIDGET (dialog));
}

void
create_send_dialog (MyApp *app, GtkDialog *parent, const char *recip)
{
	GtkTextBuffer *buf;
	GtkTextView *view;
	GtkEntry *entry;
	GtkDialog *dialog;
	GtkBuilder *ui;
	GtkWidget *w;

	ui = get_ui (app, "send_dialog");

	dialog = GTK_DIALOG (gtk_builder_get_object (ui, "send_dialog"));

	if (parent) {
		gtk_window_set_transient_for (GTK_WINDOW (dialog),
			GTK_WINDOW (parent));
	}

	view = GTK_TEXT_VIEW (gtk_builder_get_object (ui, "messagebody"));
	buf = gtk_text_view_get_buffer (view);
	gtk_text_buffer_set_text (buf, "", 0);

	entry = GTK_ENTRY (gtk_builder_get_object (ui, "recipient"));
	if (recip)
		gtk_entry_set_text (entry, recip);
	else
		gtk_entry_set_text (entry, "");

	set_send_sensitivity (NULL, ui);

	g_object_set_data (G_OBJECT (ui), "app", (gpointer) app);

	/* hook up signals */
	g_signal_connect (G_OBJECT (dialog), "delete-event",
			  G_CALLBACK (gtk_widget_hide), (gpointer) dialog);

	g_signal_connect (G_OBJECT (dialog), "hide",
			  G_CALLBACK (destroy_send_dialog), (gpointer) ui);

	w = GTK_WIDGET (gtk_builder_get_object (ui, "msgcancelbutton"));
	g_signal_connect_swapped (G_OBJECT (w), "clicked",
				  G_CALLBACK (gtk_widget_hide), (gpointer) dialog);

	g_signal_connect (G_OBJECT (entry), "phone-changed",
			  G_CALLBACK (phone_number_changed), (gpointer) ui);
	g_signal_connect (G_OBJECT (buf), "changed",
			  G_CALLBACK (set_send_sensitivity), (gpointer) ui);

	w = GTK_WIDGET (gtk_builder_get_object (ui, "sendbutton"));
	g_signal_connect (G_OBJECT (w), "clicked",
			  G_CALLBACK (send_message), (gpointer) ui);

	gtk_widget_show_all (GTK_WIDGET (dialog));
}

static void
initialise_dequeuer (GConfClient *client, guint cnxn_id,
		GConfEntry *entry, MyApp *app)
{
	if (gconf_client_get_bool (app->client,
				CONFBASE"/popup_messages", NULL)) {
		app->popup_cb = g_timeout_add (250,
				(GSourceFunc) dequeue_message, (gpointer) app);
	} else if (app->popup_cb) {
			g_source_remove (app->popup_cb);
			app->popup_cb = 0;
	}
}

void
ui_init (MyApp *app)
{
	GtkWidget *btchooser, *btdevice;
	GConfBridge *bridge;
	GtkWidget *ep = e_phone_entry_new ();

	app->ui = get_ui (app, NULL);
	if (!app->ui)
		g_error ("Couldn't load user interface.");

	/* Close button and windowframe close button apply the settings
	   and hide the prefs panel */
	g_signal_connect (gtk_builder_get_object (app->ui, "prefs_dialog"),
			  "delete-event",
			  G_CALLBACK (prefs_dialog_response),
			  app);
	g_signal_connect (gtk_builder_get_object (app->ui, "prefs_dialog"),
			  "response",
			  G_CALLBACK (prefs_dialog_response),
			  app);

	/* Set Bluetooth options */
	btchooser = GTK_WIDGET (gtk_builder_get_object (app->ui, "btchooser"));
	btdevice = GTK_WIDGET (gtk_builder_get_object (app->ui, "btdevice"));
	g_signal_connect (btchooser,
			  "chooser-created",
			  G_CALLBACK (chooser_created),
			  NULL);
	g_signal_connect (btchooser,
			  "notify::is-available",
			  G_CALLBACK (is_available_changed),
			  btdevice);

	/* connect signal handlers for radio buttons */
	s_connect (app, "btdevice", "btchooser", CONNECTION_BLUETOOTH);
	s_connect (app, "serialport1", NULL, CONNECTION_SERIAL1);
	s_connect (app, "serialport2", NULL, CONNECTION_SERIAL2);
	s_connect (app, "irdaport", NULL, CONNECTION_IRCOMM);
	s_connect (app, "otherport", "otherportentry", CONNECTION_OTHER);

	/* Connect a few toggle buttons */
	bridge = gconf_bridge_get ();

	gconf_bridge_bind_property (bridge,
				    CONFBASE"/auto_retry",
				    G_OBJECT (gtk_builder_get_object (app->ui, "auto_retry")),
				    "active");
	gconf_bridge_bind_property (bridge,
				    CONFBASE"/sync_clock",
				    G_OBJECT (gtk_builder_get_object (app->ui, "sync_clock")),
				    "active");
	gconf_bridge_bind_property (bridge,
				    CONFBASE"/popup_messages",
				    G_OBJECT (gtk_builder_get_object (app->ui, "prefs_popup")),
				    "active");
	gconf_bridge_bind_property (bridge,
				    CONFBASE"/sound_alert",
				    G_OBJECT (gtk_builder_get_object (app->ui, "prefs_sound")),
				    "active");

	/* The other port address */
	gconf_bridge_bind_property_delayed (bridge,
				    CONFBASE"/other_serial",
				    G_OBJECT (gtk_builder_get_object (app->ui, "otherportentry")),
				    "text");

	/* And the address chooser */
	gconf_bridge_bind_property (bridge,
				    CONFBASE"/bluetooth_addr",
				    G_OBJECT (btchooser),
				    "device");

	/* set up popup on message */
	initialise_dequeuer (NULL, 0, NULL, app);
	gconf_client_notify_add (app->client,
			CONFBASE"/popup_messages",
			(GConfClientNotifyFunc) initialise_dequeuer,
			(gpointer) app,
			NULL, NULL);
}

void
ui_hide (MyApp *app)
{
	GtkWidget *w;
	GdkDisplay *display;

	w = GTK_WIDGET (gtk_builder_get_object (app->ui, "prefs_dialog"));
	display = gtk_widget_get_display (w);
	gtk_widget_hide (w);

	w = GTK_WIDGET (gtk_builder_get_object (app->ui, "send_dialog"));
	gtk_widget_hide (w);

	gdk_display_sync (display);
}

void
show_prefs_window (MyApp *app)
{
	GtkWidget *prefs = GTK_WIDGET (gtk_builder_get_object (app->ui, "prefs_dialog"));
	populate_prefs (app);
	gtk_window_present (GTK_WINDOW (prefs));
}

static char *
time_to_string (time_t time)
{
	char work[128];
	strftime (work, sizeof(work), "%c", gmtime (&time));
	return g_strdup (work);
}

gboolean
dequeue_message (MyApp *app)
{
	GList *ptr = NULL;
	Message *msg = NULL;
	GtkDialog *dialog;
	GtkTextView *textview;
	GtkLabel *l_sender, *l_sent;
	GtkTextBuffer *buf;
	char *time;
	GtkBuilder *ui;
	GtkWidget *w;

	g_mutex_lock (&app->message_mutex);
	ptr = g_list_first (app->messages);
	if (ptr) {
		app->messages = g_list_remove_link (app->messages, ptr);
	}
	g_mutex_unlock (&app->message_mutex);

	if (! ptr) {
		return TRUE;
	}

	/* time to get on with displaying it */
	msg = (Message *) ptr->data;

	ui = get_ui (app, "sms_dialog");

	w = GTK_WIDGET (gtk_builder_get_object (ui, "sms_dialog"));
	g_signal_connect_swapped (G_OBJECT (w), "delete-event",
			G_CALLBACK (gtk_widget_destroy), (gpointer) w);

	g_object_set_data (G_OBJECT (w), "app", (gpointer) app);

	dialog = GTK_DIALOG (w);

	w = GTK_WIDGET (gtk_builder_get_object (ui, "okbutton"));
	g_signal_connect_swapped (G_OBJECT (w), "clicked",
			G_CALLBACK (gtk_widget_destroy), (gpointer) dialog);

	w = GTK_WIDGET (gtk_builder_get_object (ui, "replybutton"));
	g_signal_connect_swapped (G_OBJECT (w), "clicked",
			G_CALLBACK (message_dialog_reply), (gpointer) ui);

	l_sender = GTK_LABEL (gtk_builder_get_object (ui, "senderlabel"));
	l_sent = GTK_LABEL (gtk_builder_get_object (ui, "datelabel"));
	textview = GTK_TEXT_VIEW (gtk_builder_get_object (ui, "messagecontents"));

	buf = gtk_text_view_get_buffer (textview);
	gtk_text_buffer_set_text (buf, msg->message, strlen (msg->message));
	gtk_label_set_text (l_sender, msg->sender);

	time = time_to_string (msg->timestamp);
	gtk_label_set_text (l_sent, time);
	g_free (time);

	gtk_widget_show_all (GTK_WIDGET (dialog));

	g_free (msg->sender);
	g_free (msg->message);
	g_free (msg);
	g_list_free_1 (ptr);

	set_icon_state (app);

	return TRUE;
}

