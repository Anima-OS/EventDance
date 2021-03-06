/*
 * evd-peer.c
 *
 * EventDance, Peer-to-peer IPC library <http://eventdance.org>
 *
 * Copyright (C) 2009/2010/2011/2012, Igalia S.L.
 *
 * Authors:
 *   Eduardo Lima Mitev <elima@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 3, or (at your option) any later version as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License at http://www.gnu.org/licenses/lgpl-3.0.txt
 * for more details.
 */

#include <string.h>

#include "evd-peer.h"

#include "evd-transport.h"
#include "evd-utils.h"

G_DEFINE_TYPE (EvdPeer, evd_peer, G_TYPE_OBJECT)

#define EVD_PEER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                   EVD_TYPE_PEER, \
                                   EvdPeerPrivate))

#define DEFAULT_TIMEOUT_INTERVAL 15

/* private data */
struct _EvdPeerPrivate
{
  gchar *id;

  gboolean closed;

  GQueue *backlog;

  GTimer *idle_timer;
  guint timeout_interval;

  EvdTransport *transport;
};

typedef struct
{
  EvdMessageType type;
  gsize len;
  gchar *buf;
} BacklogFrame;

/* properties */
enum
{
  PROP_0,
  PROP_ID,
  PROP_TRANSPORT
};

static void     evd_peer_class_init         (EvdPeerClass *class);
static void     evd_peer_init               (EvdPeer *self);

static void     evd_peer_finalize           (GObject *obj);
static void     evd_peer_dispose            (GObject *obj);

static void     evd_peer_set_property       (GObject      *obj,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);
static void     evd_peer_get_property       (GObject    *obj,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec);

static void     free_backlog_frame          (gpointer data,
                                             gpointer user_data);

