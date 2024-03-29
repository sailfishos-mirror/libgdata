/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Philip Withnall 2010, 2015 <philip@tecnocode.co.uk>
 *
 * GData Client is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * GData Client is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GData Client.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <glib.h>
#include <locale.h>
#include <string.h>
#include <sys/types.h>
#ifndef G_OS_WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "gdata.h"
#include "common.h"

#ifdef HAVE_LIBSOUP_2_55_90
static gpointer
run_server_thread (GMainLoop *loop)
{
	g_main_context_push_thread_default (g_main_loop_get_context (loop));
	g_main_loop_run (loop);
	g_main_context_pop_thread_default (g_main_loop_get_context (loop));

	return NULL;
}
#else /* if !HAVE_LIBSOUP_2_55_90 */
static gpointer
run_server_thread (SoupServer *server)
{
	soup_server_run (server);

	return NULL;
}
#endif /* !HAVE_LIBSOUP_2_55_90 */

static GThread *
run_server (SoupServer *server, GMainLoop *loop)
{
	GThread *thread;
	gchar *port_string;
	GError *error = NULL;
	guint16 port;

#ifdef HAVE_LIBSOUP_2_55_90
	thread = g_thread_try_new ("server-thread", (GThreadFunc) run_server_thread, loop, &error);
#else /* if !HAVE_LIBSOUP_2_55_90 */
	thread = g_thread_try_new ("server-thread", (GThreadFunc) run_server_thread, server, &error);
#endif /* !HAVE_LIBSOUP_2_55_90 */
	g_assert_no_error (error);
	g_assert (thread != NULL);

	/* Set the port so that libgdata doesn't override it. */
#ifdef HAVE_LIBSOUP_2_55_90
{
	GSList *uris;  /* owned */

	uris = soup_server_get_uris (server);
	g_assert (uris != NULL);
	port = soup_uri_get_port (uris->data);

	g_slist_free_full (uris, (GDestroyNotify) soup_uri_free);
}
#else /* if !HAVE_LIBSOUP_2_55_90 */
	port = soup_server_get_port (server);
#endif /* !HAVE_LIBSOUP_2_55_90 */

	port_string = g_strdup_printf ("%u", port);
	g_setenv ("LIBGDATA_HTTPS_PORT", port_string, TRUE);
	g_free (port_string);

	return thread;
}

#ifdef HAVE_LIBSOUP_2_55_90
static gboolean
quit_server_cb (GMainLoop *loop)
{
	g_main_loop_quit (loop);

	return FALSE;
}

static void
stop_server (SoupServer *server, GMainLoop *loop)
{
	soup_add_completion (g_main_loop_get_context (loop),
	                     (GSourceFunc) quit_server_cb, loop);
}
#else /* if !HAVE_LIBSOUP_2_55_90 */
static gboolean
quit_server_cb (SoupServer *server)
{
	soup_server_quit (server);

	return FALSE;
}

static void
stop_server (SoupServer *server, GMainLoop *loop)
{
	soup_add_completion (g_main_loop_get_context (loop),
	                     (GSourceFunc) quit_server_cb, server);
}
#endif /* !HAVE_LIBSOUP_2_55_90 */

static gchar *
get_test_string (guint start_num, guint end_num)
{
	GString *test_string;
	guint i;

	g_return_val_if_fail (end_num < G_MAXUINT, NULL);

	test_string = g_string_new (NULL);

	for (i = start_num; i < end_num + 1; i++)
		g_string_append_printf (test_string, "%u\n", i);

	return g_string_free (test_string, FALSE);
}

static void
test_download_stream_download_server_content_length_handler_cb (SoupServer *server, SoupMessage *message, const char *path, GHashTable *query,
                                                                SoupClientContext *client, gpointer user_data)
{
	gchar *test_string;
	goffset test_string_length;

	test_string = get_test_string (1, 1000);
	test_string_length = strlen (test_string) + 1;

	/* Add some response headers */
	soup_message_set_status (message, SOUP_STATUS_OK);
	soup_message_headers_set_content_type (message->response_headers, "text/plain", NULL);
	soup_message_headers_set_content_length (message->response_headers, test_string_length);

	/* Set the response body */
	soup_message_body_append (message->response_body, SOUP_MEMORY_TAKE, test_string, test_string_length);
}

