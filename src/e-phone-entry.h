/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2005 Bastien Nocera <hadess@hadess.net>
 *
 * e-phone-entry.h
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

#ifndef PHONE_ENTRY_H
#define PHONE_ENTRY_H

#include <gtk/gtk.h>
#include "e-contact-entry.h"

G_BEGIN_DECLS

#define E_PHONE_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), e_phone_entry_get_type (), EPhoneEntry))
#define E_PHONE_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), e_phone_entry_get_type (), EPhoneEntryClass))
#define E_IS_PHONE_ENTRY(obj) (G_TYPE_CHECK_INSTANCE_TYPE (obj, e_phone_entry_get_type ()))
#define E_IS_PHONE_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), e_phone_entry_get_type ()))
#define E_PHONE_ENTRY_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), E_PHONE_ENTRY_TYPE, EPhoneEntryClass))

typedef struct EPhoneEntry EPhoneEntry;

struct EPhoneEntry {
	EContactEntry entry;
	char *text;
	char *phone_number;
};

typedef struct {
	EContactEntryClass parent_class;
	/*
	 * Fired when the phone number has changed, NULL if we shouldn't
	 * be able to activate
	 */
	void (*phone_changed) (GtkWidget *entry, const char *phone_number);
} EPhoneEntryClass;

GType e_phone_entry_get_type (void);

GtkWidget *e_phone_entry_new (void);
char *e_phone_entry_get_number (EPhoneEntry *pentry);

G_END_DECLS

#endif /* PHONE_ENTRY_H */