static void
evd_peer_class_init (EvdPeerClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);

  obj_class->dispose = evd_peer_dispose;
  obj_class->finalize = evd_peer_finalize;
  obj_class->get_property = evd_peer_get_property;
  obj_class->set_property = evd_peer_set_property;

  g_object_class_install_property (obj_class, PROP_ID,
                                   g_param_spec_string ("id",
                                                        "Peer's UUID",
                                                        "A string representing the UUID of the peer",
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (obj_class, PROP_TRANSPORT,
                                   g_param_spec_object ("transport",
                                                        "Peer's transport",
                                                        "Transport object which this peer uses for sending and receiving data",
                                                        G_TYPE_OBJECT,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));


  /* add private structure */
  g_type_class_add_private (obj_class, sizeof (EvdPeerPrivate));
}

static void
evd_peer_init (EvdPeer *self)
{
  EvdPeerPrivate *priv;

  priv = EVD_PEER_GET_PRIVATE (self);
  self->priv = priv;

  self->priv->closed = FALSE;

  priv->backlog = g_queue_new ();

  priv->idle_timer = g_timer_new ();
  priv->timeout_interval = DEFAULT_TIMEOUT_INTERVAL;

  self->priv->id = evd_uuid_new ();
}

static void
evd_peer_dispose (GObject *obj)
{
  EvdPeer *self = EVD_PEER (obj);

  if (self->priv->transport != NULL)
    {
      g_object_unref (self->priv->transport);
      self->priv->transport = NULL;
    }

  G_OBJECT_CLASS (evd_peer_parent_class)->dispose (obj);
}

static void
evd_peer_finalize (GObject *obj)
{
  EvdPeer *self = EVD_PEER (obj);

  g_timer_destroy (self->priv->idle_timer);

  g_queue_foreach (self->priv->backlog,
                   free_backlog_frame,
                   NULL);
  g_queue_free (self->priv->backlog);

  g_free (self->priv->id);

  G_OBJECT_CLASS (evd_peer_parent_class)->finalize (obj);
}

static void
evd_peer_set_property (GObject      *obj,
                       guint         prop_id,
                       const GValue *value,
                       GParamSpec   *pspec)
{
  EvdPeer *self;

  self = EVD_PEER (obj);

  switch (prop_id)
    {
    case PROP_TRANSPORT:
      self->priv->transport = EVD_TRANSPORT (g_value_dup_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
evd_peer_get_property (GObject    *obj,
                       guint       prop_id,
                       GValue     *value,
                       GParamSpec *pspec)
{
  EvdPeer *self;

  self = EVD_PEER (obj);

  switch (prop_id)
    {
    case PROP_ID:
      g_value_set_string (value, evd_peer_get_id (self));
      break;

    case PROP_TRANSPORT:
      g_value_set_object (value, G_OBJECT (self->priv->transport));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
free_backlog_frame (gpointer data, gpointer user_data)
{
  BacklogFrame *frame = data;

  if (frame->buf != NULL)
    g_free (frame->buf);

  g_slice_free (BacklogFrame, frame);
}

static BacklogFrame *
create_new_backlog_frame (const gchar *message, gsize size, EvdMessageType type)
{
  BacklogFrame *frame;

  frame = g_slice_new0 (BacklogFrame);
  frame->type = type;
  frame->len = size;

  frame->buf = g_new (gchar, size + 1);
  memcpy (frame->buf, message, size);
  frame->buf[size] = '\0';

  return frame;
}

/* public methods */

const gchar *
evd_peer_get_id (EvdPeer *self)
{
  g_return_val_if_fail (EVD_IS_PEER (self), NULL);

  return self->priv->id;
}

/**
 * evd_peer_get_transport:
 *
 * Returns: (transfer none):
 **/
EvdTransport *
evd_peer_get_transport (EvdPeer *self)
{
  g_return_val_if_fail (EVD_IS_PEER (self), NULL);

  return self->priv->transport;
}

gboolean
evd_peer_backlog_push_frame (EvdPeer      *self,
                             const gchar  *frame,
                             gsize         size,
                             GError      **error)
{
  return evd_peer_push_message (self,
                                frame,
                                size,
                                EVD_MESSAGE_TYPE_TEXT,
                                error);
}

gboolean
evd_peer_backlog_unshift_frame (EvdPeer      *self,
                                const gchar  *frame,
                                gsize         size,
                                GError      **error)
{
  return evd_peer_unshift_message (self,
                                   frame,
                                   size,
                                   EVD_MESSAGE_TYPE_TEXT,
                                   error);
}

gchar *
evd_peer_backlog_pop_frame (EvdPeer *self,
                            gsize   *size)
{
  return evd_peer_pop_message (self, size, NULL);
}

guint
evd_peer_backlog_get_length (EvdPeer *self)
{
  g_return_val_if_fail (EVD_IS_PEER (self), 0);

  return g_queue_get_length (self->priv->backlog);
}

void
evd_peer_touch (EvdPeer *self)
{
  g_return_if_fail (EVD_IS_PEER (self));

  g_timer_start (self->priv->idle_timer);
}

gboolean
evd_peer_is_alive (EvdPeer *self)
{
  g_return_val_if_fail (EVD_IS_PEER (self), FALSE);

  return
    ! self->priv->closed &&
    (g_timer_elapsed (self->priv->idle_timer, NULL) <=
     self->priv->timeout_interval ||
     evd_transport_peer_is_connected (self->priv->transport,
                                      self));
}

gboolean
evd_peer_is_closed (EvdPeer *self)
{
  g_return_val_if_fail (EVD_IS_PEER (self), FALSE);

  return self->priv->closed;
}

gboolean
evd_peer_send (EvdPeer      *self,
               const gchar  *buffer,
               gsize         size,
               GError      **error)
{
  g_return_val_if_fail (EVD_IS_PEER (self), FALSE);

  return evd_transport_send (self->priv->transport,
                             self,
                             buffer,
                             size,
                             error);
}

gboolean
evd_peer_send_text (EvdPeer      *self,
                    const gchar  *buffer,
                    GError      **error)
{
  g_return_val_if_fail (EVD_IS_PEER (self), FALSE);

  return evd_transport_send_text (self->priv->transport,
                                  self,
                                  buffer,
                                  error);
}

void
evd_peer_close (EvdPeer *self, gboolean gracefully)
{
  g_return_if_fail (EVD_IS_PEER (self));

  if (! self->priv->closed)
    {
      self->priv->closed = TRUE;

      evd_transport_close_peer (self->priv->transport,
                                self,
                                gracefully,
                                NULL);
    }
}

/**
 * evd_peer_push_message:
 *
 * Returns:
 *
 * Since: 0.1.20
 **/
gboolean
evd_peer_push_message (EvdPeer         *self,
                       const gchar     *message,
                       gsize            size,
                       EvdMessageType   type,
                       GError         **error)
{
  BacklogFrame *frame;

  g_return_val_if_fail (EVD_IS_PEER (self), FALSE);
  g_return_val_if_fail (message != NULL, FALSE);

  /* TODO: check backlog limits here */

  frame = create_new_backlog_frame (message, size, type);

  g_queue_push_tail (self->priv->backlog, frame);

  return TRUE;
}

/**
 * evd_peer_unshift_message:
 *
 * Returns:
 *
 * Since: 0.1.20
 **/
gboolean
evd_peer_unshift_message (EvdPeer         *self,
                          const gchar     *message,
                          gsize            size,
                          EvdMessageType   type,
                          GError         **error)
{
  BacklogFrame *frame;

  g_return_val_if_fail (EVD_IS_PEER (self), FALSE);
  g_return_val_if_fail (message != NULL, FALSE);

  if (size == 0)
    return TRUE;

  /* TODO: check backlog limits here */

  frame = create_new_backlog_frame (message, size, type);

  g_queue_push_head (self->priv->backlog, frame);

  return TRUE;
}

/**
 * evd_peer_pop_message:
 * @size: (allow-none):
 * @type: (allow-none):
 *
 * Returns: (transfer full):
 *
 * Since: 0.1.20
 **/
gchar *
evd_peer_pop_message (EvdPeer *self, gsize *size, EvdMessageType *type)
{
  BacklogFrame *frame;

  g_return_val_if_fail (EVD_IS_PEER (self), NULL);

  frame = g_queue_pop_head (self->priv->backlog);

  if (frame != NULL)
    {
      gchar *str;

      str = frame->buf;

      if (size != NULL)
        *size = frame->len;

      if (type != NULL)
        *type = frame->type;

      g_slice_free (BacklogFrame, frame);

      return str;
    }
  else
    {
      return NULL;
    }
}
