/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Philip Withnall 2008-2009 <philip@tecnocode.co.uk>
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

#include <glib.h>
#include <libxml/parser.h>
#include <libsoup/soup.h>

#include <gdata/gdata-service.h>

#ifndef GDATA_PRIVATE_H
#define GDATA_PRIVATE_H

G_BEGIN_DECLS

/**
 * GDataLogLevel:
 * @GDATA_LOG_NONE: Output no debug messages or network logs
 * @GDATA_LOG_MESSAGES: Output debug messages, but not network logs
 * @GDATA_LOG_HEADERS: Output debug messages and network traffic headers
 * @GDATA_LOG_FULL: Output debug messages and full network traffic logs
 *
 * Logging level.
 **/
typedef enum {
	GDATA_LOG_NONE = 0,
	GDATA_LOG_MESSAGES = 1,
	GDATA_LOG_HEADERS = 2,
	GDATA_LOG_FULL = 3
} GDataLogLevel;

#include "gdata-service.h"
G_GNUC_INTERNAL SoupSession *_gdata_service_get_session (GDataService *self) G_GNUC_PURE;
G_GNUC_INTERNAL void _gdata_service_set_authenticated (GDataService *self, gboolean authenticated);
G_GNUC_INTERNAL SoupMessage *_gdata_service_build_message (GDataService *self, const gchar *method, const gchar *uri, const gchar *etag, gboolean etag_if_match);
G_GNUC_INTERNAL guint _gdata_service_send_message (GDataService *self, SoupMessage *message, GCancellable *cancellable, GError **error);
G_GNUC_INTERNAL SoupMessage *_gdata_service_query (GDataService *self, const gchar *feed_uri, GDataQuery *query, GCancellable *cancellable,
                                                   GError **error) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
G_GNUC_INTERNAL const gchar *_gdata_service_get_scheme (void) G_GNUC_CONST;
G_GNUC_INTERNAL gchar *_gdata_service_build_uri (gboolean force_http,
                                                 const gchar *format, ...) G_GNUC_PRINTF (2, 3) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
G_GNUC_INTERNAL GDataLogLevel _gdata_service_get_log_level (void) G_GNUC_CONST;

#include "gdata-query.h"
G_GNUC_INTERNAL void _gdata_query_set_next_uri (GDataQuery *self, const gchar *next_uri);
G_GNUC_INTERNAL void _gdata_query_set_previous_uri (GDataQuery *self, const gchar *previous_uri);

#include "gdata-parsable.h"
G_GNUC_INTERNAL GDataParsable *_gdata_parsable_new_from_xml (GType parsable_type, const gchar *xml, gint length, gpointer user_data,
                                                             GError **error) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
G_GNUC_INTERNAL GDataParsable *_gdata_parsable_new_from_xml_node (GType parsable_type, xmlDoc *doc, xmlNode *node, gpointer user_data,
                                                                  GError **error) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
G_GNUC_INTERNAL void _gdata_parsable_get_xml (GDataParsable *self, GString *xml_string, gboolean declare_namespaces);
G_GNUC_INTERNAL void _gdata_parsable_string_append_escaped (GString *xml_string, const gchar *pre, const gchar *element_content, const gchar *post);
G_GNUC_INTERNAL gboolean _gdata_parsable_is_constructed_from_xml (GDataParsable *self);

#include "gdata-feed.h"
G_GNUC_INTERNAL GDataFeed *_gdata_feed_new (const gchar *title, const gchar *id, GTimeVal *updated) G_GNUC_WARN_UNUSED_RESULT;
G_GNUC_INTERNAL GDataFeed *_gdata_feed_new_from_xml (GType feed_type, const gchar *xml, gint length, GType entry_type,
                                                     GDataQueryProgressCallback progress_callback, gpointer progress_user_data,
                                                     GError **error) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;
G_GNUC_INTERNAL void _gdata_feed_add_entry (GDataFeed *self, GDataEntry *entry);
G_GNUC_INTERNAL gpointer _gdata_feed_parse_data_new (GType entry_type, GDataQueryProgressCallback progress_callback, gpointer progress_user_data);
G_GNUC_INTERNAL void _gdata_feed_parse_data_free (gpointer data);
G_GNUC_INTERNAL void _gdata_feed_call_progress_callback (GDataFeed *self, gpointer user_data, GDataEntry *entry);

#include "gdata-entry.h"
#include "gdata-batch-operation.h"
G_GNUC_INTERNAL void _gdata_entry_set_updated (GDataEntry *self, GTimeVal *updated);
G_GNUC_INTERNAL void _gdata_entry_set_batch_data (GDataEntry *self, guint id, GDataBatchOperationType type);

#include "gdata/services/documents/gdata-documents-service.h"

G_GNUC_INTERNAL GDataService *_gdata_documents_service_get_spreadsheet_service (GDataDocumentsService *self) G_GNUC_PURE;

#include "gdata-parser.h"

G_GNUC_INTERNAL GFileOutputStream *_gdata_download_stream_find_destination (const gchar *default_filename, GFile *target_dest_file, GFile **actual_dest_file,
                                                                            gboolean replace_file_if_exists,
                                                                            GCancellable *cancellable, GError **error) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

G_END_DECLS

#endif /* !GDATA_PRIVATE_H */
