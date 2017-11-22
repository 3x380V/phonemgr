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

#ifndef _APP_H
#define _APP_H

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include "phonemgr-listener.h"
#include "phonemgr-object.h"

typedef struct _appinfo {
	/* gui stuff */
	GtkStatusIcon *tray_icon;
	GtkMenu *menu;
	GtkBuilder    *ui;
	int    iconstate;
	int    chars;
	GtkWidget *send_item;
	gboolean flashon;

	/* auxilliary controllers */
	GConfClient *client;
	PhonemgrListener    *listener;
	PhonemgrObject      *object;

	/* connection state */
	char   *devname;
	gboolean    reconnect;

	/* messages */
	GMutex  *message_mutex;
	GList   *messages;

	/* thread stuff for connecting and disconnecting */
	GThread *connecting_thread;
	GThread *disconnecting_thread;
	GMutex *connecting_mutex;
	gboolean connecting;
	gint status;

	/* signal handlers and timeouts */
	gulong  status_cb;
	gulong  message_cb;
	gulong  reconnector;
	gulong  popup_cb;
	gulong  battery_cb;
	gulong  network_cb;
} MyApp;

typedef struct _message {
	char   *sender;
	time_t   timestamp;
	char   *message;
} Message;

/* menu functions */
void construct_menu (MyApp *app);

/* ui functions */
void ui_init (MyApp *app);
void show_prefs_window (MyApp *app);
gboolean dequeue_message (MyApp *app);
void create_send_dialog (MyApp *app, GtkDialog *parent, const char *recip);
void enable_flasher (MyApp *app);
void play_alert (MyApp *app);
void ui_hide (MyApp *app);
GtkWidget *bluetooth_chooser_button_create (void);

/* connection functions */
void free_connection (MyApp *app);
void disconnect_signal_handlers (MyApp *app);
void initialise_connection (MyApp *app);
void reconnect_phone (MyApp *app);

/* icon functions */
void set_icon_state (MyApp *app);
void tray_icon_init (MyApp *app);
void tray_icon_hide (MyApp *app);

#include "phonemgr-conf.h"

#endif