static SoupServer *
create_server (SoupServerCallback callback, gpointer user_data, GMainLoop **main_loop)
{
	GMainContext *context;
	SoupServer *server;
#ifdef HAVE_LIBSOUP_2_55_90
	gchar *cert_path = NULL, *key_path = NULL;
	GError *error = NULL;
#else /* if !HAVE_LIBSOUP_2_55_90 */
	union {
		struct sockaddr_in in;
		struct sockaddr norm;
	} sock;
	SoupAddress *addr;
#endif /* HAVE_LIBSOUP_2_55_90 */

	/* Create the server */
	g_assert (main_loop != NULL);
	context = g_main_context_new ();
	*main_loop = g_main_loop_new (context, FALSE);

#ifdef HAVE_LIBSOUP_2_55_90
	server = soup_server_new (NULL, NULL);

	cert_path = g_test_build_filename (G_TEST_DIST, "cert.pem", NULL);
	key_path = g_test_build_filename (G_TEST_DIST, "key.pem", NULL);

	soup_server_set_ssl_cert_file (server, cert_path, key_path, &error);
	g_assert_no_error (error);

	g_free (key_path);
	g_free (cert_path);

	soup_server_add_handler (server, NULL, callback, user_data, NULL);

	g_main_context_push_thread_default (context);

	soup_server_listen_local (server, 0  /* random port */,
	                          SOUP_SERVER_LISTEN_HTTPS, &error);
	g_assert_no_error (error);

	g_main_context_pop_thread_default (context);
#else /* if !HAVE_LIBSOUP_2_55_90 */
	memset (&sock, 0, sizeof (sock));
	sock.in.sin_family = AF_INET;
	sock.in.sin_addr.s_addr = htonl (INADDR_LOOPBACK);
	sock.in.sin_port = htons (0); /* random port */

	addr = soup_address_new_from_sockaddr (&sock.norm, sizeof (sock.norm));
	g_assert (addr != NULL);

	server = soup_server_new (SOUP_SERVER_INTERFACE, addr,
	                          SOUP_SERVER_ASYNC_CONTEXT, context,
	                          NULL);

	soup_server_add_handler (server, NULL, callback, user_data, NULL);

	g_object_unref (addr);
#endif /* !HAVE_LIBSOUP_2_55_90 */

	g_assert (server != NULL);
	g_main_context_unref (context);

	return server;
}

static gchar *
build_server_uri (SoupServer *server)
{
#ifdef HAVE_LIBSOUP_2_55_90
	GSList *uris;  /* owned */
	GSList *l;  /* unowned */
	gchar *retval = NULL;  /* owned */

	uris = soup_server_get_uris (server);

	for (l = uris; l != NULL && retval == NULL; l = l->next) {
		if (soup_uri_get_scheme (l->data) == SOUP_URI_SCHEME_HTTPS) {
			retval = soup_uri_to_string (l->data, FALSE);
		}
	}

	g_slist_free_full (uris, (GDestroyNotify) soup_uri_free);

	g_assert (retval != NULL);

	return retval;
#else /* if !HAVE_LIBSOUP_2_55_90 */
	return g_strdup_printf ("https://%s:%u/",
	                        soup_address_get_physical (soup_socket_get_local_address (soup_server_get_listener (server))),
	                        soup_server_get_port (server));
#endif /* !HAVE_LIBSOUP_2_55_90 */
}

static void
test_download_stream_download_content_length (void)
{
	SoupServer *server;
	GMainLoop *main_loop;
	GThread *thread;
	gchar *download_uri, *test_string;
	GDataService *service;
	GInputStream *download_stream;
	gssize length_read;
	GString *contents;
	guint8 buffer[20];
	gboolean success;
	GError *error = NULL;

	/* Create and run the server */
	server = create_server ((SoupServerCallback) test_download_stream_download_server_content_length_handler_cb, NULL, &main_loop);
	thread = run_server (server, main_loop);

	/* Create a new download stream connected to the server */
	download_uri = build_server_uri (server);
	service = GDATA_SERVICE (gdata_youtube_service_new ("developer-key", NULL));
	download_stream = gdata_download_stream_new (service, NULL, download_uri, NULL);
	g_object_unref (service);
	g_free (download_uri);

	/* Read the entire stream into a string which we can later compare with what we expect. */
	contents = g_string_new (NULL);

	while ((length_read = g_input_stream_read (download_stream, buffer, sizeof (buffer), NULL, &error)) > 0) {
		g_assert_cmpint (length_read, <=, sizeof (buffer));

		g_string_append_len (contents, (const gchar*) buffer, length_read);
	}

	/* Check we've reached EOF successfully */
	g_assert_no_error (error);
	g_assert_cmpint (length_read, ==, 0);

	/* Close the stream */
	success = g_input_stream_close (download_stream, NULL, &error);
	g_assert_no_error (error);
	g_assert (success == TRUE);

	/* Compare the downloaded string to the original */
	test_string = get_test_string (1, 1000);

	g_assert_cmpint (contents->len, ==, strlen (test_string) + 1);
	g_assert_cmpstr (contents->str, ==, test_string);

	g_free (test_string);
	g_string_free (contents, TRUE);

	/* Kill the server and wait for it to die */
	stop_server (server, main_loop);
	g_thread_join (thread);

	g_object_unref (download_stream);
	g_object_unref (server);
	g_main_loop_unref (main_loop);
}

