/*
 * test-resolver.c
 *
 * EventDance, Peer-to-peer IPC library <http://eventdance.org>
 *
 * Copyright (C) 2009-2014, Igalia S.L.
 *
 * Authors:
 *   Eduardo Lima Mitev <elima@igalia.com>
 */

#include <glib.h>

#ifdef HAVE_GIO_UNIX
#include <gio/gunixsocketaddress.h>
#endif

#include "evd-resolver.h"

#define UNIX_ADDR              "/this-is-any-unix-addr"
#define IPV4_1                 "192.168.0.1:1234"
#define IPV6_1                 "[::1]:4321"
#define RESOLVE_GOOD_LOCALHOST "localhost:80"
#define CANCEL                 "172.16.1.1:22"
#define RESOLVE_CANCEL         "localhost:80"
#define NONEXISTANT_1          "127.0.0.0.1"
#define NONEXISTANT_2          "nonexistentdomain"

typedef struct
{
  GMainLoop   *main_loop;
  EvdResolver *resolver;
} Fixture;

static void
fixture_setup (Fixture       *f,
               gconstpointer  test_data)
{
  f->resolver = evd_resolver_get_default ();
  f->main_loop = g_main_loop_new (NULL, FALSE);
}

static void
fixture_teardown (Fixture       *f,
                  gconstpointer  test_data)
{
  g_object_unref (f->resolver);
  g_main_loop_unref (f->main_loop);
}

static void
get_default (Fixture       *f,
             gconstpointer  test_data)
{
  EvdResolver *other_resolver;

  g_assert (EVD_IS_RESOLVER (f->resolver));
  g_assert_cmpint (G_OBJECT (f->resolver)->ref_count, ==, 1);

  other_resolver = evd_resolver_get_default ();
  g_assert (f->resolver == other_resolver);
  g_assert_cmpint (G_OBJECT (f->resolver)->ref_count, ==, 2);

  g_object_unref (other_resolver);
}

#ifdef HAVE_GIO_UNIX

/* unix-addr */

static void
unix_addr_on_resolve (GObject      *obj,
                      GAsyncResult *res,
                      gpointer      user_data)
{
  EvdResolver *resolver;
  Fixture *f = (Fixture *) user_data;
  GError *error = NULL;
  GList *addresses;
  GSocketAddress *addr;

  g_assert (EVD_IS_RESOLVER (obj));
  resolver = EVD_RESOLVER (obj);
  g_assert (resolver == f->resolver);

  addresses = evd_resolver_resolve_finish (resolver, res, &error);
  g_assert (addresses != NULL);
  g_assert_no_error (error);

  g_assert_cmpint (g_list_length (addresses), ==, 1);

  addr = G_SOCKET_ADDRESS (addresses->data);
  g_assert (G_IS_SOCKET_ADDRESS (addr));
  g_assert_cmpint (g_socket_address_get_family (addr),
                   ==,
                   G_SOCKET_FAMILY_UNIX);
  g_assert_cmpstr (g_unix_socket_address_get_path (G_UNIX_SOCKET_ADDRESS (addr)),
                   ==,
                   UNIX_ADDR);

  g_main_loop_quit (f->main_loop);

  g_resolver_free_addresses (addresses);
}

static void
unix_addr (Fixture       *f,
           gconstpointer  test_data)
{
  evd_resolver_resolve (f->resolver,
                        UNIX_ADDR,
                        NULL,
                        unix_addr_on_resolve,
                        f);

  g_main_loop_run (f->main_loop);
}

#endif

/* ipv4 */

static void
ipv4_on_resolve (GObject      *obj,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  EvdResolver *resolver;
  Fixture *f = (Fixture *) user_data;
  GError *error = NULL;
  GList *addresses;
  GSocketAddress *addr;
  GInetAddress *inet_addr;
  gchar *addr_str;

  g_assert (EVD_IS_RESOLVER (obj));
  resolver = EVD_RESOLVER (obj);
  g_assert (resolver == f->resolver);

  addresses = evd_resolver_resolve_finish (resolver, res, &error);
  g_assert (addresses != NULL);
  g_assert_no_error (error);

  g_assert_cmpint (g_list_length (addresses), ==, 1);

  addr = G_SOCKET_ADDRESS (addresses->data);
  g_assert (G_IS_SOCKET_ADDRESS (addr));
  g_assert_cmpint (g_socket_address_get_family (addr),
                   ==,
                   G_SOCKET_FAMILY_IPV4);
  g_assert_cmpint (g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (addr)),
                   ==,
                   1234);

  inet_addr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (addr));
  addr_str = g_inet_address_to_string (inet_addr);
  g_assert_cmpstr (addr_str,
                   ==,
                   "192.168.0.1");
  g_free (addr_str);
  g_object_unref (inet_addr);

  g_main_loop_quit (f->main_loop);
}

static void
ipv4 (Fixture       *f,
      gconstpointer  test_data)
{
  evd_resolver_resolve (f->resolver,
                        IPV4_1,
                        NULL,
                        ipv4_on_resolve,
                        f);

  g_main_loop_run (f->main_loop);
}

/* ipv6 */

