/* Text out of a binary Excel workbook (.xls, BIFF5 through BIFF8), for content
 * search. Shared strings, plain labels, formula results, numbers and sheet names
 * are printed; layout and formatting are not.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <stdlib.h>
#include <glib.h>
#include <gio/gio.h>
#include <glib/gprintf.h>
#include <gsf/gsf.h>

#include "nemo-helper-util.h"

typedef struct {
	const guint8 *data;
	gsize len;
	gsize pos;
	gsize rec_end;
	gboolean biff8;
	GString *out;
} Biff;

#define REC_BOF8       0x0809
#define REC_BOF2       0x0009
#define REC_BOF3       0x0209
#define REC_BOF4       0x0409
#define REC_EOF        0x000A
#define REC_FORMULA    0x0006
#define REC_CONTINUE   0x003C
#define REC_BOUNDSHEET 0x0085
#define REC_MULRK      0x00BD
#define REC_RSTRING    0x00D6
#define REC_SST        0x00FC
#define REC_NUMBER     0x0203
#define REC_LABEL      0x0204
#define REC_STRING     0x0207
#define REC_RK         0x027E

/* Long strings run on into CONTINUE records. Step into the next one, if that is
 * what follows. */
static gboolean
biff_continue (Biff *b)
{
	gsize len;

	if (b->rec_end + 4 > b->len || rd16 (b->data + b->rec_end) != REC_CONTINUE) {
		return FALSE;
	}

	len = rd16 (b->data + b->rec_end + 2);
	b->pos = b->rec_end + 4;
	b->rec_end = MIN (b->pos + len, b->len);
	return TRUE;
}

/* n more bytes of a field, which may sit at the start of the next CONTINUE. A
 * field split down the middle is given up on. */
static gboolean
biff_need (Biff *b, gsize n)
{
	while (b->pos + n > b->rec_end) {
		if (b->pos < b->rec_end || !biff_continue (b)) {
			return FALSE;
		}
	}

	return TRUE;
}

static guint8
biff_u8 (Biff *b)
{
	return b->data[b->pos++];
}

static guint16
biff_u16 (Biff *b)
{
	guint16 v = rd16 (b->data + b->pos);

	b->pos += 2;
	return v;
}

static guint32
biff_u32 (Biff *b)
{
	guint32 v = rd32 (b->data + b->pos);

	b->pos += 4;
	return v;
}

static void
biff_skip (Biff *b, gsize n)
{
	while (n > 0) {
		gsize take;

		if (b->pos >= b->rec_end && !biff_continue (b)) {
			return;
		}

		take = MIN (n, b->rec_end - b->pos);
		b->pos += take;
		n -= take;
	}
}

/* cch characters of a BIFF8 string body. Where the body crosses into a CONTINUE
 * record, the continuation opens with a fresh flags byte of its own. */
static void
biff_chars (Biff *b, gsize cch, gboolean wide)
{
	while (cch > 0) {
		gsize unit, take;

		if (b->pos >= b->rec_end) {
			if (!biff_continue (b) || b->pos >= b->rec_end) {
				break;
			}
			wide = biff_u8 (b) & 1;
		}

		unit = wide ? 2 : 1;
		take = MIN ((b->rec_end - b->pos) / unit, cch);

		if (take == 0) {
			b->pos = b->rec_end;
			continue;
		}

		if (wide) {
			helper_append_utf16 (b->out, b->data + b->pos, take);
		} else {
			helper_append_latin1 (b->out, b->data + b->pos, take);
		}

		b->pos += take * unit;
		cch -= take;
	}

	g_string_append_c (b->out, ' ');
}

/* XLUnicodeRichExtendedString: the shape of every BIFF8 string, with optional
 * formatting runs and Asian phonetic data hanging off the end. */
static void
biff_string8 (Biff *b)
{
	guint16 cch, runs = 0;
	guint32 ext = 0;
	guint8 flags;

	if (!biff_need (b, 3)) {
		return;
	}

	cch = biff_u16 (b);
	flags = biff_u8 (b);

	if ((flags & 0x08) && biff_need (b, 2)) {
		runs = biff_u16 (b);
	}
	if ((flags & 0x04) && biff_need (b, 4)) {
		ext = biff_u32 (b);
	}

	biff_chars (b, cch, flags & 1);
	biff_skip (b, (gsize) runs * 4 + ext);
}

/* The older one-byte strings, in the workbook's code page. */
static void
biff_string5 (Biff *b, gsize cch)
{
	if (!biff_need (b, cch)) {
		return;
	}

	helper_append_cp1252 (b->out, b->data + b->pos, cch);
	g_string_append_c (b->out, ' ');
	b->pos += cch;
}

static void
biff_cell_string (Biff *b)
{
	if (b->biff8) {
		biff_string8 (b);
	} else if (biff_need (b, 2)) {
		biff_string5 (b, biff_u16 (b));
	}
}