static void
test_download_stream_download_server_seek_handler_cb (SoupServer *server, SoupMessage *message, const char *path, GHashTable *query,
                                                      SoupClientContext *client, gpointer user_data)
{
	gchar *test_string;
	goffset test_string_length;

	test_string = get_test_string (1, 1000);
	test_string_length = strlen (test_string) + 1;

	/* Add some response headers */
	soup_message_set_status (message, SOUP_STATUS_OK);
	soup_message_body_append (message->response_body, SOUP_MEMORY_TAKE, test_string, test_string_length);
}

/* Test seeking before the first read */
static void
test_download_stream_download_seek_before_start (void)
{
	SoupServer *server;
	GMainLoop *main_loop;
	GThread *thread;
	gchar *download_uri, *test_string;
	goffset test_string_offset = 0;
	guint test_string_length;
	GDataService *service;
	GInputStream *download_stream;
	gssize length_read;
	guint8 buffer[20];
	gboolean success;
	GError *error = NULL;

	/* Create and run the server */
	server = create_server ((SoupServerCallback) test_download_stream_download_server_seek_handler_cb, NULL, &main_loop);
	thread = run_server (server, main_loop);

	/* Create a new download stream connected to the server */
	download_uri = build_server_uri (server);
	service = GDATA_SERVICE (gdata_youtube_service_new ("developer-key", NULL));
	download_stream = gdata_download_stream_new (service, NULL, download_uri, NULL);
	g_object_unref (service);
	g_free (download_uri);

	/* Read alternating blocks into a string and compare with what we expect as we go. i.e. Skip 20 bytes, then read 20 bytes, etc. */
	test_string = get_test_string (1, 1000);
	test_string_length = strlen (test_string) + 1;

	while (TRUE) {
		/* Check the seek offset */
		g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), ==, test_string_offset);

		/* Seek forward a buffer length */
		if (g_seekable_seek (G_SEEKABLE (download_stream), sizeof (buffer), G_SEEK_CUR, NULL, &error) == FALSE) {
			/* Tried to seek past the end of the stream */
			g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
			g_clear_error (&error);
			break;
		}

		test_string_offset += sizeof (buffer);
		g_assert_no_error (error);

		/* Check the seek offset again */
		g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), ==, test_string_offset);

		/* Read a buffer-load */
		length_read = g_input_stream_read (download_stream, buffer, sizeof (buffer), NULL, &error);

		g_assert_no_error (error);
		g_assert_cmpint (length_read, <=, sizeof (buffer));

		/* Check the buffer-load against the test string */
		g_assert (memcmp (buffer, test_string + test_string_offset, length_read) == 0);
		test_string_offset += length_read;

		/* Check the seek offset again */
		g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), ==, test_string_offset);

		if (length_read < (gssize) sizeof (buffer)) {
			/* Done */
			break;
		}
	}

	g_free (test_string);

	/* Check the seek offset is within one buffer-load of the end */
	g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), >, test_string_length - sizeof (buffer));
	g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), <=, test_string_length);

	/* Close the stream */
	success = g_input_stream_close (download_stream, NULL, &error);
	g_assert_no_error (error);
	g_assert (success == TRUE);

	/* Kill the server and wait for it to die */
	stop_server (server, main_loop);
	g_thread_join (thread);

	g_object_unref (download_stream);
	g_object_unref (server);
	g_main_loop_unref (main_loop);
}

