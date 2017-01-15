/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * PhoneManager Listener
 * Copyright (C) 2003-2004 Edd Dumbill <edd@usefulinc.com>
 * Copyright (C) 2005-2007 Bastien Nocera <hadess@hadess.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <glib.h>
#include <glib-object.h>
#include <gnokii.h>

#include "phonemgr-listener.h"
#include "phonemgr-marshal.h"
#include "phonemgr-utils.h"

/* #define DUMMY 1 */
#define POLL_TIMEOUT 300 * 1000
#define TRYLOCK_TIMEOUT 50 * 1000

#define CHECK_EXIT { if (l->terminated != FALSE) { g_mutex_unlock (&l->mutex); goto exit_thread; } }

static gpointer		 parent_class = NULL;

typedef struct {
	PhonemgrListenerCallStatus status;
	char *number;
	char *name;
} PhoneMgrCall;

typedef struct {
	float batterylevel;
	gn_power_source powersource;
} PhoneMgrBattery;

typedef struct {
	int mcc, mnc, lac, cid;
} PhoneMgrNetwork;

typedef struct {
	int type;
	union {
		gn_sms *message;
		PhoneMgrCall *call;
		PhoneMgrBattery *battery;
		PhoneMgrNetwork *network;
	};
} AsyncSignal;

struct _PhonemgrListener
{
	GObject object;

	GThread *thread;
	GAsyncQueue *queue;
	GMutex mutex;

	PhonemgrState *phone_state;

	char *driver;
	char *own_number;
	char *imei;

	/* The previous call status */
	gn_call_status prev_call_status;
	int call_id;

	/* Battery info */
	float batterylevel;
	gn_power_source powersource;

	/* Network info cache */
	int cid;
	int lac;
	char network_code[10];

	guint connected : 1;
	guint terminated : 1;
	guint debug : 1;

	/* Whether the driver supports GN_OP_OnSMS */
	guint supports_sms_notif : 1;
	/* Whether or not the driver supports GN_OP_GetPowersource */
	guint supports_power_source : 1;
};

static void phonemgr_listener_class_init (PhonemgrListenerClass *klass);
static void phonemgr_listener_init (PhonemgrListener *bc);
static void phonemgr_listener_finalize (GObject *obj);
static void phonemgr_listener_get_own_details (PhonemgrListener *l);

#ifndef DUMMY
static void phonemgr_listener_thread (PhonemgrListener *l);
#endif

enum {
	MESSAGE_SIGNAL,
	REPORT_STATUS_SIGNAL,
	STATUS_SIGNAL,
	CALL_STATUS_SIGNAL,
	BATTERY_SIGNAL,
	NETWORK_SIGNAL,
	LAST_SIGNAL
};

enum {
	PROP_0,
	PROP_DEBUG,
};

static int phonemgr_listener_signals[LAST_SIGNAL] = { 0 } ;

G_DEFINE_TYPE (PhonemgrListener, phonemgr_listener, G_TYPE_OBJECT)

