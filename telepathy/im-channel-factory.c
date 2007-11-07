/*
 * im-channel-factory.c - SmsImChannelFactory source
 * Copyright (C) 2007 Will Thompson
 * Copyright (C) 2007 Collabora Ltd.
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

#include <string.h>

#include <telepathy-glib/channel-factory-iface.h>
#include <telepathy-glib/interfaces.h>
#include <telepathy-glib/handle-repo.h>
#include <telepathy-glib/base-connection.h>

#include "debug.h"
#include "im-channel.h"
#include "im-channel-factory.h"
#include "connection.h"

typedef struct _SmsImChannelFactoryPrivate SmsImChannelFactoryPrivate;
struct _SmsImChannelFactoryPrivate {
    SmsConnection *conn;
    GHashTable *channels;
    gboolean dispose_has_run;
};
#define SMS_IM_CHANNEL_FACTORY_GET_PRIVATE(o) \
    (G_TYPE_INSTANCE_GET_PRIVATE((o), SMS_TYPE_IM_CHANNEL_FACTORY, \
                                 SmsImChannelFactoryPrivate))

static void sms_im_channel_factory_iface_init (gpointer g_iface,
                                                gpointer iface_data);

G_DEFINE_TYPE_WITH_CODE(SmsImChannelFactory,
    sms_im_channel_factory,
    G_TYPE_OBJECT,
    G_IMPLEMENT_INTERFACE (TP_TYPE_CHANNEL_FACTORY_IFACE,
      sms_im_channel_factory_iface_init));

/* properties: */
enum {
    PROP_CONNECTION = 1,

    LAST_PROPERTY
};

static SmsIMChannel *get_im_channel (SmsImChannelFactory *self, TpHandle handle, gboolean *created);
static void set_im_channel_factory_listener (SmsImChannelFactory *fac, GObject *conn);
static void new_sms (GObject *l, char *phone, time_t timestamp, char *message, gpointer data);

static void
sms_im_channel_factory_init (SmsImChannelFactory *self)
{
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE(self);

    priv->channels = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);

    priv->conn = NULL;
    priv->dispose_has_run = FALSE;
}

static void
sms_im_channel_factory_dispose (GObject *object)
{
    SmsImChannelFactory *self = SMS_IM_CHANNEL_FACTORY (object);
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE(self);

    if (priv->dispose_has_run)
        return;

    priv->dispose_has_run = TRUE;

    tp_channel_factory_iface_close_all (TP_CHANNEL_FACTORY_IFACE (object));
    g_assert (priv->channels == NULL);

    if (G_OBJECT_CLASS (sms_im_channel_factory_parent_class)->dispose)
        G_OBJECT_CLASS (sms_im_channel_factory_parent_class)->dispose (object);
}

static void
sms_im_channel_factory_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    SmsImChannelFactory *fac = SMS_IM_CHANNEL_FACTORY (object);
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE (fac);

    switch (property_id) {
        case PROP_CONNECTION:
            g_value_set_object (value, priv->conn);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
sms_im_channel_factory_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
    SmsImChannelFactory *fac = SMS_IM_CHANNEL_FACTORY (object);
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE (fac);

    switch (property_id) {
        case PROP_CONNECTION:
            priv->conn = g_value_get_object (value);
            set_im_channel_factory_listener (fac, G_OBJECT (priv->conn));
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
            break;
    }
}

static void
sms_im_channel_factory_class_init (SmsImChannelFactoryClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GParamSpec *param_spec;
//    void *conv_handle = purple_conversations_get_handle();

    object_class->dispose = sms_im_channel_factory_dispose;
    object_class->get_property = sms_im_channel_factory_get_property;
    object_class->set_property = sms_im_channel_factory_set_property;

    param_spec = g_param_spec_object ("connection", "SmsConnection object",
                                      "Sms connection object that owns this "
                                      "IM channel factory object.",
                                      SMS_TYPE_CONNECTION,
                                      G_PARAM_CONSTRUCT_ONLY |
                                      G_PARAM_READWRITE |
                                      G_PARAM_STATIC_NICK |
                                      G_PARAM_STATIC_BLURB);
    g_object_class_install_property (object_class, PROP_CONNECTION, param_spec);

    g_type_class_add_private (object_class,
                              sizeof(SmsImChannelFactoryPrivate));
}