/* Test seeking forwards after the first read */
static void
test_download_stream_download_seek_after_start_forwards (void)
{
	SoupServer *server;
	GMainLoop *main_loop;
	GThread *thread;
	gchar *download_uri, *test_string;
	goffset test_string_offset = 0;
	guint test_string_length;
	GDataService *service;
	GInputStream *download_stream;
	gssize length_read;
	guint8 buffer[20];
	gboolean success;
	GError *error = NULL;

	/* Create and run the server */
	server = create_server ((SoupServerCallback) test_download_stream_download_server_seek_handler_cb, NULL, &main_loop);
	thread = run_server (server, main_loop);

	/* Create a new download stream connected to the server */
	download_uri = build_server_uri (server);
	service = GDATA_SERVICE (gdata_youtube_service_new ("developer-key", NULL));
	download_stream = gdata_download_stream_new (service, NULL, download_uri, NULL);
	g_object_unref (service);
	g_free (download_uri);

	/* Read alternating blocks into a string and compare with what we expect as we go. i.e. Read 20 bytes, then skip 20 bytes, etc. */
	test_string = get_test_string (1, 1000);
	test_string_length = strlen (test_string) + 1;

	while (TRUE) {
		/* Check the seek offset */
		g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), ==, test_string_offset);

		/* Read a buffer-load */
		length_read = g_input_stream_read (download_stream, buffer, sizeof (buffer), NULL, &error);

		g_assert_no_error (error);
		g_assert_cmpint (length_read, <=, sizeof (buffer));

		/* Check the buffer-load against the test string */
		g_assert (memcmp (buffer, test_string + test_string_offset, length_read) == 0);
		test_string_offset += length_read;

		/* Check the seek offset again */
		g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), ==, test_string_offset);

		if (length_read < (gssize) sizeof (buffer)) {
			/* Done */
			break;
		}

		/* Seek forward a buffer length */
		if (g_seekable_seek (G_SEEKABLE (download_stream), sizeof (buffer), G_SEEK_CUR, NULL, &error) == FALSE) {
			/* Tried to seek past the end of the stream */
			g_assert_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_ARGUMENT);
			g_clear_error (&error);
			break;
		}

		test_string_offset += sizeof (buffer);
		g_assert_no_error (error);

		/* Check the seek offset again */
		g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), ==, test_string_offset);
	}

	g_free (test_string);

	/* Check the seek offset is within one buffer-load of the end */
	g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), >, test_string_length - sizeof (buffer));
	g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), <=, test_string_length);

	/* Close the stream */
	success = g_input_stream_close (download_stream, NULL, &error);
	g_assert_no_error (error);
	g_assert (success == TRUE);

	/* Kill the server and wait for it to die */
	stop_server (server, main_loop);
	g_thread_join (thread);

	g_object_unref (download_stream);
	g_object_unref (server);
	g_main_loop_unref (main_loop);
}

/* Test seeking backwards after the first read */
static void
test_download_stream_download_seek_after_start_backwards (void)
{
	SoupServer *server;
	GMainLoop *main_loop;
	GThread *thread;
	gchar *download_uri, *test_string;
	goffset test_string_offset = 0;
	guint repeat_count;
	GDataService *service;
	GInputStream *download_stream;
	gssize length_read;
	guint8 buffer[20];
	gboolean success;
	GError *error = NULL;

	/* Create and run the server */
	server = create_server ((SoupServerCallback) test_download_stream_download_server_seek_handler_cb, NULL, &main_loop);
	thread = run_server (server, main_loop);

	/* Create a new download stream connected to the server */
	download_uri = build_server_uri (server);
	service = GDATA_SERVICE (gdata_youtube_service_new ("developer-key", NULL));
	download_stream = gdata_download_stream_new (service, NULL, download_uri, NULL);
	g_object_unref (service);
	g_free (download_uri);

	/* Read a block in, then skip back over the block again. i.e. Read the first block, read the second block, skip back over the second block,
	 * read the second block again, skip back over it, etc. Close the stream after doing this several times. */
	test_string = get_test_string (1, 1000);

	/* Read a buffer-load to begin with */
	length_read = g_input_stream_read (download_stream, buffer, sizeof (buffer), NULL, &error);

	g_assert_no_error (error);
	test_string_offset += length_read;

	for (repeat_count = 6; repeat_count > 0; repeat_count--) {
		/* Check the seek offset */
		g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), ==, test_string_offset);

		/* Read a buffer-load */
		length_read = g_input_stream_read (download_stream, buffer, sizeof (buffer), NULL, &error);

		g_assert_no_error (error);
		g_assert_cmpint (length_read, <=, sizeof (buffer));

		/* Check the buffer-load against the test string */
		g_assert (memcmp (buffer, test_string + test_string_offset, length_read) == 0);
		test_string_offset += length_read;

		/* Check the seek offset again */
		g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), ==, test_string_offset);

		/* Seek backwards a buffer length */
		success = g_seekable_seek (G_SEEKABLE (download_stream), -length_read, G_SEEK_CUR, NULL, &error);
		g_assert_no_error (error);
		g_assert (success == TRUE);
		test_string_offset -= length_read;

		/* Check the seek offset again */
		g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), ==, test_string_offset);
	}

	g_free (test_string);

	/* Check the seek offset is at the end of the first buffer-load */
	g_assert_cmpint (g_seekable_tell (G_SEEKABLE (download_stream)), ==, sizeof (buffer));

	/* Close the stream */
	success = g_input_stream_close (download_stream, NULL, &error);
	g_assert_no_error (error);
	g_assert (success == TRUE);

	/* Kill the server and wait for it to die */
	stop_server (server, main_loop);
	g_thread_join (thread);

	g_object_unref (download_stream);
	g_object_unref (server);
	g_main_loop_unref (main_loop);
}

