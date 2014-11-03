/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/*
 * GData Client
 * Copyright (C) Philip Withnall 2010 <philip@tecnocode.co.uk>
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

#ifndef GDATA_GCONTACT_LANGUAGE_H
#define GDATA_GCONTACT_LANGUAGE_H

#include <glib.h>
#include <glib-object.h>

#include <gdata/gdata-parsable.h>

G_BEGIN_DECLS

#define GDATA_TYPE_GCONTACT_LANGUAGE		(gdata_gcontact_language_get_type ())
#define GDATA_GCONTACT_LANGUAGE(o)		(G_TYPE_CHECK_INSTANCE_CAST ((o), GDATA_TYPE_GCONTACT_LANGUAGE, GDataGContactLanguage))
#define GDATA_GCONTACT_LANGUAGE_CLASS(k)	(G_TYPE_CHECK_CLASS_CAST((k), GDATA_TYPE_GCONTACT_LANGUAGE, GDataGContactLanguageClass))
#define GDATA_IS_GCONTACT_LANGUAGE(o)		(G_TYPE_CHECK_INSTANCE_TYPE ((o), GDATA_TYPE_GCONTACT_LANGUAGE))
#define GDATA_IS_GCONTACT_LANGUAGE_CLASS(k)	(G_TYPE_CHECK_CLASS_TYPE ((k), GDATA_TYPE_GCONTACT_LANGUAGE))
#define GDATA_GCONTACT_LANGUAGE_GET_CLASS(o)	(G_TYPE_INSTANCE_GET_CLASS ((o), GDATA_TYPE_GCONTACT_LANGUAGE, GDataGContactLanguageClass))

typedef struct _GDataGContactLanguagePrivate	GDataGContactLanguagePrivate;

/**
 * GDataGContactLanguage:
 *
 * All the fields in the #GDataGContactLanguage structure are private and should never be accessed directly.
 *
 * Since: 0.7.0
 **/
typedef struct {
	GDataParsable parent;
	GDataGContactLanguagePrivate *priv;
} GDataGContactLanguage;

/**
 * GDataGContactLanguageClass:
 *
 * All the fields in the #GDataGContactLanguageClass structure are private and should never be accessed directly.
 *
 * Since: 0.7.0
 **/
typedef struct {
	/*< private >*/
	GDataParsableClass parent;

	/*< private >*/
	/* Padding for future expansion */
	void (*_g_reserved0) (void);
	void (*_g_reserved1) (void);
} GDataGContactLanguageClass;

GType gdata_gcontact_language_get_type (void) G_GNUC_CONST;

GDataGContactLanguage *gdata_gcontact_language_new (const gchar *code, const gchar *label) G_GNUC_WARN_UNUSED_RESULT G_GNUC_MALLOC;

const gchar *gdata_gcontact_language_get_code (GDataGContactLanguage *self) G_GNUC_PURE;
void gdata_gcontact_language_set_code (GDataGContactLanguage *self, const gchar *code);

const gchar *gdata_gcontact_language_get_label (GDataGContactLanguage *self) G_GNUC_PURE;
void gdata_gcontact_language_set_label (GDataGContactLanguage *self, const gchar *label);

G_END_DECLS

#endif /* !GDATA_GCONTACT_LANGUAGE_H */
