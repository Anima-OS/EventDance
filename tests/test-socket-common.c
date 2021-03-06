/*
 * test-socket-common.c
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
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#ifndef __TEST_SOCKET_COMMON_C__
#define __TEST_SOCKET_COMMON_C__

#include <string.h>
#include <gio/gio.h>

#include <evd.h>
#include <evd-socket-manager.h>

#include <evd-socket-protected.h>

#define EVD_SOCKET_TEST_UNREAD_TEXT "Once upon a time "
#define EVD_SOCKET_TEST_TEXT1       "in a very remote land... "
#define EVD_SOCKET_TEST_TEXT2       "and they lived in joy forever."

typedef struct
{
  GMainLoop *main_loop;
  EvdSocket *socket;
  EvdSocket *socket1;
  EvdSocket *socket2;
  GSocketAddress *socket_addr;

  gint break_src_id;

  gboolean bind;
  gboolean listen;
  gboolean connect;
  gboolean new_conn;

  gsize total_read;
  gint total_closed;
  gboolean completed;
} EvdSocketFixture;

static void
evd_socket_fixture_setup (EvdSocketFixture *fixture,
                          gconstpointer     test_data)
{
  fixture->main_loop = g_main_loop_new (NULL, FALSE);
  fixture->break_src_id = 0;

  fixture->socket = evd_socket_new ();
  fixture->socket1 = evd_socket_new ();
  fixture->socket2 = NULL;
  fixture->socket_addr = NULL;

  fixture->bind = FALSE;
  fixture->listen = FALSE;
  fixture->connect = FALSE;
  fixture->new_conn = FALSE;

  fixture->total_read = 0;
  fixture->total_closed = 0;
  fixture->completed = FALSE;

  g_assert (evd_socket_manager_get () == NULL);
}

static gboolean
evd_socket_test_break (gpointer user_data)
{
  EvdSocketFixture *f = (EvdSocketFixture *) user_data;

  if (f->main_loop != NULL)
    {
      if (f->break_src_id != 0)
        g_source_remove (f->break_src_id);

      g_main_context_wakeup (g_main_loop_get_context (f->main_loop));
      g_main_loop_quit (f->main_loop);
      g_main_loop_unref (f->main_loop);
      f->main_loop = NULL;
    }

  return FALSE;
}

static void
evd_socket_fixture_teardown (EvdSocketFixture *fixture,
                             gconstpointer     test_data)
{
  evd_socket_test_break ((gpointer) fixture);

  g_object_unref (fixture->socket);
  g_object_unref (fixture->socket1);
  if (fixture->socket2 != NULL)
    g_object_unref (fixture->socket2);

  if (fixture->socket_addr != NULL)
    g_object_unref (fixture->socket_addr);

  g_assert (evd_socket_manager_get () == NULL);
}

static void
evd_socket_test_config (EvdSocket      *socket,
                        GSocketFamily   family,
                        GSocketType     type,
                        GSocketProtocol protocol)
{
  GSocketFamily _family;
  GSocketType _type;
  GSocketProtocol _protocol;

  g_object_get (socket,
                "family", &_family,
                "protocol", &_protocol,
                "type", &_type,
                NULL);
  g_assert (family == _family);
  g_assert (type == _type);
  g_assert (protocol == _protocol);
}

static void
evd_socket_test_on_error (EvdSocket *self,
                          gint       code,
                          gchar     *message,
                          gpointer   user_data)
{
  g_error ("%s", message);
  g_assert_not_reached ();
}

static void
evd_socket_test_on_close (EvdSocket *self, gpointer user_data)
{
  EvdSocketFixture *f = (EvdSocketFixture *) user_data;

  f->total_closed ++;

  /* if all socket closed, finish mainloop */
  if (f->total_closed == 2)
    {
      f->completed = TRUE;
      evd_socket_test_break ((gpointer) f);
    }
}