static void
test_upload_stream_upload_no_entry_content_length_server_handler_cb (SoupServer *server, SoupMessage *message, const char *path, GHashTable *query,
                                                                     SoupClientContext *client, gpointer user_data)
{
	gchar *test_string;
	goffset test_string_length;

	/* Check the Slug and Content-Type headers have been correctly set by the client */
	g_assert_cmpstr (soup_message_headers_get_content_type (message->request_headers, NULL), ==, "text/plain");
	g_assert_cmpstr (soup_message_headers_get_one (message->request_headers, "Slug"), ==, "slug");

	/* Check the client sent the right data */
	test_string = get_test_string (1, 1000);
	test_string_length = strlen (test_string) + 1;

	g_assert_cmpint (message->request_body->length, ==, test_string_length);
	g_assert_cmpstr (message->request_body->data, ==, test_string);

	g_free (test_string);

	/* Add some response headers */
	soup_message_set_status (message, SOUP_STATUS_OK);
	soup_message_headers_set_content_type (message->response_headers, "text/plain", NULL);

	/* Set the response body */
	soup_message_body_append (message->response_body, SOUP_MEMORY_STATIC, "Test passed!", 13);
}

static void
test_upload_stream_upload_no_entry_content_length (void)
{
	SoupServer *server;
	GMainLoop *main_loop;
	GThread *thread;
	gchar *upload_uri, *test_string;
	GDataService *service;
	GOutputStream *upload_stream;
	gssize length_written;
	gsize total_length_written = 0;
	goffset test_string_length;
	gboolean success;
	GError *error = NULL;

	/* Create and run the server */
	server = create_server ((SoupServerCallback) test_upload_stream_upload_no_entry_content_length_server_handler_cb, NULL, &main_loop);
	thread = run_server (server, main_loop);

	/* Create a new upload stream uploading to the server */
	upload_uri = build_server_uri (server);
	service = GDATA_SERVICE (gdata_youtube_service_new ("developer-key", NULL));
	upload_stream = gdata_upload_stream_new (service, NULL, SOUP_METHOD_POST, upload_uri, NULL, "slug", "text/plain", NULL);
	g_object_unref (service);
	g_free (upload_uri);

	/* Write the entire test string to the stream */
	test_string = get_test_string (1, 1000);
	test_string_length = strlen (test_string) + 1;

	while ((length_written = g_output_stream_write (upload_stream, test_string + total_length_written,
	                                                test_string_length - total_length_written, NULL, &error)) > 0) {
		g_assert_cmpint (length_written, <=, test_string_length - total_length_written);

		total_length_written += length_written;
	}

	g_free (test_string);

	/* Check we've had a successful return value */
	g_assert_no_error (error);
	g_assert_cmpint (length_written, ==, 0);
	g_assert_cmpint (total_length_written, ==, test_string_length);

	/* Close the stream */
	success = g_output_stream_close (upload_stream, NULL, &error);
	g_assert_no_error (error);
	g_assert (success == TRUE);

	/* Kill the server and wait for it to die */
	stop_server (server, main_loop);
	g_thread_join (thread);

	g_object_unref (upload_stream);
	g_object_unref (server);
	g_main_loop_unref (main_loop);
}

/* Test parameters for a run of test_upload_stream_resumable(). */
typedef struct {
	enum {
		CONTENT_ONLY = 0,
		CONTENT_AND_METADATA = 1,
		METADATA_ONLY = 2,
	} content_type;
#define UPLOAD_STREAM_RESUMABLE_MAX_CONTENT_TYPE 2
	gsize file_size;
	enum {
		ERROR_ON_INITIAL_REQUEST = 0,
		ERROR_ON_SUBSEQUENT_REQUEST = 1,
		ERROR_ON_FINAL_REQUEST = 2,
		NO_ERROR = 3,
	} error_type;
#define UPLOAD_STREAM_RESUMABLE_MAX_ERROR_TYPE 3
} UploadStreamResumableTestParams;

static const gchar *upload_stream_resumable_content_type_names[] = {
	"content-only",
	"content-and-metadata",
	"metadata-only",
};

static const gchar *upload_stream_resumable_error_type_names[] = {
	"initial-error",
	"subsequent-error",
	"final-error",
	"success",
};

typedef struct {
	UploadStreamResumableTestParams *test_params;
	gsize next_range_start;
	gsize next_range_end;
	guint next_path_index;
	const gchar *test_string;
} UploadStreamResumableServerData;

