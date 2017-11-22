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
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>

#include "app.h"

static void
tray_icon_activate_cb (GtkStatusIcon *status_icon, MyApp *app)
{
	/* if not popping up messages automatically, popup a
	   message on button 1 click */
	if (app->messages && (! app->popup_cb)) {
		dequeue_message (app);
	} else if (phonemgr_listener_connected (app->listener) != FALSE) {
		create_send_dialog (app, NULL, NULL);
	}
}

static void
tray_icon_popup_menu_cb (GtkStatusIcon *status_icon,
			 guint          button,
			 guint          activate_time,
			 MyApp         *app)
{
	gtk_menu_popup (GTK_MENU (app->menu), NULL, NULL,
			gtk_status_icon_position_menu, status_icon,
			button, activate_time);
}

void
set_icon_state (MyApp *app)
{
	if (app->listener && phonemgr_listener_connected (app->listener)) {
		gtk_widget_set_sensitive (app->send_item, TRUE);
		if (app->messages) {
			gtk_status_icon_set_from_icon_name (app->tray_icon, "phone-message");
			gtk_status_icon_set_tooltip_text (app->tray_icon, _("Message arrived"));
		} else {
			gtk_status_icon_set_from_icon_name (app->tray_icon, "phone");
			gtk_status_icon_set_tooltip_text (app->tray_icon, _("Connected"));
		}
	} else if (app->connecting) {
		gtk_status_icon_set_from_icon_name (app->tray_icon, "phone-connecting");
		gtk_status_icon_set_tooltip_text (app->tray_icon, _("Connecting to phone"));
		gtk_widget_set_sensitive (app->send_item, FALSE);
	} else {
		gtk_status_icon_set_from_icon_name (app->tray_icon, "phone-error");
		gtk_status_icon_set_tooltip_text (app->tray_icon, _("Not connected"));
		gtk_widget_set_sensitive (app->send_item, FALSE);
	}
}

void
tray_icon_init (MyApp *app)
{
	gtk_icon_theme_append_search_path (gtk_icon_theme_get_default (),
					   DATA_DIR G_DIR_SEPARATOR_S "icons");

	app->tray_icon = gtk_status_icon_new ();
	gtk_status_icon_set_title (app->tray_icon, _("Phone Manager"));
	gtk_window_set_default_icon_name ("phone");
	set_icon_state (app);

	g_signal_connect (G_OBJECT (app->tray_icon), "activate",
			  G_CALLBACK (tray_icon_activate_cb), (gpointer) app);

	g_signal_connect (G_OBJECT (app->tray_icon), "popup-menu",
			  G_CALLBACK (tray_icon_popup_menu_cb), (gpointer) app);
}

void
tray_icon_hide (MyApp *app)
{
	gtk_status_icon_set_visible (app->tray_icon, FALSE);
}