static void
ipv6_on_resolve (GObject      *obj,
                 GAsyncResult *res,
                 gpointer      user_data)
{
  EvdResolver *resolver;
  Fixture *f = (Fixture *) user_data;
  GError *error = NULL;
  GList *addresses;
  GSocketAddress *addr;
  GInetAddress *inet_addr;
  gchar *addr_str;

  g_assert (EVD_IS_RESOLVER (obj));
  resolver = EVD_RESOLVER (obj);
  g_assert (resolver == f->resolver);

  addresses = evd_resolver_resolve_finish (resolver, res, &error);
  g_assert (addresses != NULL);
  g_assert_no_error (error);

  g_assert_cmpint (g_list_length (addresses), ==, 1);

  addr = G_SOCKET_ADDRESS (addresses->data);
  g_assert (G_IS_SOCKET_ADDRESS (addr));
  g_assert_cmpint (g_socket_address_get_family (addr),
                   ==,
                   G_SOCKET_FAMILY_IPV6);
  g_assert_cmpint (g_inet_socket_address_get_port (G_INET_SOCKET_ADDRESS (addr)),
                   ==,
                   4321);

  inet_addr = g_inet_socket_address_get_address (G_INET_SOCKET_ADDRESS (addr));
  addr_str = g_inet_address_to_string (inet_addr);
  g_assert_cmpstr (addr_str,
                   ==,
                   "::1");
  g_free (addr_str);
  g_object_unref (inet_addr);

  g_main_loop_quit (f->main_loop);
}

static void
ipv6 (Fixture       *f,
      gconstpointer  test_data)
{
  evd_resolver_resolve (f->resolver,
                        IPV6_1,
                        NULL,
                        ipv6_on_resolve,
                        f);

  g_main_loop_run (f->main_loop);
}

/* resolve good localhost */

static void
resolve_good_localhost_on_resolve (GObject      *obj,
                                   GAsyncResult *res,
                                   gpointer      user_data)
{
  EvdResolver *resolver;
  Fixture *f = (Fixture *) user_data;
  GError *error = NULL;
  GList *addresses;

  g_assert (EVD_IS_RESOLVER (obj));
  resolver = EVD_RESOLVER (obj);
  g_assert (resolver == f->resolver);

  addresses = evd_resolver_resolve_finish (resolver, res, &error);
  g_assert (addresses != NULL);
  g_assert_no_error (error);

  g_assert_cmpint (g_list_length (addresses), >=, 1);

  g_main_loop_quit (f->main_loop);
}

static void
resolve_good_localhost (Fixture       *f,
                        gconstpointer  test_data)
{
  evd_resolver_resolve (f->resolver,
                        RESOLVE_GOOD_LOCALHOST,
                        NULL,
                        resolve_good_localhost_on_resolve,
                        f);

  g_main_loop_run (f->main_loop);
}

/* cancel */

/*
static void
resolve_cancel_on_resolve (GObject      *obj,
                           GAsyncResult *res,
                           gpointer      user_data)
{
  EvdResolver *resolver;
  Fixture *f = (Fixture *) user_data;
  GError *error = NULL;
  GList *addresses;

  g_assert (EVD_IS_RESOLVER (obj));
  resolver = EVD_RESOLVER (obj);
  g_assert (resolver == f->resolver);

  addresses = evd_resolver_resolve_finish (resolver, res, &error);

  g_assert (addresses == NULL);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);

  g_error_free (error);

  g_main_loop_quit (f->main_loop);
}
*/

static void
resolve_cancel (Fixture       *f,
                gconstpointer  test_data)
{
  /* @TODO: completes this test once EvdResolver supports cancelling */
}

/* error */

static void
resolve_error_on_resolve (GObject      *obj,
                          GAsyncResult *res,
                          gpointer      user_data)
{
  EvdResolver *resolver;
  Fixture *f = (Fixture *) user_data;
  GList *addresses;
  GError *error = NULL;

  g_assert (EVD_IS_RESOLVER (obj));
  resolver = EVD_RESOLVER (obj);
  g_assert (resolver == f->resolver);

  addresses = evd_resolver_resolve_finish (resolver, res, &error);

  g_assert (addresses == NULL);
  g_assert_error (error, G_RESOLVER_ERROR, G_RESOLVER_ERROR_NOT_FOUND);

  g_error_free (error);

  g_main_loop_quit (f->main_loop);
}

static void
resolve_error (Fixture       *f,
               gconstpointer  test_data)
{
  evd_resolver_resolve (f->resolver,
                        NONEXISTANT_1,
                        NULL,
                        resolve_error_on_resolve,
                        f);

  g_main_loop_run (f->main_loop);

  evd_resolver_resolve (f->resolver,
                        NONEXISTANT_2,
                        NULL,
                        resolve_error_on_resolve,
                        f);

  g_main_loop_run (f->main_loop);
}

gint
main (gint argc, gchar **argv)
{
#ifndef GLIB_VERSION_2_36
  g_type_init ();
#endif

  g_test_init (&argc, &argv, NULL);

  g_test_add ("/evd/resolver/get-default",
              Fixture,
              NULL,
              fixture_setup,
              get_default,
              fixture_teardown);

#ifdef HAVE_GIO_UNIX
  g_test_add ("/evd/resolver/unix",
              Fixture,
              NULL,
              fixture_setup,
              unix_addr,
              fixture_teardown);
#endif

  g_test_add ("/evd/resolver/ipv4",
              Fixture,
              NULL,
              fixture_setup,
              ipv4,
              fixture_teardown);

  g_test_add ("/evd/resolver/ipv6",
              Fixture,
              NULL,
              fixture_setup,
              ipv6,
              fixture_teardown);

  g_test_add ("/evd/resolver/resolve",
              Fixture,
              NULL,
              fixture_setup,
              resolve_good_localhost,
              fixture_teardown);

  g_test_add ("/evd/resolver/cancel",
              Fixture,
              NULL,
              fixture_setup,
              resolve_cancel,
              fixture_teardown);

  if (g_test_slow ())
    g_test_add ("/evd/resolver/error",
                Fixture,
                NULL,
                fixture_setup,
                resolve_error,
                fixture_teardown);

  return g_test_run ();
}