static void
test_upload_stream_resumable_server_handler_cb (SoupServer *server, SoupMessage *message, const char *path, GHashTable *query,
                                                SoupClientContext *client, UploadStreamResumableServerData *server_data)
{
	UploadStreamResumableTestParams *test_params;

	test_params = server_data->test_params;

	/* Are we handling the initial request, or a subsequent one? */
	if (strcmp (path, "/") == 0) {
		/* Initial request. */
		gchar *file_size_str;

		/* Check the Slug and X-Upload-* headers. */
		g_assert_cmpstr (soup_message_headers_get_one (message->request_headers, "Slug"), ==, "slug");

		file_size_str = g_strdup_printf ("%" G_GSIZE_FORMAT, test_params->file_size);
		g_assert_cmpstr (soup_message_headers_get_one (message->request_headers, "X-Upload-Content-Type"), ==, "text/plain");
		g_assert_cmpstr (soup_message_headers_get_one (message->request_headers, "X-Upload-Content-Length"), ==, file_size_str);
		g_free (file_size_str);

		/* Check the Content-Type and content. */
		switch (test_params->content_type) {
			case CONTENT_ONLY:
				/* Check nothing was sent. */
				g_assert_cmpstr (soup_message_headers_get_content_type (message->request_headers, NULL), ==, NULL);
				g_assert_cmpint (message->request_body->length, ==, 0);

				break;
			case CONTENT_AND_METADATA:
			case METADATA_ONLY:
				/* Check the JSON sent by the client. */
				g_assert_cmpstr (soup_message_headers_get_content_type (message->request_headers, NULL), ==, "application/json");

				g_assert (message->request_body->data[message->request_body->length] == '\0');
				g_assert (gdata_test_compare_json_strings (message->request_body->data,
					"{"
						"'title': 'Test title!',"
						"'kind': 'youtube#video',"
						"'snippet': {"
							"'title': 'Test title!'"
						"},"
						"'status': {"
							"'privacyStatus': 'public'"
						"},"
						"'recordingDetails': {}"
					"}", TRUE) == TRUE);

				break;
			default:
				g_assert_not_reached ();
		}

		/* Send a response. */
		switch (test_params->error_type) {
			case ERROR_ON_INITIAL_REQUEST:
				/* Error. */
				goto error;
			case ERROR_ON_SUBSEQUENT_REQUEST:
			case ERROR_ON_FINAL_REQUEST:
			case NO_ERROR:
				/* Success. */
				if (test_params->file_size == 0) {
					goto completion;
				} else {
					goto continuation;
				}

				break;
			default:
				g_assert_not_reached ();
		}
	} else if (*path == '/' && g_ascii_strtoull (path + 1, NULL, 10) == server_data->next_path_index) {
		/* Subsequent request. */

		/* Check the Slug and X-Upload-* headers. */
		g_assert_cmpstr (soup_message_headers_get_one (message->request_headers, "Slug"), ==, NULL);
		g_assert_cmpstr (soup_message_headers_get_one (message->request_headers, "X-Upload-Content-Type"), ==, NULL);
		g_assert_cmpstr (soup_message_headers_get_one (message->request_headers, "X-Upload-Content-Length"), ==, NULL);

		/* Check the Content-Type and content. */
		switch (test_params->content_type) {
			case CONTENT_ONLY:
			case CONTENT_AND_METADATA: {
				goffset range_start, range_end, range_length;

				/* Check the headers. */
				g_assert_cmpstr (soup_message_headers_get_content_type (message->request_headers, NULL), ==, "text/plain");
				g_assert_cmpint (soup_message_headers_get_content_length (message->request_headers), ==, message->request_body->length);
				g_assert_cmpint (message->request_body->length, >, 0);
				g_assert_cmpint (message->request_body->length, <=, 512 * 1024 /* 512 KiB */);
				g_assert (soup_message_headers_get_content_range (message->request_headers, &range_start, &range_end,
				                                                  &range_length) == TRUE);
				g_assert_cmpint (range_start, ==, server_data->next_range_start);
				g_assert_cmpint (range_end, ==, server_data->next_range_end);
				g_assert_cmpint (range_length, ==, test_params->file_size);

				/* Check the content. */
				g_assert (memcmp (server_data->test_string + range_start, message->request_body->data,
				                  message->request_body->length) == 0);

				/* Update the expected values. */
				server_data->next_range_start = range_end + 1;
				server_data->next_range_end = MIN (server_data->next_range_start + 512 * 1024, test_params->file_size) - 1;
				server_data->next_path_index++;

				break;
			}
			case METADATA_ONLY:
			default:
				g_assert_not_reached ();
		}

		/* Send a response. */
		switch (test_params->error_type) {
			case ERROR_ON_SUBSEQUENT_REQUEST:
			case ERROR_ON_FINAL_REQUEST:
				/* Skip the error if this isn't the final request. */
				if (test_params->error_type == ERROR_ON_SUBSEQUENT_REQUEST ||
				    (test_params->error_type == ERROR_ON_FINAL_REQUEST && server_data->next_range_start == test_params->file_size)) {
					goto error;
				}

				/* Fall through */
			case NO_ERROR:
				/* Success. */
				if (server_data->next_range_start == test_params->file_size) {
					goto completion;
				} else {
					goto continuation;
				}

				break;
			case ERROR_ON_INITIAL_REQUEST:
			default:
				g_assert_not_reached ();
		}
	} else {
		g_assert_not_reached ();
	}

	return;

error: {
		const gchar *error_response =
			"{"
				"'error': {"
					"'errors': ["
						"{"
							"'domain': 'global',"
							"'reason': 'authError',"
							"'message': 'Invalid token.',"
							"'location': 'Authorization: GoogleLogin'"
						"}"
					"],"
					"'code': 400,"
					"'message': 'Invalid token.'"
				"}"
			"}";

		/* Error. */
		soup_message_set_status (message, SOUP_STATUS_UNAUTHORIZED); /* arbitrary error status code */
		soup_message_body_append (message->response_body, SOUP_MEMORY_STATIC, error_response, strlen (error_response));
	}

	return;

continuation: {
		gchar *upload_uri, *server_uri;

		/* Continuation. */
		if (server_data->next_path_index == 0) {
			soup_message_set_status (message, SOUP_STATUS_OK);
		} else {
			soup_message_set_status (message, 308);
		}

		server_uri = build_server_uri (server);
		g_assert_cmpstr (server_uri + strlen (server_uri) - 1, ==, "/");
		upload_uri = g_strdup_printf ("%s%u", server_uri,
		                              ++server_data->next_path_index);
		soup_message_headers_replace (message->response_headers, "Location", upload_uri);
		g_free (upload_uri);
		g_free (server_uri);
	}

	return;

completion: {
		const gchar *completion_response =
			"{"
				"'kind': 'youtube#video',"
				"'snippet': {"
					"'title': 'Test title!',"
					"'categoryId': '10'"  /* Music */
				"},"
				"'status': {"
					"'privacyStatus': 'public'"
				"},"
				"'recordingDetails': {"
					"'recordingDate': '2005-10-02'"
				"}"
			"}";

		/* Completion. */
		soup_message_set_status (message, SOUP_STATUS_CREATED);
		soup_message_headers_set_content_type (message->response_headers, "application/json", NULL);
		soup_message_body_append (message->response_body, SOUP_MEMORY_STATIC, completion_response, strlen (completion_response));
	}
}