static void
evd_socket_test_on_read (EvdSocket *self, gpointer user_data)
{
  EvdSocketFixture *f = (EvdSocketFixture *) user_data;
  GError *error = NULL;
  gchar buf[1024] = { 0, };
  gssize size;
  gchar *expected_st;
  gssize expected_size;
  GInputStream *input_stream;

  g_assert (evd_socket_can_read (self));

  input_stream = evd_socket_get_input_stream (self);

  size = g_input_stream_read (input_stream, buf, 1023, NULL, &error);
  g_assert_no_error (error);

  if (size == 0)
    return;

  /* validate text read */
  expected_size = strlen (EVD_SOCKET_TEST_UNREAD_TEXT) +
    strlen (EVD_SOCKET_TEST_TEXT1) +
    strlen (EVD_SOCKET_TEST_TEXT2);

  g_assert_cmpint (size, ==, expected_size);

  expected_st = g_strconcat (EVD_SOCKET_TEST_UNREAD_TEXT,
                             EVD_SOCKET_TEST_TEXT1,
                             EVD_SOCKET_TEST_TEXT2,
                             NULL);
  g_assert_cmpstr (buf, ==, expected_st);
  g_free (expected_st);

  /* close socket if finished reading */
  f->total_read += size;
  if (f->total_read ==
      (strlen (EVD_SOCKET_TEST_UNREAD_TEXT) +
       strlen (EVD_SOCKET_TEST_TEXT1) +
       strlen (EVD_SOCKET_TEST_TEXT2)) * 2)
    {
      g_assert (evd_socket_close (self, &error));
      g_assert_no_error (error);
    }
}

static void
evd_socket_test_on_write (EvdSocket *self, gpointer user_data)
{
  GError *error = NULL;
  gssize size;

  g_assert (evd_socket_can_write (self));

  evd_socket_unread (self, EVD_SOCKET_TEST_UNREAD_TEXT, strlen (EVD_SOCKET_TEST_UNREAD_TEXT), &error);
  g_assert_no_error (error);

  /* @TODO: 'evd_socket_can_read' should check if the EvdBufferedInputStream
     has unread data. Bypassing by now. */
  /*  g_assert (evd_socket_can_read (self) == TRUE); */

  size = evd_socket_write (self,
                           EVD_SOCKET_TEST_TEXT1,
                           strlen (EVD_SOCKET_TEST_TEXT1),
                           &error);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, strlen (EVD_SOCKET_TEST_TEXT1));

  size = evd_socket_write (self,
                           EVD_SOCKET_TEST_TEXT2,
                           strlen (EVD_SOCKET_TEST_TEXT2),
                           &error);
  g_assert_no_error (error);
  g_assert_cmpint (size, ==, strlen (EVD_SOCKET_TEST_TEXT2));
}

static void
evd_socket_test_on_new_conn (EvdSocket *self,
                             EvdSocket *client,
                             gpointer   user_data)
{
  EvdSocketFixture *f = (EvdSocketFixture *) user_data;

  f->new_conn = TRUE;

  g_assert (EVD_IS_SOCKET (self));
  g_assert (EVD_IS_SOCKET (client));
  g_assert_cmpint (evd_socket_get_status (client), ==, EVD_SOCKET_STATE_CONNECTED);
  g_assert (evd_socket_get_socket (client) != NULL);

  evd_socket_base_set_read_handler (EVD_SOCKET_BASE (client),
                               G_CALLBACK (evd_socket_test_on_read),
                               f);
  g_assert (evd_socket_base_get_on_read (EVD_SOCKET_BASE (client)) != NULL);

  evd_socket_base_set_write_handler (EVD_SOCKET_BASE (client),
                                G_CALLBACK (evd_socket_test_on_write),
                                f);
  g_assert (evd_socket_base_get_on_write (EVD_SOCKET_BASE (client)) != NULL);

  f->socket2 = client;

  g_signal_connect (f->socket2,
                    "error",
                    G_CALLBACK (evd_socket_test_on_error),
                    (gpointer) f);

  g_signal_connect (f->socket2,
                    "close",
                    G_CALLBACK (evd_socket_test_on_close),
                    (gpointer) f);

  g_object_ref (f->socket2);
}