static void
phonemgr_listener_get_property (GObject *object,
				 guint property_id,
				 GValue *value,
				 GParamSpec *pspec)
{
	PhonemgrListener *l;

	l = PHONEMGR_LISTENER (object);

	switch (property_id) {
	case PROP_DEBUG:
		g_value_set_boolean (value, l->debug != FALSE);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
phonemgr_listener_set_property (GObject *object,
				guint property_id,
				const GValue *value,
				GParamSpec *pspec)
{
	PhonemgrListener *l;

	l = PHONEMGR_LISTENER (object);

	switch (property_id) {
	case PROP_DEBUG:
		l->debug = g_value_get_boolean (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
	}
}

static void
phonemgr_listener_class_init (PhonemgrListenerClass *klass)
{
	GObjectClass *object_class;

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	object_class = (GObjectClass*) klass;
	object_class->set_property = phonemgr_listener_set_property;
	object_class->get_property = phonemgr_listener_get_property;

	phonemgr_listener_signals[MESSAGE_SIGNAL] =
		g_signal_new ("message",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (PhonemgrListenerClass, message),
			      NULL, NULL,
			      phonemgr_marshal_VOID__STRING_ULONG_STRING,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_STRING, G_TYPE_ULONG, G_TYPE_STRING);

	phonemgr_listener_signals[REPORT_STATUS_SIGNAL] =
		g_signal_new ("report-status",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (PhonemgrListenerClass, report_status),
			      NULL, NULL,
			      phonemgr_marshal_VOID__STRING_ULONG_INT,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_STRING, G_TYPE_ULONG, G_TYPE_INT);

	phonemgr_listener_signals[STATUS_SIGNAL] =
		g_signal_new ("status",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (PhonemgrListenerClass, status),
			      NULL, NULL,
			      phonemgr_marshal_VOID__UINT,
			      G_TYPE_NONE,
			      1,
			      G_TYPE_UINT);

	phonemgr_listener_signals[CALL_STATUS_SIGNAL] =
		g_signal_new ("call-status",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (PhonemgrListenerClass, call_status),
			      NULL, NULL,
			      phonemgr_marshal_VOID__UINT_STRING_STRING,
			      G_TYPE_NONE,
			      3,
			      G_TYPE_UINT, G_TYPE_STRING, G_TYPE_STRING);

	phonemgr_listener_signals[BATTERY_SIGNAL] =
		g_signal_new ("battery",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (PhonemgrListenerClass, battery),
			      NULL, NULL,
			      phonemgr_marshal_VOID__INT_BOOLEAN,
			      G_TYPE_NONE,
			      2,
			      G_TYPE_INT, G_TYPE_BOOLEAN);

	phonemgr_listener_signals[NETWORK_SIGNAL] =
		g_signal_new ("network",
			      G_OBJECT_CLASS_TYPE (klass),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (PhonemgrListenerClass, network),
			      NULL, NULL,
			      phonemgr_marshal_VOID__INT_INT_INT_INT,
			      G_TYPE_NONE,
			      4,
			      G_TYPE_INT, G_TYPE_INT, G_TYPE_INT, G_TYPE_INT);

	g_object_class_install_property (object_class,
					 PROP_DEBUG,
					 g_param_spec_boolean ("debug", NULL, NULL,
							       FALSE, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

	object_class->finalize = phonemgr_listener_finalize;

	klass->message = NULL;
	klass->status = NULL;
}

GQuark
phonemgr_listener_error_quark (void)
{
	static GQuark q = 0;
	if (q == 0)
		q = g_quark_from_static_string ("phonemgr-error-quark");
	return q;
}

/* Only for use within the polling thread */
static gn_error
phonemgr_listener_gnokii_func (gn_operation op, PhonemgrListener *l)
{
	gn_error retval;

	retval = gn_sm_functions(op, &l->phone_state->data, &l->phone_state->state);
	if (retval == GN_ERR_NOTREADY) {
		g_message ("Operation '%d' failed with error: %s",
			   op, phonemgr_utils_gn_error_to_string (retval, NULL));
		l->terminated = TRUE;
		l->connected = FALSE;
	}
	return retval;
}

static void
phonemgr_listener_emit_status (PhonemgrListener *l, PhonemgrListenerStatus status)
{
	g_signal_emit (G_OBJECT (l),
		       phonemgr_listener_signals[STATUS_SIGNAL],
		       0, status);
}

static void
phonemgr_listener_emit_report_status (PhonemgrListener *l, gn_sms *message)
{
	time_t time;
	char *sender;
	PhonemgrListenerReportStatus status;

	switch (message->user_data[0].dr_status) {
	case GN_SMS_DR_Status_Delivered:
		status = PHONEMGR_LISTENER_REPORT_DELIVERED;
		break;
	case GN_SMS_DR_Status_Pending:
		status = PHONEMGR_LISTENER_REPORT_PENDING;
		break;
	case GN_SMS_DR_Status_Failed_Temporary:
		status = PHONEMGR_LISTENER_REPORT_FAILED_TEMPORARY;
		break;
	case GN_SMS_DR_Status_Failed_Permanent:
		status = PHONEMGR_LISTENER_REPORT_FAILED_PERMANENT;
		break;
	default:
		return;
	}

	time = gn_timestamp_to_gtime (message->smsc_time);
	sender = g_strdup (message->remote.number);

	g_signal_emit (G_OBJECT (l),
		       phonemgr_listener_signals[REPORT_STATUS_SIGNAL],
		       0, sender, time, status);

	g_free (sender);
}

static void
phonemgr_listener_emit_call_status (PhonemgrListener *l,
				    PhonemgrListenerCallStatus status,
				    const char *phone,
				    const char *name)
{
	g_signal_emit (G_OBJECT (l),
		       phonemgr_listener_signals[CALL_STATUS_SIGNAL],
		       0, status, phone, name);
}

static void
phonemgr_listener_emit_battery (PhonemgrListener *l,
				int percent,
				gboolean on_ac)
{
	g_signal_emit (G_OBJECT (l),
		       phonemgr_listener_signals[BATTERY_SIGNAL],
		       0, percent, on_ac);
}

static void
phonemgr_listener_emit_message (PhonemgrListener *l, gn_sms *message)
{
	time_t time;
	char *text, *sender, *origtext;

	text = NULL;

	/* The data should be in whatever the locale's encoding is,
	 * if it doesn't fit the default GSM alphabet (roughly ISO8859-1)
	 * as a design decision in gnokii */
	origtext = (char *) message->user_data[0].u.text;

	if (gn_char_def_alphabet (origtext)) {
		GError *err = NULL;

		text = g_convert (origtext, -1,
				  "UTF-8", "ISO8859-1",
				  NULL, NULL, &err);

		if (err != NULL) {
			g_warning ("Conversion error from GSM default alphabet: %d %s", err->code, err->message);
			g_error_free (err);
			g_free (text);
			text = g_strdup (origtext);
		}
	} else if (g_utf8_validate (origtext, -1, NULL) == FALSE) {
		GError *err = NULL;

		text = g_locale_to_utf8 (origtext, -1, NULL, NULL, &err);

		if (err != NULL) {
			g_warning ("Conversion error: %d %s", err->code, err->message);
			g_error_free (err);
			g_free (text);
			text = g_strdup (origtext);
		}
	}

	if (text == NULL)
		text = g_strdup (origtext);

	time = gn_timestamp_to_gtime (message->smsc_time);
	sender = g_strdup (message->remote.number);

	g_signal_emit (G_OBJECT (l),
		       phonemgr_listener_signals[MESSAGE_SIGNAL],
		       0, sender, time, text);

	g_free (text);
	g_free (sender);
}

PhonemgrListener *
phonemgr_listener_new (gboolean debug)
{
	return PHONEMGR_LISTENER (g_object_new (PHONEMGR_TYPE_LISTENER, "debug", debug, NULL));
}

#ifndef DUMMY
static void
phonemgr_listener_init (PhonemgrListener *l)
{
	l->queue = g_async_queue_new ();
	g_mutex_init (&l->mutex);
	l->driver = NULL;
	l->own_number = NULL;
	l->batterylevel = 1;
	l->supports_power_source = TRUE;
	l->powersource = GN_PS_BATTERY;
}

static void
phonemgr_listener_finalize(GObject *obj)
{
	PhonemgrListener *l;

	l = PHONEMGR_LISTENER (obj);
	if (l != NULL) {
		if (l->connected)
			phonemgr_listener_disconnect (l);
		//FIXME empty the queue of its stuff
		g_async_queue_unref (l->queue);
	}

	G_OBJECT_CLASS (parent_class)->finalize(obj);
}

gboolean
phonemgr_listener_connect (PhonemgrListener *l, const char *device, GError **error)
{
	PhonemgrConnectionType type;
	int channel;

	g_return_val_if_fail (PHONEMGR_IS_LISTENER (l), FALSE);
	g_return_val_if_fail (l->connected == FALSE, FALSE);
	g_return_val_if_fail (l->phone_state == NULL, FALSE);

	l->terminated = FALSE;

	phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_CONNECTING);

	channel = -1;

	type = phonemgr_utils_address_is (device);
	if (type == PHONEMGR_CONNECTION_BLUETOOTH) {
		channel = phonemgr_utils_get_serial_channel (device);
		if (channel < 0) {
			//FIXME
			phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_ERROR);
			return FALSE;
		}
	}

	phonemgr_utils_free (l->phone_state);
	l->phone_state = phonemgr_utils_connect (device, NULL, channel, l->debug, error);
	if (l->phone_state == NULL) {
		//FIXME
		phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_ERROR);
		return FALSE;
	}

	g_free (l->driver);
	l->driver = phonemgr_utils_guess_driver (l->phone_state, device, error);
	if (l->driver == NULL) {
		//FIXME
		phonemgr_utils_disconnect (l->phone_state);
		phonemgr_utils_free (l->phone_state);
		phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_ERROR);
		return FALSE;
	}

	/* Do we need to see gnapplet driver? */
	if (strcmp (l->driver, PHONEMGR_DEFAULT_GNAPPLET_DRIVER) == 0) {
		channel = phonemgr_utils_get_gnapplet_channel (device);
		if (channel < 0) {
			phonemgr_utils_disconnect (l->phone_state);
			phonemgr_utils_free (l->phone_state);
			g_free (l->driver);
			l->driver = NULL;
			//FIXME
			g_message ("Couldn't connect to the device, gnapplet needed but not running");
			phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_ERROR);
			return FALSE;
		}
	}

	/* Need a different driver? then reconnect */
	if (strcmp (l->driver, PHONEMGR_DEFAULT_DRIVER) != 0
	    && strcmp (l->driver, PHONEMGR_DEFAULT_USB_DRIVER) != 0) {
		phonemgr_utils_disconnect (l->phone_state);
		phonemgr_utils_free (l->phone_state);
		if (type == PHONEMGR_CONNECTION_IRDA)
			g_usleep (G_USEC_PER_SEC);
		l->phone_state = phonemgr_utils_connect (device, l->driver, channel, l->debug, error);
		if (l->phone_state == NULL) {
			//FIXME
			phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_ERROR);
			return FALSE;
		}
	}

	g_message ("Using driver '%s'", l->driver);
	l->connected = TRUE;

	phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_CONNECTED);
	phonemgr_listener_get_own_details (l);

	l->thread = g_thread_create ((GThreadFunc) phonemgr_listener_thread,
				     l, TRUE, NULL);

	return l->connected;
}

