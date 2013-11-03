/*
 * evd-error.h
 *
 * EventDance, Peer-to-peer IPC library <http://eventdance.org>
 *
 * Copyright (C) 2009-2013, Igalia S.L.
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

#ifndef __EVD_ERROR_H__
#define __EVD_ERROR_H__

#include <glib.h>
#include <gnutls/gnutls.h>
#include <gcrypt.h>

#define EVD_ERROR_DOMAIN_STR "org.eventdance.lib.Evd.ErrorDomain"
#define EVD_ERROR            g_quark_from_string (EVD_ERROR_DOMAIN_STR)

#define EVD_GNUTLS_ERROR_DOMAIN_STR "org.eventdance.lib.Gnutls.ErrorDomain"
#define EVD_GNUTLS_ERROR            g_quark_from_string (EVD_GNUTLS_ERROR_DOMAIN_STR)

#define EVD_GCRYPT_ERROR_DOMAIN_STR "org.eventdance.lib.Gcry.ErrorDomain"
#define EVD_GCRYPT_ERROR            g_quark_from_string (EVD_GCRYPT_ERROR_DOMAIN_STR)

#define EVD_ERRNO_ERROR_DOMAIN_STR "org.eventdance.lib.Errno.ErrorDomain"
#define EVD_ERRNO_ERROR            g_quark_from_string (EVD_ERRNO_ERROR_DOMAIN_STR)

typedef enum
{
  EVD_ERROR_NONE,
  EVD_ERROR_ABSTRACT,
  EVD_ERROR_EPOLL,
  EVD_ERROR_NOT_CONNECTING,
  EVD_ERROR_NOT_CONNECTED,
  EVD_ERROR_SOCKET_ACCEPT,
  EVD_ERROR_TOO_LONG,
  EVD_ERROR_BUFFER_FULL,
  EVD_ERROR_RESOLVE_ADDRESS,

  /* padding for future expansiion */
  EVD_ERROR_PADDING0,
  EVD_ERROR_PADDING1,
  EVD_ERROR_PADDING2,
  EVD_ERROR_PADDING3,
  EVD_ERROR_PADDING4,
  EVD_ERROR_PADDING5,
  EVD_ERROR_PADDING6,
  EVD_ERROR_PADDING7,
  EVD_ERROR_PADDING8,
  EVD_ERROR_PADDING9,
} EvdErrorEnum;


void         evd_error_build_gcrypt         (guint    gcrypt_error,
                                             GError **error);

gboolean     evd_error_propagate_gnutls     (gint     gnutls_error_code,
                                             GError **error);

#endif /* __EVD_ERROR_H__ */
