/* Shared bits for the document-to-text helpers: whole-stream reads out of an
 * OLE2 container, and the text encodings the binary Office formats use.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef NEMO_HELPER_UTIL_H
#define NEMO_HELPER_UTIL_H

#include <string.h>
#include <glib.h>
#include <gsf/gsf.h>

static inline guint16
rd16 (const guint8 *p)
{
	return (guint16) (p[0] | (p[1] << 8));
}

static inline guint32
rd32 (const guint8 *p)
{
	return (guint32) p[0] | ((guint32) p[1] << 8) | ((guint32) p[2] << 16) | ((guint32) p[3] << 24);
}

static inline double
rd_double (const guint8 *p)
{
	double v;

	memcpy (&v, p, sizeof v);
	return v;
}

/* The whole of one child stream, or NULL when the container has no such stream. */
static guint8 *
helper_read_stream (GsfInfile *infile, const char *name, gsize *len)
{
	GsfInput *stream;
	gsf_off_t size;
	guint8 *buf;

	stream = gsf_infile_child_by_name (infile, name);
	if (stream == NULL) {
		return NULL;
	}

	size = gsf_input_size (stream);
	buf = g_malloc (size + 1);

	if (size > 0 && gsf_input_read (stream, size, buf) == NULL) {
		g_free (buf);
		g_object_unref (stream);
		return NULL;
	}

	g_object_unref (stream);
	*len = size;
	return buf;
}

static guint8 *
helper_read_input (GsfInput *input, gsize *len)
{
	gsf_off_t size = gsf_input_size (input);
	guint8 *buf = g_malloc (size + 1);

	if (size > 0 && gsf_input_read (input, size, buf) == NULL) {
		g_free (buf);
		return NULL;
	}

	*len = size;
	return buf;
}

static void
helper_append_utf16 (GString *out, const guint8 *data, gsize n_units)
{
	gsize written = 0;
	gchar *utf8;

	utf8 = g_convert ((const gchar *) data, n_units * 2, "UTF-8", "UTF-16LE", NULL, &written, NULL);
	if (utf8 != NULL) {
		g_string_append_len (out, utf8, written);
		g_free (utf8);
	}
}

/* The one-byte form of a UTF-16 string: every byte is a code unit's low half. */
static void
helper_append_latin1 (GString *out, const guint8 *data, gsize n)
{
	gsize i;

	for (i = 0; i < n; i++) {
		g_string_append_unichar (out, data[i]);
	}
}

static void
helper_append_cp1252 (GString *out, const guint8 *data, gsize n)
{
	gsize written = 0;
	gchar *utf8;

	utf8 = g_convert ((const gchar *) data, n, "UTF-8", "WINDOWS-1252", NULL, &written, NULL);
	if (utf8 != NULL) {
		g_string_append_len (out, utf8, written);
		g_free (utf8);
	} else {
		helper_append_latin1 (out, data, n);
	}
}

/* Control characters mark paragraphs, cells and fields in the binary formats;
 * a line break for the paragraph ones, a space for the rest. UTF-8 never puts a
 * byte under 0x80 inside a multibyte sequence, so a byte-wise pass is safe. */
static void
helper_clean (GString *s)
{
	gsize i;

	for (i = 0; i < s->len; i++) {
		guchar c = (guchar) s->str[i];

		if (c >= 0x20 || c == '\t' || c == '\n') {
			continue;
		}

		s->str[i] = (c == '\r' || c == 0x0B || c == 0x0C) ? '\n' : ' ';
	}
}

#endif