static gboolean
phonemgr_listener_push (PhonemgrListener *l)
{
	AsyncSignal *signal;

	g_return_val_if_fail (l->connected != FALSE, FALSE);

	signal = g_async_queue_try_pop (l->queue);
	if (signal == NULL)
		return FALSE;

	switch (signal->type) {
	case MESSAGE_SIGNAL:
		g_message ("emitting message");
		phonemgr_listener_emit_message (l, signal->message);
		g_free (signal->message);
		break;
	case REPORT_STATUS_SIGNAL:
		g_message ("emitting delivery report status");
		phonemgr_listener_emit_report_status (l, signal->message);
		g_free (signal->message);
		break;
	case CALL_STATUS_SIGNAL:
		g_message ("emitting call status");
		phonemgr_listener_emit_call_status (l, signal->call->status,
						    signal->call->number,
						    signal->call->name);
		g_free (signal->call->number);
		g_free (signal->call->name);
		g_free (signal->call);
		break;
	case BATTERY_SIGNAL:
		g_message ("emitting battery");
		phonemgr_listener_emit_battery (l,
						(int) signal->battery->batterylevel,
						signal->battery->powersource == GN_PS_ACDC);
		g_free (signal->battery);
		break;
	case NETWORK_SIGNAL:
		g_message ("emitting network info");
		g_signal_emit (G_OBJECT (l),
			       phonemgr_listener_signals[NETWORK_SIGNAL], 0,
			       signal->network->mcc,
			       signal->network->mnc,
			       signal->network->lac,
			       signal->network->cid);
		g_free (signal->network);
		break;
	default:
		g_assert_not_reached ();
	}

	g_free (signal);

	return FALSE;
}

static gn_error
phonemgr_listener_new_sms_cb (gn_sms *message, struct gn_statemachine *state, void *user_data)
{
	PhonemgrListener *l = (PhonemgrListener *) user_data;
	AsyncSignal *signal;
	int type;

	/* Check that it's a type of message we support */
	if (message->type == GN_SMS_MT_DeliveryReport)
		type = REPORT_STATUS_SIGNAL;
	else if (message->type == GN_SMS_MT_Deliver)
		type = MESSAGE_SIGNAL;
	else
		return GN_ERR_NONE;

	signal = g_new0 (AsyncSignal, 1);
	signal->type = type;
	/* The message is allocated on the stack in the driver, so copy it */
	signal->message = g_memdup (message, sizeof (gn_sms));

	g_async_queue_push (l->queue, signal);
	g_idle_add ((GSourceFunc) phonemgr_listener_push, l);

	return GN_ERR_NONE;
}

static void
phonemgr_listener_call_status (PhonemgrListener *l, gn_call_status call_status, const char *number, const char *name)
{
	PhonemgrListenerCallStatus status;
	PhoneMgrCall *call;
	AsyncSignal *signal;

	if (call_status == l->prev_call_status)
		return;

	status = PHONEMGR_LISTENER_CALL_UNKNOWN;

	switch (call_status) {
	case GN_CALL_Idle:
		status = PHONEMGR_LISTENER_CALL_IDLE;
		break;
	case GN_CALL_Incoming:
		status = PHONEMGR_LISTENER_CALL_INCOMING;
		break;
	case GN_CALL_RemoteHangup:
	case GN_CALL_LocalHangup:
	case GN_CALL_Held:
	case GN_CALL_Resumed:
		break;
	case GN_CALL_Established:
		status = PHONEMGR_LISTENER_CALL_ONGOING;
		break;
	default:
		break;
	}

	if (status == PHONEMGR_LISTENER_CALL_UNKNOWN)
		return;
	l->prev_call_status = call_status;

	call = g_new0 (PhoneMgrCall, 1);
	call->status = status;
	call->number = g_strdup (number);
	call->name = g_strdup (name);

	signal = g_new0 (AsyncSignal, 1);
	signal->type = CALL_STATUS_SIGNAL;
	signal->call = call;

	g_async_queue_push (l->queue, signal);
	g_idle_add ((GSourceFunc) phonemgr_listener_push, l);
}