static void
im_channel_closed_cb (SmsIMChannel *chan, gpointer user_data)
{
    SmsImChannelFactory *fac = SMS_IM_CHANNEL_FACTORY (user_data);
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE (fac);
    TpHandle contact_handle;

    if (priv->channels)
    {
        g_object_get (chan, "handle", &contact_handle, NULL);

        DEBUG ("removing channel with handle %d", contact_handle);

        g_hash_table_remove (priv->channels, GINT_TO_POINTER (contact_handle));
    }
}

static SmsIMChannel *
new_im_channel (SmsImChannelFactory *self,
                guint handle)
{
    SmsImChannelFactoryPrivate *priv;
    TpBaseConnection *conn;
    SmsIMChannel *chan;
    char *object_path;

    g_assert (SMS_IS_IM_CHANNEL_FACTORY (self));

    priv = SMS_IM_CHANNEL_FACTORY_GET_PRIVATE (self);
    conn = (TpBaseConnection *)priv->conn;

    g_assert (!g_hash_table_lookup (priv->channels, GINT_TO_POINTER (handle)));

    object_path = g_strdup_printf ("%s/ImChannel%u", conn->object_path, handle);

    chan = g_object_new (SMS_TYPE_IM_CHANNEL,
                         "connection", priv->conn,
                         "object-path", object_path,
                         "handle", handle,
                         NULL);

    DEBUG ("Created IM channel with object path %s", object_path);

    g_signal_connect (chan, "closed", G_CALLBACK (im_channel_closed_cb), self);

    g_hash_table_insert (priv->channels, GINT_TO_POINTER (handle), chan);

    tp_channel_factory_iface_emit_new_channel (self, (TpChannelIface *)chan,
            NULL);

    g_free (object_path);

    return chan;
}

static SmsIMChannel *
get_im_channel (SmsImChannelFactory *self,
                TpHandle handle,
                gboolean *created)
{
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE (self);
    SmsIMChannel *chan =
        g_hash_table_lookup (priv->channels, GINT_TO_POINTER (handle));

    if (chan)
    {
        if (created)
            *created = FALSE;
    }
    else
    {
        chan = new_im_channel (self, handle);
        if (created)
            *created = TRUE;
    }
    g_assert (chan);
    return chan;
}

static void
sms_im_channel_factory_iface_close_all (TpChannelFactoryIface *iface)
{
    SmsImChannelFactory *fac = SMS_IM_CHANNEL_FACTORY (iface);
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE (fac);
    GHashTable *tmp;

    DEBUG ("closing im channels");

    if (priv->conn)
    {
    	GObject *listener;

    	g_object_get (priv->conn, "listener", &listener, NULL);
    	g_signal_handlers_disconnect_by_func(listener, new_sms, fac);
    }
    if (priv->channels)
    {
        tmp = priv->channels;
        priv->channels = NULL;
        g_hash_table_destroy (tmp);
    }
}

static void
sms_im_channel_factory_iface_connecting (TpChannelFactoryIface *iface)
{
    /* SmsImChannelFactory *self = SMS_IM_CHANNEL_FACTORY (iface); */
}

static void
sms_im_channel_factory_iface_disconnected (TpChannelFactoryIface *iface)
{
    /* SmsImChannelFactory *self = SMS_IM_CHANNEL_FACTORY (iface); */
}

struct _ForeachData
{
    TpChannelFunc foreach;
    gpointer user_data;
};

static void
_foreach_slave (gpointer key, gpointer value, gpointer user_data)
{
    struct _ForeachData *data = (struct _ForeachData *) user_data;
    TpChannelIface *chan = TP_CHANNEL_IFACE (value);

    data->foreach (chan, data->user_data);
}

static void
sms_im_channel_factory_iface_foreach (TpChannelFactoryIface *iface,
                                       TpChannelFunc foreach,
                                       gpointer user_data)
{
    SmsImChannelFactory *fac = SMS_IM_CHANNEL_FACTORY (iface);
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE (fac);
    struct _ForeachData data;

    data.user_data = user_data;
    data.foreach = foreach;

    g_hash_table_foreach (priv->channels, _foreach_slave, &data);
}

