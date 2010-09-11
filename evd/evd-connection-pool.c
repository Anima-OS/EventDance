/*
 * evd-connection-pool.c
 *
 * EventDance project - An event distribution framework (http://eventdance.org)
 *
 * Copyright (C) 2009, Igalia S.L.
 *
 * Authors:
 *   Eduardo Lima Mitev <elima@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * version 3 as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License at http://www.gnu.org/licenses/lgpl-3.0.txt
 * for more details.
 */

#include "evd-connection-pool.h"

#include "evd-utils.h"
#include "evd-error.h"
#include "evd-socket.h"

G_DEFINE_TYPE (EvdConnectionPool, evd_connection_pool, EVD_TYPE_IO_STREAM_GROUP)

#define EVD_CONNECTION_POOL_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                              EVD_TYPE_CONNECTION_POOL, \
                                              EvdConnectionPoolPrivate))

#define DEFAULT_MIN_CONNS 1
#define DEFAULT_MAX_CONNS 5

#define TOTAL_SOCKETS(pool) (g_queue_get_length (pool->priv->sockets) + \
                             g_queue_get_length (pool->priv->conns))

#define HAS_REQUESTS(pool)  (g_queue_get_length (pool->priv->requests) > 0)

/* private data */
struct _EvdConnectionPoolPrivate
{
  gchar *target;

  guint min_conns;
  guint max_conns;

  GQueue *conns;
  GQueue *sockets;
  GQueue *requests;
};

static void     evd_connection_pool_class_init         (EvdConnectionPoolClass *class);
static void     evd_connection_pool_init               (EvdConnectionPool *self);

static void     evd_connection_pool_finalize           (GObject *obj);
static void     evd_connection_pool_dispose            (GObject *obj);

static void     evd_connection_pool_create_new_socket  (EvdConnectionPool *self);

static void     evd_connection_pool_reuse_socket       (EvdConnectionPool *self,
                                                        EvdSocket         *socket);

static void
evd_connection_pool_class_init (EvdConnectionPoolClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);

  obj_class->dispose = evd_connection_pool_dispose;
  obj_class->finalize = evd_connection_pool_finalize;

  /* add private structure */
  g_type_class_add_private (obj_class, sizeof (EvdConnectionPoolPrivate));
}

static void
evd_connection_pool_init (EvdConnectionPool *self)
{
  EvdConnectionPoolPrivate *priv;

  priv = EVD_CONNECTION_POOL_GET_PRIVATE (self);
  self->priv = priv;

  priv->min_conns = DEFAULT_MIN_CONNS;
  priv->max_conns = DEFAULT_MAX_CONNS;

  priv->conns = g_queue_new ();
  priv->sockets = g_queue_new ();
  priv->requests = g_queue_new ();
}

static void
evd_connection_pool_dispose (GObject *obj)
{
  /* @TODO */

  G_OBJECT_CLASS (evd_connection_pool_parent_class)->dispose (obj);
}

static void
evd_connection_pool_finalize (GObject *obj)
{
  /* @TODO */

  G_OBJECT_CLASS (evd_connection_pool_parent_class)->finalize (obj);
}

static void
evd_connection_pool_connection_on_close (EvdConnection *conn,
                                         gpointer       user_data)
{
  EvdConnectionPool *self = EVD_CONNECTION_POOL (user_data);

  g_queue_remove (self->priv->conns, conn);

  if (TOTAL_SOCKETS (self) < self->priv->min_conns)
    {
      EvdSocket *socket;

      socket = evd_connection_get_socket (conn);

      g_object_ref (socket);
      evd_connection_pool_reuse_socket (self, socket);
    }

  g_object_unref (conn);
}

static void
evd_connection_pool_finish_request (EvdConnectionPool  *self,
                                    EvdConnection      *conn,
                                    GSimpleAsyncResult *res)
{
  g_signal_handlers_disconnect_by_func (conn,
                                        evd_connection_pool_connection_on_close,
                                        self);

  g_simple_async_result_set_op_res_gpointer (res, conn, g_object_unref);
  g_simple_async_result_complete_in_idle (res);
  g_object_unref (res);
}