static void
test_upload_stream_resumable (gconstpointer user_data)
{
	UploadStreamResumableTestParams *test_params;
	UploadStreamResumableServerData server_data;
	SoupServer *server;
	GMainLoop *main_loop;
	GThread *thread;
	gchar *upload_uri;
	GDataService *service;
	GDataEntry *entry = NULL;
	GOutputStream *upload_stream;
	gssize length_written;
	gsize total_length_written = 0;
	gchar *test_string;
	goffset test_string_length;
	gboolean success;
	GError *error = NULL;

	test_params = (UploadStreamResumableTestParams*) user_data;

	/* Build the test string. */
	if (test_params->file_size > 0) {
		test_string = get_test_string (1, test_params->file_size / 4 /* arbitrary number which should generate enough data */);
		g_assert (strlen (test_string) + 1 >= test_params->file_size);
		test_string[test_params->file_size - 1] = '\0'; /* trim the string to the right length */
	} else {
		test_string = NULL;
	}

	test_string_length = test_params->file_size;

	/* Create and run the server */
	server_data.test_params = test_params;
	server_data.next_range_start = 0;
	server_data.next_range_end = MIN (test_params->file_size, 512 * 1024 /* 512 KiB */) - 1;
	server_data.next_path_index = 0;
	server_data.test_string = test_string;

	server = create_server ((SoupServerCallback) test_upload_stream_resumable_server_handler_cb, &server_data, &main_loop);
	thread = run_server (server, main_loop);

	/* Create a new upload stream uploading to the server */
	if (test_params->content_type == CONTENT_AND_METADATA || test_params->content_type == METADATA_ONLY) {
		/* Build a test entry. */
		entry = GDATA_ENTRY (gdata_youtube_video_new (NULL));
		gdata_entry_set_title (entry, "Test title!");
	}

	upload_uri = build_server_uri (server);
	service = GDATA_SERVICE (gdata_youtube_service_new ("developer-key", NULL));
	upload_stream = gdata_upload_stream_new_resumable (service, NULL, SOUP_METHOD_POST, upload_uri, entry, "slug", "text/plain",
	                                                   test_params->file_size, NULL);
	g_object_unref (service);
	g_free (upload_uri);
	g_clear_object (&entry);

	if (test_params->file_size > 0) {
		while ((length_written = g_output_stream_write (upload_stream, test_string + total_length_written,
		                                                test_string_length - total_length_written, NULL, &error)) > 0) {
			g_assert_cmpint (length_written, <=, test_string_length - total_length_written);

			total_length_written += length_written;
		}
	} else {
		/* Do an empty write to poke things into action. */
		if ((length_written = g_output_stream_write (upload_stream, "", 0, NULL, &error)) > 0) {
			total_length_written += length_written;
		}
	}

	/* Check the return value. */
	switch (test_params->error_type) {
		case ERROR_ON_INITIAL_REQUEST:
		case ERROR_ON_SUBSEQUENT_REQUEST:
		case ERROR_ON_FINAL_REQUEST:
			/* We can't check the write() call for errors, since whether it throws an error depends on whether the range it's writing
			 * overlaps a resumable upload chunk, which is entirely arbitrary and unpredictable. */
			g_assert_cmpint (length_written, <=, 0);
			g_assert_cmpint (total_length_written, <=, test_string_length);
			g_clear_error (&error);

			/* Close the stream */
			success = g_output_stream_close (upload_stream, NULL, &error);
			g_assert_error (error, GDATA_SERVICE_ERROR, GDATA_SERVICE_ERROR_AUTHENTICATION_REQUIRED);
			g_assert (success == FALSE);
			g_clear_error (&error);

			break;
		case NO_ERROR:
			/* Check we've had a successful return value */
			g_assert_no_error (error);
			g_assert_cmpint (length_written, ==, 0);
			g_assert_cmpint (total_length_written, ==, test_string_length);

			/* Close the stream */
			success = g_output_stream_close (upload_stream, NULL, &error);
			g_assert_no_error (error);
			g_assert (success == TRUE);

			break;
		default:
			g_assert_not_reached ();
	}

	/* Kill the server and wait for it to die */
	stop_server (server, main_loop);
	g_thread_join (thread);

	g_free (test_string);
	g_object_unref (upload_stream);
	g_object_unref (server);
	g_main_loop_unref (main_loop);
}