/* RK: a number squeezed into 30 bits, as an integer or the top of a double. */
static double
rk_value (guint32 rk)
{
	double v;

	if (rk & 2) {
		v = (double) ((gint32) rk >> 2);
	} else {
		guint64 bits = ((guint64) (rk & ~3u)) << 32;

		memcpy (&v, &bits, sizeof v);
	}

	return (rk & 1) ? v / 100 : v;
}

static void
append_number (GString *out, double v)
{
	g_string_append_printf (out, "%.15g ", v);
}

static void
parse_biff (const guint8 *data, gsize len, GString *out)
{
	Biff b = { data, len, 0, 0, TRUE, out };
	gsize pos = 0;

	while (pos + 4 <= len) {
		guint16 type = rd16 (data + pos);
		guint16 rlen = rd16 (data + pos + 2);

		b.pos = pos + 4;
		b.rec_end = MIN (b.pos + rlen, len);

		switch (type) {
		case REC_BOF8:
		case REC_BOF2:
		case REC_BOF3:
		case REC_BOF4:
			if (biff_need (&b, 2)) {
				b.biff8 = biff_u16 (&b) >= 0x0600;
			}
			break;

		case REC_SST:
			if (biff_need (&b, 8)) {
				guint32 unique;

				biff_u32 (&b);
				unique = biff_u32 (&b);

				while (unique-- > 0 && b.pos < b.len) {
					biff_string8 (&b);
				}
			}
			break;

		case REC_LABEL:
		case REC_RSTRING:
			if (biff_need (&b, 6)) {
				b.pos += 6;
				biff_cell_string (&b);
			}
			break;

		case REC_STRING:
			biff_cell_string (&b);
			break;

		case REC_BOUNDSHEET:
			if (biff_need (&b, 8)) {
				guint8 cch;

				b.pos += 6;
				cch = biff_u8 (&b);

				if (b.biff8) {
					guint8 flags = biff_u8 (&b);

					biff_chars (&b, cch, flags & 1);
				} else {
					biff_string5 (&b, cch);
				}
			}
			break;

		case REC_NUMBER:
			if (biff_need (&b, 14)) {
				append_number (out, rd_double (data + b.pos + 6));
			}
			break;

		case REC_RK:
			if (biff_need (&b, 10)) {
				append_number (out, rk_value (rd32 (data + b.pos + 6)));
			}
			break;

		case REC_MULRK:
			/* row, first column, then (ixfe, rk) pairs up to a trailing last column */
			b.pos += 4;
			while (b.pos + 8 <= b.rec_end) {
				append_number (out, rk_value (rd32 (data + b.pos + 2)));
				b.pos += 6;
			}
			break;

		case REC_FORMULA:
			/* A string result is carried by the STRING record that follows. */
			if (biff_need (&b, 14) && rd16 (data + b.pos + 12) != 0xFFFF) {
				append_number (out, rd_double (data + b.pos + 6));
			}
			break;

		default:
			break;
		}

		pos = b.rec_end;
	}
}

static gboolean
looks_like_biff (const guint8 *data, gsize len)
{
	guint16 type;

	if (len < 4) {
		return FALSE;
	}

	type = rd16 (data);
	return type == REC_BOF8 || type == REC_BOF2 || type == REC_BOF3 || type == REC_BOF4;
}

int
main (int argc, char *argv[])
{
	GsfInput *input;
	GsfInfile *ole;
	GError *error = NULL;
	GFile *file;
	GString *out;
	guint8 *data = NULL;
	gsize len = 0;

	if (argc < 2) {
		g_printerr ("Need a filename\n");
		return 1;
	}

	file = g_file_new_for_path (argv[1]);
	input = gsf_input_gio_new (file, &error);
	g_object_unref (file);

	if (input == NULL) {
		g_printerr ("Could not open xls file: %s\n", error->message);
		g_error_free (error);
		return 1;
	}

	ole = gsf_infile_msole_new (input, NULL);

	if (ole != NULL) {
		data = helper_read_stream (ole, "Workbook", &len);
		if (data == NULL) {
			data = helper_read_stream (ole, "Book", &len);
		}
		g_object_unref (ole);
	} else {
		/* The oldest workbooks are a bare record stream with no container. */
		data = helper_read_input (input, &len);
		if (data != NULL && !looks_like_biff (data, len)) {
			g_free (data);
			data = NULL;
		}
	}

	g_object_unref (input);

	if (data == NULL) {
		g_printerr ("Not an Excel workbook: %s\n", argv[1]);
		return 1;
	}

	out = g_string_new (NULL);
	parse_biff (data, len, out);
	g_free (data);

	helper_clean (out);
	g_printf ("%s\n", out->str);
	g_string_free (out, TRUE);

	return 0;
}
