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

#include "app.h"

static gboolean
prefs_activated (GtkMenuItem *item, MyApp *app)
{
	show_prefs_window (app);
	return TRUE;
}

static gboolean
about_activated(GtkMenuItem *item, gpointer data)
{
	const char *authors[] = { "Bastien Nocera <hadess@hadess.net>", "Edd Dumbill <edd@usefulinc.com>", NULL };
	const char *documenters[] = { NULL };

	gtk_show_about_dialog (NULL,
			       "authors", authors,
			       "comments", _("Send and receive messages from your mobile phone."),
			       "copyright", "Copyright \xc2\xa9 2003-2004 Edd Dumbill\n" \
			                    "Copyright \xc2\xa9 2005-2010 Bastien Nocera\n" \
			                    "Copyright \xc2\xa9 2011 Daniele Forsi",
			       "documenters", documenters,
			       "logo-icon-name", "phone",
			       "program-name", _("Phone Manager"),
			       "version", VERSION,
			       "translator-credits", _("translator_credits"),
			       "website", "http://live.gnome.org/PhoneManager",
			       "website-label", _("Phone Manager website"),
			       NULL);

	return TRUE;
}

static gboolean
quit_activated (GtkMenuItem *item, gpointer data)
{
	MyApp *app = (MyApp *)data;
	disconnect_signal_handlers (app);
	gtk_main_quit ();
	return TRUE;
}

static gboolean
send_activated (GtkMenuItem *item, gpointer data)
{
	MyApp *app = (MyApp *)data;

	create_send_dialog (app, NULL, NULL);
	return TRUE;
}

void
construct_menu (MyApp *app)
{
	GtkWidget *item;

	app->menu = GTK_MENU (gtk_menu_new ());

	item = gtk_image_menu_item_new_with_mnemonic (_("_Send Message"));
	g_signal_connect (G_OBJECT(item), "activate",
			  G_CALLBACK (send_activated), (gpointer) app);
	gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item),
				       GTK_WIDGET (gtk_image_new_from_icon_name ("mail-message-new",
										 GTK_ICON_SIZE_MENU)));
	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL (app->menu), item);
	app->send_item = item;

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_PREFERENCES,
						   NULL);
	g_signal_connect (G_OBJECT(item), "activate",
			  G_CALLBACK (prefs_activated), (gpointer) app);

	gtk_widget_show (item);
	gtk_menu_shell_append(GTK_MENU_SHELL(app->menu), item);

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_ABOUT, NULL);

	g_signal_connect (G_OBJECT(item), "activate",
			  G_CALLBACK (about_activated), (gpointer) app);

	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL(app->menu), item);

	item = gtk_separator_menu_item_new ();

	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL(app->menu), item);

	item = gtk_image_menu_item_new_from_stock (GTK_STOCK_QUIT, NULL);
	g_signal_connect (G_OBJECT(item), "activate",
			  G_CALLBACK (quit_activated), (gpointer) app);

	gtk_widget_show (item);
	gtk_menu_shell_append (GTK_MENU_SHELL(app->menu), item);
}