int
main (int argc, char *argv[])
{
	gdata_test_init (argc, argv);

	/* Only print out headers, since we're sending a lot of data. */
	g_setenv ("LIBGDATA_DEBUG", "2" /* GDATA_LOG_HEADERS */, TRUE);

	g_test_add_func ("/download-stream/download_content_length", test_download_stream_download_content_length);
	g_test_add_func ("/download-stream/download_seek/before_start", test_download_stream_download_seek_before_start);
	g_test_add_func ("/download-stream/download_seek/after_start_forwards", test_download_stream_download_seek_after_start_forwards);
	g_test_add_func ("/download-stream/download_seek/after_start_backwards", test_download_stream_download_seek_after_start_backwards);

	g_test_add_func ("/upload-stream/upload_no_entry_content_length", test_upload_stream_upload_no_entry_content_length);

	/* Test all possible combinations of conditions for resumable uploads. */
	{
		guint i, j, k;

		const gsize file_sizes[] = { /* all in bytes */
			407 * 1024, /* < 512 KiB */
			512 * 1024, /* 512 KiB */
			666 * 1024, /* > 512 KiB, < 1024 KiB */
			1024 * 1024, /* 1024 KiB */
			1025 * 1024, /* > 1024 KiB */
		};

		for (i = 0; i < UPLOAD_STREAM_RESUMABLE_MAX_CONTENT_TYPE + 1; i++) {
			for (j = 0; j < G_N_ELEMENTS (file_sizes); j++) {
				for (k = 0; k < UPLOAD_STREAM_RESUMABLE_MAX_ERROR_TYPE + 1; k++) {
					UploadStreamResumableTestParams *test_params;
					gchar *test_name;
					gsize file_size;

					/* Ignore combinations of METADATA_ONLY with file_sizes or non-initial errors. */
					if (i == METADATA_ONLY && (j != 0 || k != 0)) {
						continue;
					} else if (i == METADATA_ONLY) {
						file_size = 0 /* bytes */;
					} else {
						file_size = file_sizes[j];
					}

					test_name = g_strdup_printf ("/upload-stream/resumable/%s/%s/%" G_GSIZE_FORMAT,
					                             upload_stream_resumable_content_type_names[i],
					                             upload_stream_resumable_error_type_names[k],
					                             file_size);

					/* Allocate a new struct. We leak this. */
					test_params = g_slice_new (UploadStreamResumableTestParams);
					test_params->content_type = i;
					test_params->file_size = file_size;
					test_params->error_type = k;

					g_test_add_data_func (test_name, test_params, test_upload_stream_resumable);

					g_free (test_name);
				}
			}
		}
	}

	return g_test_run ();
}
