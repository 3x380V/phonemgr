/*
 * main.c - entry point and libpurple boilerplate for telepathy-sms
 * Copyright (C) 2007 Will Thompson
 * Copyright (C) 2007 Collabora Ltd.
 * Portions taken from libpurple/examples/nullclient.c:
 *   Copyright (C) 2007 Sadrul Habib Chowdhury, Sean Egan, Gary Kramlich,
 *                      Mark Doliner, Richard Laager
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>

#include <string.h>
#include <errno.h>
#include <signal.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <telepathy-glib/run.h>
#include <telepathy-glib/debug.h>

//#include "defines.h"
#include "debug.h"
#include "connection-manager.h"

static gboolean g_fatal_warnings = FALSE;

static const GOptionEntry entries[] = {
	{"g-fatal-warnings", 0, 0, G_OPTION_ARG_NONE, &g_fatal_warnings, "Make all warnings fatal", NULL},
	{NULL}
};

static TpBaseConnectionManager *
get_cm (void)
{
	return (TpBaseConnectionManager *) sms_connection_manager_get ();
}

int
main(int argc,
     char **argv)
{
    int ret = 0;
    GOptionContext *context;

    g_set_prgname("telepathy-sms");
    g_thread_init (NULL);

    context = g_option_context_new ("Telepathy SMS backend");
    g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);

    g_option_context_parse (context, &argc, &argv, NULL);

    if (g_fatal_warnings) {
	    GLogLevelFlags fatal_mask;

	    fatal_mask = g_log_set_always_fatal (G_LOG_FATAL_MASK);
	    fatal_mask |= G_LOG_LEVEL_WARNING | G_LOG_LEVEL_CRITICAL;
	    g_log_set_always_fatal (fatal_mask);
    }

    signal (SIGCHLD, SIG_IGN);

    tp_debug_set_flags_from_env ("SMS_DEBUG");
    ret = tp_run_connection_manager ("telepathy-sms", PACKAGE_VERSION,
    				     get_cm, argc, argv);

    return ret;
}