static void
phonemgr_listener_new_call_cb (gn_call_status call_status, gn_call_info *call_info, struct gn_statemachine *state, void *user_data)
{
	PhonemgrListener *l = (PhonemgrListener *) user_data;

	/* We should ignore things that aren't the first call, but we have no idea of what
	 * call ID the drivers might be using */

	l->call_id = call_info->call_id;
	gn_call_notifier (call_status, call_info, state);

	phonemgr_listener_call_status (l, call_status, call_info->number, call_info->name);
}
static void
phonemgr_listener_cell_not_cb (gn_network_info *info, void *user_data)
{
	PhonemgrListener *l = (PhonemgrListener *) user_data;
	AsyncSignal *signal;
	int cid, lac, mcc, mnc;
	char *end;

	if (info->cell_id[2] == 0 && info->cell_id[3] == 0)
		cid = (info->cell_id[0] << 8) + info->cell_id[1];
	else
		cid = (info->cell_id[0] << 24) + (info->cell_id[1] << 16) + (info->cell_id[2] << 8) + info->cell_id[3];

	lac = (info->LAC[0] << 8) + info->LAC[1];

	/* Is it the same cells? */
	if (lac == l->lac && cid == l->cid)
		return;

	/* Only call GN_OP_GetNetworkInfo if we actually need it */
	if (info->network_code == NULL ||
	    info->network_code[0] == '\0') {
		gn_network_info new_info;

		l->phone_state->data.network_info = &new_info;
		l->phone_state->data.reg_notification = phonemgr_listener_cell_not_cb;
		l->phone_state->data.callback_data = l;

		if (phonemgr_listener_gnokii_func (GN_OP_GetNetworkInfo, l) != GN_ERR_NONE)
			return;
		g_stpcpy (info->network_code, new_info.network_code);
	}

	if (info->network_code == NULL ||
	    info->network_code[0] == '\0' ||
	    strstr (info->network_code, " ") == NULL)
		return;

	mcc = g_ascii_strtoll (info->network_code, &end, 0);
	if (mcc == 0)
		return;
	mnc = g_ascii_strtoll (end, NULL, 0);

	/* Save all that */
	g_stpcpy (l->network_code, info->network_code);
	l->lac = lac;
	l->cid = cid;

	/* And push all that to a signal */
	signal = g_new0 (AsyncSignal, 1);
	signal->type = NETWORK_SIGNAL;
	signal->network = g_new0 (PhoneMgrNetwork, 1);
	signal->network->mcc = mcc;
	signal->network->mnc = mnc;
	signal->network->lac = lac;
	signal->network->cid = cid;

	g_async_queue_push (l->queue, signal);
	g_idle_add ((GSourceFunc) phonemgr_listener_push, l);
}

static void
phonemgr_listener_sms_notification_soft_poll (PhonemgrListener *l)
{
	gn_sm_loop (10, &l->phone_state->state);
	/* Some phones may not be able to notify us, thus we give
	 * lowlevel chance to poll them */
	phonemgr_listener_gnokii_func (GN_OP_PollSMS, l);
}

static void
phonemgr_listener_call_notification_poll (PhonemgrListener *l)
{
	gn_call *call;

	/* Don't call gn_sm_loop(), if the SMS notification already does it */
	if (l->supports_sms_notif == FALSE)
		gn_sm_loop (1, &l->phone_state->state);

	/* Check for active calls */
	gn_call_check_active (&l->phone_state->state);
	call = gn_call_get_active (0);
	if (call == NULL) {
		/* Call is NULL when it's GN_CALL_Idle */
		phonemgr_listener_call_status (l, GN_CALL_Idle, NULL, NULL);
		l->call_id = 0;
	} else {
		phonemgr_listener_call_status (l, call->status, call->remote_number, call->remote_name);
		l->call_id = call->call_id;
	}
}

static void
phonemgr_listener_battery_poll (PhonemgrListener *l)
{
	float batterylevel = -1;
	gn_power_source powersource = -1;
	gn_battery_unit battery_unit = GN_BU_Arbitrary;

	(&l->phone_state->data)->battery_level = &batterylevel;
	(&l->phone_state->data)->power_source = &powersource;
	(&l->phone_state->data)->battery_unit = &battery_unit;

	/* Some drivers will use the same function for battery level and power source, so optimise.
	 * Make sure to get the battery level first, as most drivers implement only this one */
	if (phonemgr_listener_gnokii_func (GN_OP_GetBatteryLevel, l) == GN_ERR_NOTREADY)
		return;

	if (powersource == -1 && l->supports_power_source != FALSE) {
		gn_error error;

		error = phonemgr_listener_gnokii_func (GN_OP_GetPowersource, l);
		if (error == GN_ERR_NOTREADY) {
			return;
		} else if (error != GN_ERR_NONE) {
			g_message ("driver or phone doesn't support getting the power source");
			l->supports_power_source = FALSE;
			powersource = GN_PS_BATTERY;
		}
	} else if (l->supports_power_source == FALSE) {
		powersource = GN_PS_BATTERY;
	}

	if (batterylevel != l->batterylevel || powersource != l->powersource) {
		AsyncSignal *signal;

		l->batterylevel = batterylevel;
		l->powersource = powersource;

		/* As mentioned by Daniele Forsi at:
		 * http://thread.gmane.org/gmane.linux.drivers.gnokii/9329/focus=9331 */
		if (battery_unit == GN_BU_Arbitrary)
			batterylevel = batterylevel * 100 / l->phone_state->state.driver.phone.max_battery_level;

		signal = g_new0 (AsyncSignal, 1);
		signal->type = BATTERY_SIGNAL;
		signal->battery = g_new0 (PhoneMgrBattery, 1);
		signal->battery->batterylevel = batterylevel;
		signal->battery->powersource = powersource;

		g_async_queue_push (l->queue, signal);
		g_idle_add ((GSourceFunc) phonemgr_listener_push, l);
	}

}

static void
phonemgr_listener_get_own_details (PhonemgrListener *l)
{
	gn_memory_status memstat;
	gn_phonebook_entry entry;
	int count, start_entry, end_entry, num_entries;
	gn_error error;
	const char *imei;

	/* Get the IMEI of the phone */
	imei = gn_lib_get_phone_imei(&l->phone_state->state);
	if (imei != NULL)
		l->imei = g_strdup (imei);

	/* Get the own number from the phone */
	start_entry = 1;
	end_entry = num_entries = INT_MAX;

	memstat.memory_type = gn_str2memory_type("ON");
	l->phone_state->data.memory_status = &memstat;
	error = phonemgr_listener_gnokii_func (GN_OP_GetMemoryStatus, l);
	if (error == GN_ERR_NONE) {
		num_entries = memstat.used;
		end_entry = memstat.used + memstat.free;
	} else if (error == GN_ERR_INVALIDMEMORYTYPE) {
		g_message ("Couldn't get our own phone number (no Own Number phonebook)");
		return;
	} else {
		return;
	}

	count = start_entry;
	while (num_entries > 0 && count <= end_entry) {
		error = GN_ERR_NONE;

		memset(&entry, 0, sizeof(gn_phonebook_entry));
		entry.memory_type = memstat.memory_type;
		entry.location = count;

		l->phone_state->data.phonebook_entry = &entry;
		error = phonemgr_listener_gnokii_func (GN_OP_ReadPhonebook, l);
		if (error != GN_ERR_NONE && error != GN_ERR_EMPTYLOCATION)
			break;
		if (entry.empty != FALSE)
			continue;
		if (error == GN_ERR_NONE)
			num_entries--;
		count++;

		if (!entry.subentries_count && entry.number) {
			l->own_number = g_strdup (entry.number);
		} else {
			int i;

			for (i = 0; i < entry.subentries_count; i++) {
				if (entry.subentries[i].entry_type != GN_PHONEBOOK_ENTRY_Number)
					continue;

				l->own_number = g_strdup (entry.number);
				break;
			}
		}

		break;
	}

	if (l->own_number != NULL)
		g_message ("Our own phone number is: %s", l->own_number);
	else
		g_message ("Couldn't get our own phone number");
}

