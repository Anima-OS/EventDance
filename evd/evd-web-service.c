/*
 * evd-web-service.c
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

#include "evd-web-service.h"

#include "evd-error.h"

G_DEFINE_ABSTRACT_TYPE (EvdWebService, evd_web_service, EVD_TYPE_SERVICE)

#define RETURN_DATA_KEY "org.eventdance.lib.WebService.RETURN_TO"

static void     evd_web_service_class_init          (EvdWebServiceClass *class);
static void     evd_web_service_init                (EvdWebService *self);

static void     evd_web_service_connection_accepted (EvdService    *service,
                                                     EvdConnection *conn);

static void     evd_web_service_return_connection   (EvdWebService     *self,
                                                     EvdHttpConnection *conn);

static void
evd_web_service_class_init (EvdWebServiceClass *class)
{
  EvdServiceClass *service_class = EVD_SERVICE_CLASS (class);

  class->return_connection = evd_web_service_return_connection;

  service_class->connection_accepted = evd_web_service_connection_accepted;
}

static void
evd_web_service_init (EvdWebService *self)
{
  evd_service_set_io_stream_type (EVD_SERVICE (self), EVD_TYPE_HTTP_CONNECTION);
}

static void
evd_web_service_invoke_request_handler (EvdWebService     *self,
                                        EvdHttpConnection *conn,
                                        EvdHttpRequest    *request)
{
  EvdWebServiceClass *class;

  class = EVD_WEB_SERVICE_GET_CLASS (self);
  if (class->request_handler != NULL)
    {
      (* class->request_handler) (self, conn, request);
    }
  else
    {
      g_object_unref (request);
    }
}

static void
evd_web_service_conn_on_headers_read (GObject      *obj,
                                      GAsyncResult *res,
                                      gpointer      user_data)
{
  EvdWebService *self = EVD_WEB_SERVICE (user_data);
  EvdHttpConnection *conn = EVD_HTTP_CONNECTION (obj);
  EvdHttpRequest *request;
  GError *error = NULL;

  if ( (request =
        evd_http_connection_read_request_headers_finish (conn,
                                                         res,
                                                         &error)) != NULL)
    {
      evd_web_service_invoke_request_handler (self, conn, request);
    }
  else
    {
      if (error->domain != EVD_ERROR || error->code != EVD_ERROR_CLOSED)
        g_debug ("error reading request headers: %s", error->message);

      g_error_free (error);

      g_io_stream_close (G_IO_STREAM (conn), NULL, NULL);
    }

  g_object_unref (conn);
}

static void
evd_web_service_connection_accepted (EvdService *service, EvdConnection *conn)
{
  EvdWebService *self = EVD_WEB_SERVICE (service);
  EvdHttpRequest *request;

  request = evd_http_connection_get_current_request (EVD_HTTP_CONNECTION (conn));

  if (request != NULL)
    {
      evd_web_service_invoke_request_handler (self,
                                              EVD_HTTP_CONNECTION (conn),
                                              request);
    }
  else
    {
      g_object_ref (conn);
      evd_http_connection_read_request_headers_async (EVD_HTTP_CONNECTION (conn),
                                           NULL,
                                           evd_web_service_conn_on_headers_read,
                                           self);
    }
}

static void
evd_web_service_return_connection (EvdWebService     *self,
                                   EvdHttpConnection *conn)
{
  EvdService *return_to;

  return_to = EVD_SERVICE (g_object_get_data (G_OBJECT (conn),
                                              RETURN_DATA_KEY));
  if (return_to != NULL)
    {
      g_object_set_data (G_OBJECT (conn), RETURN_DATA_KEY, NULL);
      evd_http_connection_set_current_request (conn, NULL);

      evd_io_stream_group_add (EVD_IO_STREAM_GROUP (return_to),
                               G_IO_STREAM (conn));
    }
  else
    {
      evd_connection_flush_and_shutdown (EVD_CONNECTION (conn), NULL);
    }
}

static void
evd_web_service_on_service_destroyed (gpointer  data,
                                      GObject  *where_the_object_was)
{
  EvdHttpConnection *conn = EVD_HTTP_CONNECTION (data);

  g_object_set_data (G_OBJECT (conn), RETURN_DATA_KEY, NULL);
}

static void
evd_web_service_on_conn_destroyed (gpointer  data,
                                   GObject  *where_the_object_was)
{
  EvdHttpConnection *conn = EVD_HTTP_CONNECTION (where_the_object_was);
  EvdService *service;

  service = EVD_SERVICE (g_object_get_data (G_OBJECT (conn), RETURN_DATA_KEY));

  if (service != NULL)
    g_object_weak_unref (G_OBJECT (service),
                         evd_web_service_on_service_destroyed,
                         conn);
}

/* public methods */

EvdWebService *
evd_web_service_new (void)
{
  EvdWebService *self;

  self = g_object_new (EVD_TYPE_WEB_SERVICE, NULL);

  return self;
}

gboolean
evd_web_service_add_connection_with_request (EvdWebService     *self,
                                             EvdHttpConnection *conn,
                                             EvdHttpRequest    *request,
                                             EvdService        *return_to)
{
  EvdService *old_service;

  g_return_val_if_fail (EVD_IS_WEB_SERVICE (self), FALSE);
  g_return_val_if_fail (EVD_IS_HTTP_CONNECTION (conn), FALSE);
  g_return_val_if_fail (EVD_IS_HTTP_REQUEST (request), FALSE);

  evd_http_connection_set_current_request (conn, request);

  old_service = EVD_SERVICE (g_object_get_data (G_OBJECT (conn),
                                                RETURN_DATA_KEY));
  if (old_service != NULL)
    g_object_weak_unref (G_OBJECT (old_service),
                         evd_web_service_on_service_destroyed,
                         conn);

  g_object_set_data (G_OBJECT (conn), RETURN_DATA_KEY, return_to);

  g_object_weak_ref (G_OBJECT (conn),
                     evd_web_service_on_conn_destroyed,
                     NULL);

  if (return_to != NULL)
    g_object_weak_ref (G_OBJECT (return_to),
                       evd_web_service_on_service_destroyed,
                       conn);

  return evd_io_stream_group_add (EVD_IO_STREAM_GROUP (self),
                                  G_IO_STREAM (conn));
}