static void
evd_socket_test_on_state_changed (EvdSocket      *self,
                                  EvdSocketState  new_state,
                                  EvdSocketState  old_state,
                                  gpointer        user_data)
{
  EvdSocketFixture *f = (EvdSocketFixture *) user_data;
  GSocketAddress *address;
  GError *error = NULL;

  switch (new_state)
    {
    case EVD_SOCKET_STATE_BOUND:
      {
        f->bind = TRUE;

        address = evd_socket_get_local_address (self, &error);
        g_assert_no_error (error);

        g_assert (EVD_IS_SOCKET (f->socket));
        g_assert_cmpint (evd_socket_get_status (self), ==, EVD_SOCKET_STATE_BOUND);
        g_assert (evd_socket_get_socket (self) != NULL);

        g_assert (G_IS_SOCKET_ADDRESS (address));

        evd_socket_test_config (self,
                                g_socket_address_get_family (
                                            G_SOCKET_ADDRESS (f->socket_addr)),
                                G_SOCKET_TYPE_STREAM,
                                G_SOCKET_PROTOCOL_DEFAULT);
        break;
      }

    case EVD_SOCKET_STATE_LISTENING:
      {
        f->listen = TRUE;

        g_assert (EVD_IS_SOCKET (self));
        g_assert_cmpint (evd_socket_get_status (self),
                         ==,
                         EVD_SOCKET_STATE_LISTENING);
        g_assert (evd_socket_get_socket (self) != NULL);

        break;
      }

    case EVD_SOCKET_STATE_CONNECTED:
      {
        f->connect = TRUE;

        g_assert (EVD_IS_SOCKET (self));
        g_assert_cmpint (evd_socket_get_status (self),
                         ==,
                         EVD_SOCKET_STATE_CONNECTED);
        g_assert (evd_socket_get_socket (self) != NULL);

        break;
      }

    default:
      break;
    }
}

static gboolean
evd_socket_launch_test (gpointer user_data)
{
  EvdSocketFixture *f = (EvdSocketFixture *) user_data;
  GError *error = NULL;

  g_signal_connect (f->socket,
                    "error",
                    G_CALLBACK (evd_socket_test_on_error),
                    (gpointer) f);
  g_signal_connect (f->socket1,
                    "error",
                    G_CALLBACK (evd_socket_test_on_error),
                    (gpointer) f);

  g_signal_connect (f->socket,
                    "close",
                    G_CALLBACK (evd_socket_test_on_close),
                    (gpointer) f);
  g_signal_connect (f->socket1,
                    "close",
                    G_CALLBACK (evd_socket_test_on_close),
                    (gpointer) f);

  evd_socket_base_set_read_handler (EVD_SOCKET_BASE (f->socket1),
                               G_CALLBACK (evd_socket_test_on_read),
                               f);
  g_assert (evd_socket_base_get_on_read (EVD_SOCKET_BASE (f->socket1)) != NULL);

  evd_socket_base_set_write_handler (EVD_SOCKET_BASE (f->socket1),
                                G_CALLBACK (evd_socket_test_on_write),
                                f);
  g_assert (evd_socket_base_get_on_write (EVD_SOCKET_BASE (f->socket1)) != NULL);

  g_signal_connect (f->socket,
                    "state-changed",
                    G_CALLBACK (evd_socket_test_on_state_changed),
                    (gpointer) f);
  g_signal_connect (f->socket1,
                    "state-changed",
                    G_CALLBACK (evd_socket_test_on_state_changed),
                    (gpointer) f);

  evd_socket_bind_addr (f->socket, f->socket_addr, TRUE, &error);
  g_assert_no_error (error);

  evd_socket_listen_addr (f->socket, NULL, &error);
  g_assert_no_error (error);

  /* connect */
  g_signal_connect (f->socket,
                    "new-connection",
                    G_CALLBACK (evd_socket_test_on_new_conn),
                    (gpointer) f);

  evd_socket_connect_addr (f->socket1, f->socket_addr, &error);
  g_assert_no_error (error);
  g_assert_cmpint (evd_socket_get_status (f->socket1),
                   ==,
                   EVD_SOCKET_STATE_CONNECTING);

  return FALSE;
}

static void
evd_socket_test (EvdSocketFixture *f,
                 gconstpointer     test_data)
{
  f->break_src_id = g_timeout_add (1000,
                                   (GSourceFunc) evd_socket_test_break,
                                   (gpointer) f);

  g_idle_add ((GSourceFunc) evd_socket_launch_test,
              (gpointer) f);

  g_main_loop_run (f->main_loop);

  g_assert (f->bind);
  g_assert (f->listen);
  g_assert (f->connect);
  g_assert (f->new_conn);

  g_assert (f->completed);
}

#endif /* __TEST_SOCKET_COMMON_C__ */