static void
phonemgr_listener_set_sms_notification (PhonemgrListener *l, gboolean state)
{
	if (state != FALSE) {
		gn_error error;
		/* Try to set up SMS notification using GN_OP_OnSMS */
		gn_data_clear (&l->phone_state->data);
		l->phone_state->data.on_sms = phonemgr_listener_new_sms_cb;
		l->phone_state->data.callback_data = l;
		error = phonemgr_listener_gnokii_func (GN_OP_OnSMS, l);
		if (error == GN_ERR_NONE) {
			l->supports_sms_notif = TRUE;
			g_message ("driver and phone support sms notifications");
		} else if (error == GN_ERR_NOTREADY) {
			return;
		} else {
			g_message ("driver or phone doesn't support sms notifications");
		}
	} else {
		if (l->supports_sms_notif == FALSE)
			return;
		/* Disable the SMS callback on exit */
		if (l->supports_sms_notif != FALSE) {
			l->phone_state->data.on_sms = NULL;
			phonemgr_listener_gnokii_func (GN_OP_OnSMS, l);
		}
	}
}

static void
phonemgr_listener_set_call_notification (PhonemgrListener *l, gboolean state)
{
	if (state != FALSE) {
		/* Set up Call notification using GN_OP_SetCallNotification */
		gn_data_clear (&l->phone_state->data);
		l->phone_state->data.call_notification = phonemgr_listener_new_call_cb;
		l->phone_state->data.callback_data = l;
		phonemgr_listener_gnokii_func (GN_OP_SetCallNotification, l);
	} else {
		/* Disable call notification */
		gn_data_clear (&l->phone_state->data);
		l->phone_state->data.call_notification = NULL;
		phonemgr_listener_gnokii_func (GN_OP_SetCallNotification, l);
	}
}

static void
phonemgr_listener_set_cell_notification (PhonemgrListener *l, gboolean state)
{
	if (state != FALSE) {
		gn_network_info info;

		/* Set up cell notification using GN_OP_GetNetworkInfo */
		gn_data_clear (&l->phone_state->data);
		memset (&info, 0, sizeof(info));
		l->phone_state->data.network_info = &info;
		l->phone_state->data.reg_notification = phonemgr_listener_cell_not_cb;
		l->phone_state->data.callback_data = l;
		if (phonemgr_listener_gnokii_func (GN_OP_GetNetworkInfo, l) != GN_ERR_NONE)
			return;

		phonemgr_listener_cell_not_cb (&info, l);
	} else {
		/* Disable cell notification */
		gn_data_clear (&l->phone_state->data);
		l->phone_state->data.reg_notification = NULL;
		phonemgr_listener_gnokii_func (GN_OP_GetNetworkInfo, l);
	}
}

static void
phonemgr_listener_disconnect_cleanup (PhonemgrListener *l)
{
	g_free (l->driver);
	l->driver = NULL;
	g_free (l->own_number);
	l->own_number = NULL;
	g_free (l->imei);
	l->imei = NULL;
	l->batterylevel = 1;
	l->supports_power_source = TRUE;
	l->powersource = GN_PS_BATTERY;

	l->connected = FALSE;
	//FIXME more to kill?
	phonemgr_utils_disconnect (l->phone_state);
	phonemgr_utils_free (l->phone_state);
	l->phone_state = NULL;

	phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_IDLE);
}

static void
phonemgr_listener_thread (PhonemgrListener *l)
{
	g_mutex_lock (&l->mutex);
	CHECK_EXIT;
	phonemgr_listener_set_sms_notification (l, TRUE);
	CHECK_EXIT;
	phonemgr_listener_set_call_notification (l, TRUE);
	CHECK_EXIT;
	phonemgr_listener_set_cell_notification (l, TRUE);
	CHECK_EXIT;
	g_mutex_unlock (&l->mutex);

	while (l->terminated == FALSE) {
		if (g_mutex_trylock (&l->mutex) != FALSE) {
			CHECK_EXIT;
			if (l->supports_sms_notif != FALSE)
				phonemgr_listener_sms_notification_soft_poll (l);
			CHECK_EXIT;
			phonemgr_listener_call_notification_poll (l);
			CHECK_EXIT;
			phonemgr_listener_battery_poll (l);
			CHECK_EXIT;

			g_mutex_unlock (&l->mutex);
			g_usleep (POLL_TIMEOUT);
		} else {
			g_usleep (TRYLOCK_TIMEOUT);
		}
	}

exit_thread:
	g_mutex_lock (&l->mutex);
	if (l->connected != FALSE) {
		phonemgr_listener_set_sms_notification (l, FALSE);
		phonemgr_listener_set_call_notification (l, FALSE);
		phonemgr_listener_set_cell_notification (l, FALSE);
	} else {
		phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_DISCONNECTING);
		phonemgr_listener_disconnect_cleanup (l);
	}
	g_mutex_unlock (&l->mutex);

	g_thread_exit (NULL);
}

void
phonemgr_listener_disconnect (PhonemgrListener *l)
{
	g_return_if_fail (PHONEMGR_IS_LISTENER (l));

	phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_DISCONNECTING);

	l->terminated = TRUE;
	g_thread_join (l->thread);

	phonemgr_listener_disconnect_cleanup (l);
}

void
phonemgr_listener_cancel_call (PhonemgrListener *l)
{
	g_return_if_fail (PHONEMGR_IS_LISTENER (l));
	//FIXME
}

void
phonemgr_listener_answer_call (PhonemgrListener *l)
{
	g_return_if_fail (PHONEMGR_IS_LISTENER (l));
	//FIXME
}

