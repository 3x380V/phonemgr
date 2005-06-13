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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
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
#include <libedataserver/e-source-list.h>
#include "e-phone-entry.h"

#define GCONF_COMPLETION "/apps/evolution/addressbook"
#define GCONF_COMPLETION_SOURCES GCONF_COMPLETION "/sources"

#define CONTACT_FORMAT "%s (%s)"

/* Signals */
enum {
  PHONE_CHANGED, /* Signal argument is the phone number */
  LAST_SIGNAL
};

static int signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE(EPhoneEntry, e_phone_entry, E_TYPE_CONTACT_ENTRY);

static char *
remove_spaces (const char *str)
{
	GString *nospace;
	char *p;

	p = (char *) str;
	nospace = g_string_new ("");
	while (*p != '\0') {
		gunichar c;
		c = g_utf8_get_char (p);
		if (g_unichar_isspace (c) == FALSE)
			nospace = g_string_append_unichar (nospace, c);
		p = g_utf8_next_char(p);
	}

	return g_string_free (nospace, FALSE);
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
	if (pentry->text == NULL) {
		pentry->text = g_strdup (current);
		return;
	}

	if (strcmp (pentry->text, current) == 0) {
		g_free (current);
		return;
	}
	pentry->text = g_strdup (current);
	if (pentry->phone_number != NULL) {
		g_free (pentry->phone_number);
		pentry->phone_number = NULL;
	}
	p = current;

	while (*p != '\0') {
		gunichar c;
		c = g_utf8_get_char (p);
		/* We only allow digits, plus signs and spaces in user supplied
		 * phone numbers. */
		if (g_unichar_isdigit (c) == FALSE
				&& g_unichar_isspace (c) == FALSE
				&& c != 0x2B) {
			g_free (current);
			emit_changed_signal (pentry, NULL);
			return;
		}
		p = g_utf8_next_char(p);
	}

	/* Remove spaces from the phone number */
	pentry->phone_number = remove_spaces (current);
	g_free (current);
	emit_changed_signal (pentry, pentry->phone_number);
}

static void
contact_selected_cb (GtkWidget *entry, EContact *contact)
{
	EPhoneEntry *pentry = E_PHONE_ENTRY (entry);
	char *text;

	text = g_strdup_printf (CONTACT_FORMAT, (char*)e_contact_get_const (contact, E_CONTACT_NAME_OR_ORG), (char*)e_contact_get_const (contact, E_CONTACT_PHONE_MOBILE));
	pentry->phone_number = remove_spaces (e_contact_get_const
			(contact, E_CONTACT_PHONE_MOBILE));

	emit_changed_signal (pentry, pentry->phone_number);

	g_signal_handlers_block_by_func
		(G_OBJECT (entry), text_changed, NULL);
	gtk_entry_set_text (GTK_ENTRY (entry), text);
	g_signal_handlers_unblock_by_func
		(G_OBJECT (entry), text_changed, NULL);

	g_free (text);
}

static char *
test_display_func (EContact *contact, gpointer data)
{
	const char *mobile;

	/* No contacts without a mobile phone in the list */
	mobile = e_contact_get_const (contact, E_CONTACT_PHONE_MOBILE);
	if (mobile == NULL) {
		return NULL;
	}
	return g_strdup_printf (CONTACT_FORMAT, (char*)e_contact_get_const (contact, E_CONTACT_NAME_OR_ORG), mobile);
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
add_sources (EContactEntry *entry)
{
	ESourceList *source_list;

	source_list =
		e_source_list_new_for_gconf_default (GCONF_COMPLETION_SOURCES);
	e_contact_entry_set_source_list (E_CONTACT_ENTRY (entry),
			source_list);
	g_object_unref (source_list);
}

static void
e_phone_entry_init (EPhoneEntry *entry)
{
	EContactField fields[] = { E_CONTACT_FULL_NAME, E_CONTACT_NICKNAME, E_CONTACT_ORG, E_CONTACT_PHONE_MOBILE, 0 };

	add_sources (E_CONTACT_ENTRY (entry));
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
  GObjectClass *object_class;
  GtkWidgetClass *widget_class;
  
  object_class = (GObjectClass *) klass;
  widget_class = (GtkWidgetClass *) klass;
  
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

	return g_strdup (pentry->phone_number);
}

GtkWidget *
e_phone_entry_new_from_glade (gchar *widget_name,
			      gchar *string1, gchar *string2,
			      gint int1, gint int2)
{
	GtkWidget *w = e_phone_entry_new ();
	gtk_widget_set_name (w, widget_name);
	gtk_widget_show (w);
	return w;
}