static TpChannelFactoryRequestStatus
sms_im_channel_factory_iface_request (TpChannelFactoryIface *iface,
                                       const gchar *chan_type,
                                       TpHandleType handle_type,
                                       guint handle,
                                       gpointer request,
                                       TpChannelIface **ret,
                                       GError **error)
{
    SmsImChannelFactory *self = SMS_IM_CHANNEL_FACTORY (iface);
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE (self);
    TpHandleRepoIface *contact_repo = tp_base_connection_get_handles (
            (TpBaseConnection *)priv->conn, TP_HANDLE_TYPE_CONTACT);
    SmsIMChannel *chan;
    TpChannelFactoryRequestStatus status;
    gboolean created;

    if (strcmp (chan_type, TP_IFACE_CHANNEL_TYPE_TEXT))
        return TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_IMPLEMENTED;

    if (handle_type != TP_HANDLE_TYPE_CONTACT)
        return TP_CHANNEL_FACTORY_REQUEST_STATUS_NOT_AVAILABLE;

    if (!tp_handle_is_valid (contact_repo, handle, error))
        return TP_CHANNEL_FACTORY_REQUEST_STATUS_ERROR;

    chan = get_im_channel (self, handle, &created);
    if (created)
    {
        status = TP_CHANNEL_FACTORY_REQUEST_STATUS_CREATED;
    }
    else
    {
        status = TP_CHANNEL_FACTORY_REQUEST_STATUS_EXISTING;
    }

    *ret = TP_CHANNEL_IFACE (chan);
    return status;
}

static void
sms_im_channel_factory_iface_init (gpointer g_iface,
                                    gpointer iface_data)
{
    TpChannelFactoryIfaceClass *klass = (TpChannelFactoryIfaceClass *) g_iface;

    klass->close_all = sms_im_channel_factory_iface_close_all;
    klass->connecting = sms_im_channel_factory_iface_connecting;
    klass->connected = NULL; //sms_im_channel_factory_iface_connected;
    klass->disconnected = sms_im_channel_factory_iface_disconnected;
    klass->foreach = sms_im_channel_factory_iface_foreach;
    klass->request = sms_im_channel_factory_iface_request;
}

static void
new_sms (GObject *l, char *phone, time_t timestamp, char *message, gpointer data)
{
    SmsImChannelFactory *self = SMS_IM_CHANNEL_FACTORY (data);
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE (self);
    TpChannelTextMessageType type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
    SmsIMChannel *chan = NULL;
    TpHandle contact_handle;
    TpHandleRepoIface *contact_repo;

    contact_repo =
        tp_base_connection_get_handles (TP_BASE_CONNECTION (priv->conn), TP_HANDLE_TYPE_CONTACT);

	contact_handle = tp_handle_ensure (contact_repo, phone, NULL, NULL);

//        DEBUG ("channel %u: ignoring message %s with flags %u",
//            ui_data->contact_handle, message, flags);

//    SmsConversationUiData *ui_data = PURPLE_CONV_GET_SMS_UI_DATA (conv);

    chan = get_im_channel (self, contact_handle, NULL);

        tp_text_mixin_receive (G_OBJECT (chan), type, contact_handle,
                               timestamp, message);
        DEBUG ("channel %u: ignoring message %s",
            contact_handle, message);
}

static void
set_im_channel_factory_listener (SmsImChannelFactory *fac, GObject *conn)
{
	GObject *listener;

	g_object_get (conn, "listener", &listener, NULL);
	g_signal_connect (G_OBJECT (listener), "message",
			  G_CALLBACK (new_sms), fac);
}