void
phonemgr_listener_queue_message (PhonemgrListener *l,
				 const char *number,
				 const char *message,
				 gboolean delivery_report)
{
	char *mstr, *iso;
	GError *err = NULL;
	gn_sms sms;
	gn_error error;
	gn_sms_message_center center;

	g_return_if_fail (PHONEMGR_IS_LISTENER (l));
	g_return_if_fail (l->connected != FALSE);
	g_return_if_fail (number != NULL);
	g_return_if_fail (message != NULL);

	/* Lock the phone and set up for SMS sending */
	g_mutex_lock (&l->mutex);
	gn_data_clear(&l->phone_state->data);
	gn_sms_default_submit(&sms);

	/* Default GSM alphabet is a subset of ISO8859-1,
	 * so try that */
	iso = g_convert (message, -1, "ISO8859-1", "UTF-8", NULL, NULL, &err);
	if (err != NULL) {
		g_clear_error (&err);
		g_free (iso);
		iso = NULL;
	}

	/* If the message contains characters not in the
	 * default GSM alaphabet, we pass it as UTF-8 encoding instead */
	if (iso && gn_char_def_alphabet((unsigned char *) iso)) {
		mstr = iso;
		sms.dcs.u.general.alphabet = GN_SMS_DCS_DefaultAlphabet;
	} else {
		g_free (iso);
		/* gnokii will convert to UCS-2 itself, but we pass UTF-8 */
		mstr = g_strdup (message);
		sms.dcs.u.general.alphabet = GN_SMS_DCS_UCS2;
	}

	if (err != NULL) {
		g_warning ("Conversion error: %d %s", err->code, err->message);
		g_free (mstr);
		g_error_free (err);
		g_mutex_unlock (&l->mutex);
		return;
	}

	/* Set the destination number */
	g_strlcpy (sms.remote.number, number, sizeof(sms.remote.number));
	if (sms.remote.number[0] == '+')
		sms.remote.type = GN_GSM_NUMBER_International;
	else
		sms.remote.type = GN_GSM_NUMBER_Unknown;

	/* Get the SMS Center number */
	l->phone_state->data.message_center = &center;
	center.id = 1;
	error = gn_sm_functions (GN_OP_GetSMSCenter, &l->phone_state->data, &l->phone_state->state);
	if (error == GN_ERR_NONE) {
		g_strlcpy (sms.smsc.number, center.smsc.number, sizeof (sms.smsc.number));
		sms.smsc.type = center.smsc.type;
	} else if (error == GN_ERR_NOTREADY) {
		g_message ("Can't send message, phone disconnected");
		g_mutex_unlock (&l->mutex);
		g_free (mstr);
		return;
	}

	if (!sms.smsc.type)
		sms.smsc.type = GN_GSM_NUMBER_Unknown;

	/* Set whether to get a delivery report */
	sms.delivery_report = delivery_report;

	/* Set the message data */
	sms.user_data[0].type = GN_SMS_DATA_Text;
	g_strlcpy ((char *) sms.user_data[0].u.text, mstr, strlen (mstr) + 1);
	sms.user_data[0].length = strlen (mstr);
	g_free (mstr);
	sms.user_data[1].type = GN_SMS_DATA_None;

	l->phone_state->data.sms = &sms;

	/* Actually send the message */
	error = gn_sms_send (&l->phone_state->data, &l->phone_state->state);

	/* Remove the reference to SMS */
	if (l->phone_state->data.sms->reference != NULL)
		free (l->phone_state->data.sms->reference);
	l->phone_state->data.sms = NULL;

	/* Unlock the phone */
	g_mutex_unlock (&l->mutex);
}

void
phonemgr_listener_set_time (PhonemgrListener *l,
			    time_t time)
{
	struct tm *t;
	gn_timestamp date;
	gn_error error;
	PhoneMgrError perr;

	g_return_if_fail (PHONEMGR_IS_LISTENER (l));
	g_return_if_fail (l->connected != FALSE);
	g_return_if_fail (l->phone_state != NULL);

	t = localtime(&time);
	date.year = t->tm_year + 1900;
	date.month = t->tm_mon + 1;
	date.day = t->tm_mday;
	date.hour = t->tm_hour;
	date.minute = t->tm_min;
	date.second = t->tm_sec;

	/* Lock the phone */
	g_mutex_lock (&l->mutex);

	/* Set the time and date */
	gn_data_clear(&l->phone_state->data);
	l->phone_state->data.datetime = &date;
	error = phonemgr_listener_gnokii_func (GN_OP_SetDateTime, l);

	/* Unlock the phone */
	g_mutex_unlock (&l->mutex);

	if (error != GN_ERR_NONE)
		g_warning ("Can't set date: %s", phonemgr_utils_gn_error_to_string (error, &perr));
}

static gboolean
phonemgr_listener_parse_data_uuid (const char *dataid,
				   gn_memory_type *type,
				   int *index)
{
	char *s, *memory_type;

	g_return_val_if_fail (dataid != NULL, FALSE);

	if (g_str_has_prefix (dataid, "GPM-UUID-") == FALSE)
		return FALSE;
	s = strchr (dataid + strlen ("GPM-UUID-") + 1, '-');
	if (s == NULL)
		return FALSE;
	if (strlen(s) < 5)
		return FALSE;

	if (type != NULL) {
		memory_type = g_strndup (s + 1, 2);
		*type = gn_str2memory_type (memory_type);
		g_free (memory_type);
	}

	*index = atoi (s + 4);
	return TRUE;
}

static gboolean
phonemgr_listener_get_phonebook_entry (PhonemgrListener *l,
				       gn_memory_type type,
				       guint index,
				       gn_phonebook_entry *entry)
{
	gn_error error;

	memset(entry, 0, sizeof(gn_phonebook_entry));
	entry->memory_type = type;
	entry->location = index;

	l->phone_state->data.phonebook_entry = entry;
	error = phonemgr_listener_gnokii_func (GN_OP_ReadPhonebook, l);
	if (error == GN_ERR_EMPTYLOCATION) {
		entry->empty = TRUE;
		return TRUE;
	}

	return (error == GN_ERR_NONE);
}

