/*
 * evd-websocket-server.h
 *
 * EventDance, Peer-to-peer IPC library <http://eventdance.org>
 *
 * Copyright (C) 2011, Igalia S.L.
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

#ifndef __EVD_WEBSOCKET_SERVER_H__
#define __EVD_WEBSOCKET_SERVER_H__

#include <evd-web-service.h>

G_BEGIN_DECLS

typedef struct _EvdWebsocketServer EvdWebsocketServer;
typedef struct _EvdWebsocketServerClass EvdWebsocketServerClass;

struct _EvdWebsocketServer
{
  EvdWebService parent;
};

struct _EvdWebsocketServerClass
{
  EvdWebServiceClass parent_class;
};

#define EVD_TYPE_WEBSOCKET_SERVER           (evd_websocket_server_get_type ())
#define EVD_WEBSOCKET_SERVER(obj)           (G_TYPE_CHECK_INSTANCE_CAST ((obj), EVD_TYPE_WEBSOCKET_SERVER, EvdWebsocketServer))
#define EVD_WEBSOCKET_SERVER_CLASS(obj)     (G_TYPE_CHECK_CLASS_CAST ((obj), EVD_TYPE_WEBSOCKET_SERVER, EvdWebsocketServerClass))
#define EVD_IS_WEBSOCKET_SERVER(obj)        (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVD_TYPE_WEBSOCKET_SERVER))
#define EVD_IS_WEBSOCKET_SERVER_CLASS(obj)  (G_TYPE_CHECK_CLASS_TYPE ((obj), EVD_TYPE_WEBSOCKET_SERVER))
#define EVD_WEBSOCKET_SERVER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), EVD_TYPE_WEBSOCKET_SERVER, EvdWebsocketServerClass))


GType                   evd_websocket_server_get_type          (void) G_GNUC_CONST;

EvdWebsocketServer     *evd_websocket_server_new               (void);

G_END_DECLS

#endif /* __EVD_WEBSOCKET_SERVER_H__ */