static void
evd_connection_pool_socket_on_connect (GObject      *obj,
                                       GAsyncResult *res,
                                       gpointer      user_data)
{
  EvdConnectionPool *self = EVD_CONNECTION_POOL (user_data);
  GIOStream *io_stream;
  GError *error = NULL;

  if ( (io_stream = evd_socket_connect_finish (EVD_SOCKET (obj),
                                               res,
                                               &error)) != NULL)
    {
      EvdConnection *conn;
      EvdSocket *socket;

      conn = EVD_CONNECTION (io_stream);
      socket = evd_connection_get_socket (conn);

      g_queue_remove (self->priv->sockets, socket);

      if (HAS_REQUESTS (self))
        {
          GSimpleAsyncResult *res;

          res = G_SIMPLE_ASYNC_RESULT (g_queue_pop_head (self->priv->requests));

          g_signal_handlers_disconnect_by_func (conn,
                                        evd_connection_pool_connection_on_close,
                                        self);

          evd_connection_pool_finish_request (self, conn, res);

          if (TOTAL_SOCKETS (self) < self->priv->min_conns)
            evd_connection_pool_create_new_socket (self);
        }
      else
        {
          g_signal_connect (conn,
                           "close",
                           G_CALLBACK (evd_connection_pool_connection_on_close),
                            self);
          g_queue_push_tail (self->priv->conns, conn);
        }
    }
  else
    {
      /* @TODO: handle error */
      g_debug ("error connection: %s", error->message);
      g_error_free (error);
    }
}

static void
evd_connection_pool_reuse_socket (EvdConnectionPool *self, EvdSocket *socket)
{
  g_queue_push_tail (self->priv->sockets, (gpointer) socket);

  evd_socket_connect_async (socket,
                            self->priv->target,
                            NULL,
                            evd_connection_pool_socket_on_connect,
                            self);
}

static void
evd_connection_pool_socket_on_close (EvdSocket *socket, gpointer user_data)
{
  guint total;

  EvdConnectionPool *self = EVD_CONNECTION_POOL (user_data);

  total = TOTAL_SOCKETS (self);

  if (total >= self->priv->max_conns ||
      (total >= self->priv->min_conns && ! HAS_REQUESTS (self)) )
    {
      g_object_unref (socket);
    }
  else
    {
      evd_connection_pool_reuse_socket (self, socket);
    }
}

static void
evd_connection_pool_create_new_socket (EvdConnectionPool *self)
{
  EvdSocket *socket;

  socket = evd_socket_new ();

  g_signal_connect (socket,
                    "close",
                    G_CALLBACK (evd_connection_pool_socket_on_close),
                    self);

  evd_connection_pool_reuse_socket (self, socket);
}

/* public methods */

EvdConnectionPool *
evd_connection_pool_new (const gchar *address)
{
  EvdConnectionPool *self;

  g_return_val_if_fail (address != NULL, NULL);

  self = g_object_new (EVD_TYPE_CONNECTION_POOL, NULL);

  self->priv->target = g_strdup (address);

  while (g_queue_get_length (self->priv->sockets) < self->priv->min_conns)
    evd_connection_pool_create_new_socket (self);

  return self;
}

gboolean
evd_connection_pool_has_free_connections (EvdConnectionPool *self)
{
  g_return_val_if_fail (EVD_IS_CONNECTION_POOL (self), FALSE);

  return (g_queue_get_length (self->priv->conns) > 0);
}

void
evd_connection_pool_get_connection_async (EvdConnectionPool   *self,
                                          GCancellable        *cancellable,
                                          GAsyncReadyCallback  callback,
                                          gpointer             user_data)
{
  GSimpleAsyncResult *res;

  g_return_if_fail (EVD_IS_CONNECTION_POOL (self));

  res = g_simple_async_result_new (G_OBJECT (self),
                                   callback,
                                   user_data,
                                   evd_connection_pool_get_connection_async);

  if (g_queue_get_length (self->priv->conns) > 0)
    {
      evd_connection_pool_finish_request (self,
                          EVD_CONNECTION (g_queue_pop_head (self->priv->conns)),
                          res);

      if (TOTAL_SOCKETS (self) < self->priv->min_conns)
        evd_connection_pool_create_new_socket (self);
    }
  else
    {
      g_queue_push_tail (self->priv->requests, res);

      if (TOTAL_SOCKETS (self) < self->priv->max_conns)
        evd_connection_pool_create_new_socket (self);
    }
}

EvdConnection *
evd_connection_pool_get_connection_finish (EvdConnectionPool  *self,
                                           GAsyncResult       *result,
                                           GError            **error)
{
  g_return_val_if_fail (EVD_IS_CONNECTION_POOL (self), NULL);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
                               G_OBJECT (self),
                               evd_connection_pool_get_connection_async),
                        NULL);

  if (! g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
                                               error))
    {
      EvdConnection *conn;

      conn =
        g_simple_async_result_get_op_res_gpointer (G_SIMPLE_ASYNC_RESULT (result));

      return conn;
    }
  else
    {
      return NULL;
    }
}