#if 0
static void
sms_write_im (PurpleConversation *conv,
               const char *who,
               const char *xhtml_message,
               PurpleMessageFlags flags,
               time_t mtime)
{
    PurpleAccount *account = purple_conversation_get_account (conv);

    SmsImChannelFactory *im_factory =
        ACCOUNT_GET_SMS_CONNECTION (account)->im_factory;
    TpChannelTextMessageType type = TP_CHANNEL_TEXT_MESSAGE_TYPE_NORMAL;
    SmsIMChannel *chan = NULL;
    char *message;

    SmsConversationUiData *ui_data = PURPLE_CONV_GET_SMS_UI_DATA (conv);

    message = purple_markup_strip_html (xhtml_message);

    if (flags & PURPLE_MESSAGE_AUTO_RESP)
        type = TP_CHANNEL_TEXT_MESSAGE_TYPE_AUTO_REPLY;
    else if (purple_message_meify(message, -1))
        type = TP_CHANNEL_TEXT_MESSAGE_TYPE_ACTION;

    chan = get_im_channel (im_factory, ui_data->contact_handle, NULL);

    if (flags & PURPLE_MESSAGE_RECV)
        tp_text_mixin_receive (G_OBJECT (chan), type, ui_data->contact_handle,
                               mtime, message);
    else if (flags & PURPLE_MESSAGE_SEND)
        tp_svc_channel_type_text_emit_sent (chan, mtime, type, message);
    else if (flags & PURPLE_MESSAGE_ERROR)
        /* This is wrong.  The mtime, type and message are of the error message
         * (such as "Unable to send message: The message is too large.") not of
         * the message causing the error, and the ChannelTextSendError parameter
         * shouldn't always be unknown.  But this is the best that can be done
         * until I fix libpurple.
         */
        tp_svc_channel_type_text_emit_send_error (chan,
            TP_CHANNEL_TEXT_SEND_ERROR_UNKNOWN, mtime, type, message);
    else
        DEBUG ("channel %u: ignoring message %s with flags %u",
            ui_data->contact_handle, message, flags);

    g_free (message);
}

static void
sms_write_conv (PurpleConversation *conv,
                 const char *name,
                 const char *alias,
                 const char *message,
                 PurpleMessageFlags flags,
                 time_t mtime)
{
    PurpleConversationType type = purple_conversation_get_type (conv);
    switch (type)
    {
        case PURPLE_CONV_TYPE_IM:
            sms_write_im (conv, name, message, flags, mtime);
            break;
        default:
            DEBUG ("ignoring message to conv type %u (flags=%u; message=%s)",
                type, flags, message);
    }
}

static void
sms_create_conversation (PurpleConversation *conv)
{
    PurpleAccount *account = purple_conversation_get_account (conv);

    SmsImChannelFactory *im_factory =
        ACCOUNT_GET_SMS_CONNECTION (account)->im_factory;
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE (im_factory);
    TpBaseConnection *base_conn = TP_BASE_CONNECTION (priv->conn);
    TpHandleRepoIface *contact_repo =
        tp_base_connection_get_handles (base_conn, TP_HANDLE_TYPE_CONTACT);

    const gchar *who = purple_conversation_get_name (conv);

    SmsConversationUiData *ui_data;

    DEBUG ("(PurpleConversation *)%p created", conv);

    if (conv->type != PURPLE_CONV_TYPE_IM)
    {
        DEBUG ("not an IM conversation; ignoring");
        return;
    }

    g_assert (who);

    conv->ui_data = ui_data = g_slice_new0 (SmsConversationUiData);

    ui_data->contact_handle = tp_handle_ensure (contact_repo, who, NULL, NULL);
    g_assert (ui_data->contact_handle);
}

static void
sms_destroy_conversation (PurpleConversation *conv)
{
    PurpleAccount *account = purple_conversation_get_account (conv);

    SmsImChannelFactory *im_factory =
        ACCOUNT_GET_SMS_CONNECTION (account)->im_factory;
    SmsImChannelFactoryPrivate *priv =
        SMS_IM_CHANNEL_FACTORY_GET_PRIVATE (im_factory);
    TpBaseConnection *base_conn = TP_BASE_CONNECTION (priv->conn);
    TpHandleRepoIface *contact_repo =
        tp_base_connection_get_handles (base_conn, TP_HANDLE_TYPE_CONTACT);

    SmsConversationUiData *ui_data;

    DEBUG ("(PurpleConversation *)%p destroyed", conv);
    if (conv->type != PURPLE_CONV_TYPE_IM)
    {
        DEBUG ("not an IM conversation; ignoring");
        return;
    }

    ui_data = PURPLE_CONV_GET_SMS_UI_DATA (conv);

    tp_handle_unref (contact_repo, ui_data->contact_handle);
    if (ui_data->resend_typing_timeout_id)
        g_source_remove (ui_data->resend_typing_timeout_id);

    g_slice_free (SmsConversationUiData, ui_data);
    conv->ui_data = NULL;
}

#endif