char *
phonemgr_listener_get_data (PhonemgrListener *l,
			    PhonemgrListenerDataType type,
			    const char *dataid)
{
	switch (type) {
	case PHONEMGR_LISTENER_DATA_CONTACT:
		{
			gn_phonebook_entry entry;
			gn_memory_type memory_type;
			char *retval;
			int index;

			if (phonemgr_listener_parse_data_uuid (dataid, &memory_type, &index) == FALSE)
				return NULL;

			g_mutex_lock (&l->mutex);

			if (phonemgr_listener_get_phonebook_entry (l, memory_type, index, &entry) == FALSE) {
				g_mutex_unlock (&l->mutex);
				return NULL;
			}

			if (entry.empty != FALSE) {
				g_mutex_unlock (&l->mutex);
				return NULL;
			}

			retval = gn_phonebook2vcardstr (&entry);

			g_mutex_unlock (&l->mutex);

			return retval;
		}
		break;
	case PHONEMGR_LISTENER_DATA_CALENDAR:
		{
			gn_calnote_list calnote_list;
			gn_calnote calnote;
			char *retval;
			gn_error error;
			int index;

			if (phonemgr_listener_parse_data_uuid (dataid, NULL, &index) == FALSE)
				return NULL;

			g_mutex_lock (&l->mutex);

			memset (&calnote, 0, sizeof (calnote));
			memset (&calnote_list, 0, sizeof (calnote_list));
			l->phone_state->data.calnote = &calnote;
			l->phone_state->data.calnote_list = &calnote_list;

			calnote.location = index;
			error = phonemgr_listener_gnokii_func (GN_OP_GetCalendarNote, l);
			if (error != GN_ERR_NONE) {
				g_mutex_unlock (&l->mutex);
				return NULL;
			}
			retval = gn_calnote2icalstr (&calnote);

			g_mutex_unlock (&l->mutex);

			return retval;
		}
		break;
	case PHONEMGR_LISTENER_DATA_TODO:
		break;
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

char **
phonemgr_listener_list_all_data (PhonemgrListener *l,
				 PhonemgrListenerDataType type)
{
	switch (type) {
	case PHONEMGR_LISTENER_DATA_CONTACT:
		{
			GPtrArray *a;
			gn_memory_status memstat;
			gn_error error;
			guint i, found;

			g_mutex_lock (&l->mutex);
			memset (&memstat, 0, sizeof (memstat));
			memstat.memory_type = gn_str2memory_type("ME");
			l->phone_state->data.memory_status = &memstat;
			error = phonemgr_listener_gnokii_func (GN_OP_GetMemoryStatus, l);
			if (error != GN_ERR_NONE) {
				g_message ("GN_OP_GetMemoryStatus on ME failed");
				g_mutex_unlock (&l->mutex);
				break;
			}

			a = g_ptr_array_sized_new (memstat.used);
			for (i = 1, found = 0; found <= memstat.used; i++) {
				gn_phonebook_entry entry;
				if (phonemgr_listener_get_phonebook_entry (l, memstat.memory_type, i, &entry) == FALSE) {
					break;
				} else if (entry.empty == FALSE) {
					char *uuid;
					uuid = g_strdup_printf ("GPM-UUID-%s-%s-%d", l->imei, "ME", entry.location);
					g_ptr_array_add (a, uuid);
					found++;
				}
			}

			memset (&memstat, 0, sizeof (memstat));
			memstat.memory_type = gn_str2memory_type("SM");
			l->phone_state->data.memory_status = &memstat;
			error = phonemgr_listener_gnokii_func (GN_OP_GetMemoryStatus, l);
			if (error != GN_ERR_NONE) {
				g_message ("GN_OP_GetMemoryStatus on SM failed");
				g_ptr_array_add (a, NULL);
				g_mutex_unlock (&l->mutex);
				return (char **) g_ptr_array_free (a, FALSE);
			}

			for (i = 1, found = 0; found <= memstat.used; i++) {
				gn_phonebook_entry entry;
				if (phonemgr_listener_get_phonebook_entry (l, memstat.memory_type, i, &entry) == FALSE) {
					break;
				} else if (entry.empty == FALSE) {
					char *uuid;
					uuid = g_strdup_printf ("GPM-UUID-%s-%s-%d", l->imei, "SM", i);
					g_ptr_array_add (a, uuid);
					found++;
				}
			}
			g_ptr_array_add (a, NULL);
			g_mutex_unlock (&l->mutex);

			return (char **) g_ptr_array_free (a, FALSE);
		}
	case PHONEMGR_LISTENER_DATA_CALENDAR:
		{
			GPtrArray *a;
			gn_calnote_list calnote_list;
			gn_calnote calnote;
			gn_error error;
			guint i;

			g_mutex_lock (&l->mutex);

			memset (&calnote, 0, sizeof (calnote));
			memset (&calnote_list, 0, sizeof (calnote_list));
			l->phone_state->data.calnote = &calnote;
			l->phone_state->data.calnote_list = &calnote_list;

			a = g_ptr_array_new ();

			for (i = 1; i < INT_MAX; i++) {
				calnote.location = i;
				error = phonemgr_listener_gnokii_func (GN_OP_GetCalendarNote, l);
				if (error != GN_ERR_NONE) {
					if ((i > 1) && ((error == GN_ERR_EMPTYLOCATION) || (error == GN_ERR_INVALIDLOCATION))) {
						g_message ("error %s", phonemgr_utils_gn_error_to_string (error, NULL));
					}
					break;
				} else {
					char *uuid;
					uuid = g_strdup_printf ("GPM-UUID-%s-%s-%d", l->imei, "XX", i);
					g_ptr_array_add (a, uuid);
				}
			}

			g_ptr_array_add (a, NULL);
			g_mutex_unlock (&l->mutex);

			return (char **) g_ptr_array_free (a, FALSE);
		}
		break;
	case PHONEMGR_LISTENER_DATA_TODO:
		break;
	default:
		g_assert_not_reached ();
	}

	return NULL;
}

gboolean
phonemgr_listener_delete_data (PhonemgrListener *l,
			       PhonemgrListenerDataType type,
			       const char *dataid)
{
	switch (type) {
	case PHONEMGR_LISTENER_DATA_CONTACT:
		{
			gn_phonebook_entry entry;
			gn_memory_type memory_type;
			gn_error error;
			int index;

			if (phonemgr_listener_parse_data_uuid (dataid, &memory_type, &index) == FALSE)
				return FALSE;

			g_mutex_lock (&l->mutex);

			memset(&entry, 0, sizeof(gn_phonebook_entry));
			entry.memory_type = memory_type;
			entry.location = index;
			entry.empty = TRUE;

			l->phone_state->data.phonebook_entry = &entry;
			error = phonemgr_listener_gnokii_func (GN_OP_DeletePhonebook, l);

			g_mutex_unlock (&l->mutex);

			return (error == GN_ERR_NONE);
		}
		break;
	case PHONEMGR_LISTENER_DATA_CALENDAR:
		{
			gn_calnote_list calnote_list;
			gn_calnote calnote;
			gn_error error;
			int index;

			if (phonemgr_listener_parse_data_uuid (dataid, NULL, &index) == FALSE)
				return FALSE;

			g_mutex_lock (&l->mutex);

			memset (&calnote, 0, sizeof (calnote));
			memset (&calnote_list, 0, sizeof (calnote_list));
			l->phone_state->data.calnote = &calnote;
			l->phone_state->data.calnote_list = &calnote_list;

			calnote.location = index;
			error = phonemgr_listener_gnokii_func (GN_OP_DeleteCalendarNote, l);

			g_mutex_unlock (&l->mutex);

			return (error == GN_ERR_NONE);
		}
		break;
	case PHONEMGR_LISTENER_DATA_TODO:
		break;
	default:
		g_assert_not_reached ();
	}

	return FALSE;
}

char *
phonemgr_listener_put_data (PhonemgrListener *l,
			    PhonemgrListenerDataType type,
			    const char *data)
{
	switch (type) {
	case PHONEMGR_LISTENER_DATA_CONTACT:
		{
			gn_phonebook_entry entry;
			char *retval;
			gn_error error;
			guint i;

			g_mutex_lock (&l->mutex);

			memset(&entry, 0, sizeof(gn_phonebook_entry));
			if (vcard_to_phonebook_entry (data, &entry) == FALSE) {
				g_message ("Couldn't parse the data...");
				return NULL;
			}
			entry.memory_type = GN_MT_ME;

			/* Find an empty spot */
			for (i = 1; ; i++) {
				gn_phonebook_entry aux;

				memcpy(&aux, &entry, sizeof(gn_phonebook_entry));
				l->phone_state->data.phonebook_entry = &aux;
				l->phone_state->data.phonebook_entry->location = i;
				error = phonemgr_listener_gnokii_func (GN_OP_ReadPhonebook, l);
				if (error != GN_ERR_NONE && error != GN_ERR_EMPTYLOCATION)
					break;
				if (aux.empty || error == GN_ERR_EMPTYLOCATION) {
					entry.location = aux.location;
					error = GN_ERR_NONE;
					break;
				}
			}

			if (error != GN_ERR_NONE) {
				g_mutex_unlock (&l->mutex);
				return NULL;
			}

			//FIXME This should sanitise the phone numbers
			gn_phonebook_entry_sanitize(&entry);

			l->phone_state->data.phonebook_entry = &entry;
			error = phonemgr_listener_gnokii_func (GN_OP_WritePhonebook, l);

			if (error != GN_ERR_NONE) {
				g_mutex_unlock (&l->mutex);
				return NULL;
			}

			retval = g_strdup_printf ("GPM-UUID-%s-%s-%d", l->imei, "ME", entry.location);

			g_mutex_unlock (&l->mutex);

			return retval;
		}
		break;
	case PHONEMGR_LISTENER_DATA_CALENDAR:
		break;
	case PHONEMGR_LISTENER_DATA_TODO:
		break;
	default:
		g_assert_not_reached ();
	}

	return FALSE;
}

gboolean
phonemgr_listener_connected (PhonemgrListener *l)
{
	g_return_val_if_fail (PHONEMGR_IS_LISTENER (l), FALSE);

	return l->connected;
}

#else /* !DUMMY */

/* We poll every 50 milliseconds, so this is the number of times
 * we need to go through poll() to wait for one second */
#define POLL_SECOND 1000 / 50

static gboolean
phonemgr_listener_poll (PhonemgrListener *l)
{
	static int i = 0;
	static int target = 5 * POLL_SECOND;

	if (l->connected == FALSE)
		return FALSE;

	i++;
	if (i == target) {
		char *sender;
		GTimeVal time;
		char *text;
		GRand *rand;

		rand = g_rand_new_with_seed (i);

		if (g_rand_boolean (rand)) {
			sender = "+09876 543-21";
			text = "This is my other t\303\251st, this is NOT a supa test: http://live.gnome.org/PhoneManager";
		} else {
			sender = "+01234 567-89";
			text = "This is my test, this is my supa test with i18n chars: \303\270\317\216\307\252 (http://live.gnome.org/PhoneManager)";
		}
		g_rand_free (rand);

		g_get_current_time (&time);

		g_message ("[DUMMY] receiving from %s: %s", sender, text);
		g_signal_emit (G_OBJECT (l),
				phonemgr_listener_signals[MESSAGE_SIGNAL],
				0, sender, time.tv_sec, text);
		target = 15 * POLL_SECOND;
	}

	return TRUE;
}

gboolean
phonemgr_listener_connect (PhonemgrListener *l, const char *device, GError **err)
{
	g_message ("[DUMMY] connecting to %s", device);
	phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_CONNECTING);
	g_usleep (G_USEC_PER_SEC * 2);
	l->connected = TRUE;
	phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_CONNECTED);

	g_timeout_add (50, (GSourceFunc) phonemgr_listener_poll, l);

	return TRUE;
}

