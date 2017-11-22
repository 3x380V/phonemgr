/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2007 Bastien Nocera <hadess@hadess.net>
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

#include <gtk/gtk.h>
#include <string.h>
#include "e-phone-entry.h"

#define CONTACT_FORMAT "%s (%s)"

static char *phone_number = NULL;
static GtkWidget *activate;

static void
phone_changed_cb (EPhoneEntry *pentry, const char *number, gpointer data)
{
	g_free (phone_number);
	phone_number = g_strdup (number);

	g_message ("phone number: %s", number);

	gtk_widget_set_sensitive (activate, (number != NULL));
}

int main (int argc, char **argv)
{
	GtkWidget *window, *entry;

	gtk_init (&argc, &argv);

	window = gtk_dialog_new ();
	activate = gtk_dialog_add_button (GTK_DIALOG (window), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_widget_set_sensitive (activate, FALSE);
	entry = e_phone_entry_new ();
	g_signal_connect (G_OBJECT (entry), "phone_changed",
			G_CALLBACK (phone_changed_cb), NULL);

	gtk_container_add (GTK_CONTAINER (gtk_dialog_get_content_area (GTK_DIALOG (window))), entry);

	gtk_widget_show_all (window);

	gtk_dialog_run (GTK_DIALOG (window));
	g_print ("Phone number selected: %s\n", phone_number ? phone_number : "None");

	return 0;
}

