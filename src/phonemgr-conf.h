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

#ifndef _PHONEMGR_CONF_H
#define _PHONEMGR_CONF_H

#include <gconf/gconf-client.h>

#define CONFBASE "/apps/gnome-phone-manager"

enum {
	CONNECTION_BLUETOOTH,
	CONNECTION_SERIAL1,
	CONNECTION_SERIAL2,
	CONNECTION_IRCOMM,
	CONNECTION_OTHER,
	CONNECTION_NONE,
	CONNECTION_LAST
};

#define PHONEMGR_DEVICE_TYPE_FILTER (BLUETOOTH_TYPE_PHONE | BLUETOOTH_TYPE_MODEM)

#endif