void
phonemgr_listener_disconnect (PhonemgrListener *l)
{
	g_message ("[DUMMY] disconnecting");
	phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_DISCONNECTING);
	l->connected = FALSE;
	g_usleep (G_USEC_PER_SEC * 2);
	phonemgr_listener_emit_status (l, PHONEMGR_LISTENER_IDLE);
}

void
phonemgr_listener_queue_message (PhonemgrListener *listener,
				 const char *number,
				 const char *message,
				 gboolean delivery_report)
{
	g_message ("[DUMMY] sending message to %s: %s (Delivery report: %s)",
		   number, message, delivery_report ? "Yes" : "No");
}

gboolean
phonemgr_listener_connected (PhonemgrListener *listener)
{
	return listener->connected;
}

static void
phonemgr_listener_init (PhonemgrListener *l)
{
}

static void
phonemgr_listener_finalize(GObject *obj)
{
}

char *
phonemgr_listener_get_data (PhonemgrListener *l,
			    PhonemgrListenerDataType type,
			    const char *dataid)
{
	return NULL;
}

char **
phonemgr_listener_list_all_data (PhonemgrListener *l,
				 PhonemgrListenerDataType type)
{
	return NULL;
}

gboolean
phonemgr_listener_delete_data (PhonemgrListener *l,
			       PhonemgrListenerDataType type,
			       const char *dataid)
{
	return FALSE;
}

char *
phonemgr_listener_put_data (PhonemgrListener *l,
			    PhonemgrListenerDataType type,
			    const char *dataid,
			    const char *data)
{
	return NULL;
}

#endif /* !DUMMY */
