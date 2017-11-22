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

#ifndef _PHONEMGR_OBJECT_H
#define _PHONEMGR_OBJECT_H

G_BEGIN_DECLS

#include <glib-object.h>

typedef struct _PhonemgrObject PhonemgrObject;
typedef struct _PhonemgrObjectClass PhonemgrObjectClass;

struct _PhonemgrObjectClass {
	GObjectClass  parent_class;
	void (* number_batteries_changed) (PhonemgrObject *o, guint num_batteries);
	void (* battery_state_changed) (PhonemgrObject *o,
					guint battery_index,
					guint percentage,
					gboolean on_ac);
	void (* network_registration_changed) (PhonemgrObject *o,
					       int mcc, int mnc, int lac, int cid);
};

struct _PhonemgrObject {
	GObject parent;

	/* Network registration */
	int mcc;
	int mnc;
	int lac;
	int cid;

	/* Battery */
	guint percentage;
	guint on_ac : 1;
	/* We can only have one battery */
	guint num_batteries : 1;
};

GType phonemgr_object_get_type (void);

void phonemgr_object_emit_number_batteries_changed (PhonemgrObject *o, guint num_batteries);
void phonemgr_object_emit_battery_state_changed (PhonemgrObject *o, guint index, guint percentage, gboolean on_ac);
void phonemgr_object_emit_network_registration_changed (PhonemgrObject *o, int mcc, int mnc, int lac, int cid);

gboolean phonemgr_object_coldplug (PhonemgrObject *o, GError **error);

G_END_DECLS

#endif /* !_PHONEMGR_OBJECT_H */
