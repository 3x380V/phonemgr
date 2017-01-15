/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Bastien Nocera <hadess@hadess.net>
 *
 * e-phone-entry.c
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
 * Authors: Bastien Nocera <hadess@hadess.net>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <glib.h>

#include <gtk/gtk.h>
#include <string.h>
#include <libedataserver/libedataserver.h>
#include "e-phone-entry.h"

#define CONTACT_FORMAT "%s (%s)"

/* Signals */
enum {
	PHONE_CHANGED, /* Signal argument is the phone number */
	LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE(EPhoneEntry, e_phone_entry, E_TYPE_CONTACT_ENTRY);

static char *
cleanup_number (const char *str)
{
	GString *res;
	char *p;

	p = (char *) str;
	res = g_string_new ("");
	while (*p != '\0') {
		gunichar c;
		c = g_utf8_get_char (p);
		if (g_unichar_isdigit (c) ||
				c == '+') {
			res = g_string_append_unichar (res, c);
		}
		p = g_utf8_next_char(p);
	}

	return g_string_free (res, FALSE);
}

static void
emit_changed_signal (EPhoneEntry *pentry, const char *phone_number)
{
	g_signal_emit (G_OBJECT (pentry),
			signals[PHONE_CHANGED], 0, phone_number);
}

static void
text_changed (GtkEditable *entry, gpointer user_data)
{
	EPhoneEntry *pentry = E_PHONE_ENTRY (entry);
	char *current;
	char *p;

	current = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));

	if (pentry->text != NULL && strcmp (pentry->text, current) == 0) {
		g_free (current);
		return;
	}

	g_free (pentry->text);
	pentry->text = current;

	if (pentry->phone_number != NULL) {
		g_free (pentry->phone_number);
		pentry->phone_number = NULL;
	}
	if (g_str_equal (pentry->text, "") != FALSE) {
		emit_changed_signal (pentry, NULL);
		return;
	}

	p = current;
	while (*p != '\0') {
		gunichar c;
		c = g_utf8_get_char_validated (p, -1);
		/* We only allow digits, plus signs, spaces and dashes
		 * in user supplied phone numbers. */
		if (g_unichar_isdigit (c) == FALSE
				&& g_unichar_isspace (c) == FALSE
				&& c != 0x2B /* '+' */
				&& c != 0x2D /* '-' */) {
			emit_changed_signal (pentry, NULL);
			return;
		}
		p = g_utf8_next_char (p);
	}

	/* Remove spaces from the phone number */
	pentry->phone_number = cleanup_number (current);
	emit_changed_signal (pentry, pentry->phone_number);
}

static void
contact_selected_cb (GtkWidget *entry, EContact *contact, const char *identifier)
{
	EPhoneEntry *pentry = E_PHONE_ENTRY (entry);
	char *text;

	text = g_strdup_printf (CONTACT_FORMAT, (char*)e_contact_get_const (contact, E_CONTACT_NAME_OR_ORG), identifier);
	pentry->phone_number = cleanup_number (identifier);

	emit_changed_signal (pentry, pentry->phone_number);

	g_signal_handlers_block_by_func
		(G_OBJECT (entry), text_changed, NULL);
	gtk_entry_set_text (GTK_ENTRY (entry), text);
	g_signal_handlers_unblock_by_func
		(G_OBJECT (entry), text_changed, NULL);

	g_free (text);
}

static GList *
test_display_func (EContact *contact, gpointer data)
{
	GList *entries = NULL;
	EVCard *card = E_VCARD (contact);
	GList *attrs, *a;

	attrs = e_vcard_get_attributes (card);
	for (a = attrs; a; a = a->next) {
		if (strcmp (e_vcard_attribute_get_name (a->data), EVC_TEL) == 0
		    && e_vcard_attribute_has_type (a->data, "CELL")) {
			GList *phones, *p;

			phones = e_vcard_attribute_get_values (a->data);
			for (p = phones; p; p = p->next) {
				EContactEntyItem *item;

				item = g_new0 (EContactEntyItem, 1);
				item->display_string = g_strdup_printf (CONTACT_FORMAT, (char*) e_contact_get_const (contact, E_CONTACT_NAME_OR_ORG), (char*) p->data);
				item->identifier = g_strdup (p->data);
				entries = g_list_prepend (entries, item);
			}
		}
	}

	return g_list_reverse (entries);
}

static void
e_phone_entry_finalize (GObject *object)
{
	EPhoneEntry *pentry = E_PHONE_ENTRY (object);

	g_free (pentry->text);
	g_free (pentry->phone_number);
	G_OBJECT_CLASS (e_phone_entry_parent_class)->finalize (object);
}

static void
e_phone_entry_init (EPhoneEntry *entry)
{
	EContactField fields[] = { E_CONTACT_FULL_NAME, E_CONTACT_NICKNAME, E_CONTACT_ORG, E_CONTACT_PHONE_MOBILE, 0 };
	ESourceRegistry *registry;
	GError *error = NULL;

	/* XXX This call blocks while a D-Bus connection is made, possibly
	 *     requiring activation.  Might be better to create the registry
	 *     in main(), pass it to ui_init(), and have e_phone_entry_new()
	 *     take it as an argument.  Calling this from main() means if it
	 *     fails you can abort cleanly with a console error message. */
	registry = e_source_registry_new_sync (NULL, &error);
	if (registry == NULL) {
		g_error ("%s: %s", G_STRFUNC, error->message);
		g_assert_not_reached ();
	}
	e_contact_entry_set_registry (E_CONTACT_ENTRY (entry), registry);
	g_object_unref (registry);

	e_contact_entry_set_search_fields (E_CONTACT_ENTRY (entry), (const EContactField *)fields);
	e_contact_entry_set_display_func (E_CONTACT_ENTRY (entry), test_display_func, NULL, NULL);
	g_signal_connect (G_OBJECT (entry), "contact_selected",
			G_CALLBACK (contact_selected_cb), NULL);
	g_signal_connect (G_OBJECT (entry), "changed",
			G_CALLBACK (text_changed), NULL);
}

static void
e_phone_entry_class_init (EPhoneEntryClass *klass)
{
	GObjectClass *object_class = (GObjectClass *) klass;

	/* GObject */
	object_class->finalize = e_phone_entry_finalize;

	/* Signals */
	signals[PHONE_CHANGED] = g_signal_new ("phone-changed",
					G_TYPE_FROM_CLASS (object_class),
					G_SIGNAL_RUN_LAST,
					G_STRUCT_OFFSET (EPhoneEntryClass, phone_changed),
					NULL, NULL,
					g_cclosure_marshal_VOID__STRING,
					G_TYPE_NONE, 1, G_TYPE_STRING);
}

GtkWidget *
e_phone_entry_new (void)
{
	return g_object_new (e_phone_entry_get_type (), NULL);
}

char *
e_phone_entry_get_number (EPhoneEntry *pentry)
{
	g_return_val_if_fail (E_IS_PHONE_ENTRY (pentry), NULL);

	if (pentry->phone_number == NULL)
		return NULL;

	return cleanup_number (pentry->phone_number);
}

