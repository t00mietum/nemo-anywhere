/* Text out of a binary Word document (.doc), for content search. Word 97 and
 * later keep the text as pieces listed in a table stream, each piece either
 * one-byte code page text or UTF-16; Word 6 and 95 keep it in one run. Both
 * are read. Headers, footnotes and text boxes come out along with the body.
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

#define FIB_IDENT        0xA5EC
#define FIB_NFIB_97      0x00C1
#define FIB_FLAGS        0x0A
#define FIB_F_ENCRYPTED  0x0100
#define FIB_F_TABLE_1    0x0200
#define FIB_FC_MIN       0x18
#define FIB_FC_MAC       0x1C
#define FIB_FC_CLX       0x1A2
#define FIB_LCB_CLX      0x1A6
#define FIB_MIN_LEN      0x1AA

#define PIECE_COMPRESSED 0x40000000
#define PIECE_FC_MASK    0x3FFFFFFF

static void
append_piece (GString *out, const guint8 *doc, gsize doc_len, guint32 fc, gsize cp_count)
{
	gsize off = fc & PIECE_FC_MASK;

	if (fc & PIECE_COMPRESSED) {
		off /= 2;
		if (off < doc_len) {
			helper_append_cp1252 (out, doc + off, MIN (cp_count, doc_len - off));
		}
	} else if (off < doc_len) {
		helper_append_utf16 (out, doc + off, MIN (cp_count, (doc_len - off) / 2));
	}
}

/* The Clx: any number of property blobs, then the piece table proper - an array
 * of character positions and one descriptor per piece. */
static gboolean
read_pieces (GString *out, const guint8 *doc, gsize doc_len, const guint8 *clx, gsize clx_len)
{
	gsize pos = 0;

	while (pos < clx_len) {
		guint8 kind = clx[pos];

		if (kind == 0x01) {
			if (pos + 3 > clx_len) {
				return FALSE;
			}
			pos += 3 + rd16 (clx + pos + 1);
		} else if (kind == 0x02) {
			gsize lcb, n, i;
			const guint8 *plc;

			if (pos + 5 > clx_len) {
				return FALSE;
			}

			lcb = rd32 (clx + pos + 1);
			plc = clx + pos + 5;
			lcb = MIN (lcb, clx_len - (pos + 5));

			if (lcb < 4) {
				return FALSE;
			}

			n = (lcb - 4) / 12;

			for (i = 0; i < n; i++) {
				guint32 cp_start = rd32 (plc + i * 4);
				guint32 cp_end = rd32 (plc + (i + 1) * 4);
				const guint8 *pcd = plc + (n + 1) * 4 + i * 8;

				if (cp_end > cp_start) {
					append_piece (out, doc, doc_len, rd32 (pcd + 2), cp_end - cp_start);
				}
			}

			return n > 0;
		} else {
			return FALSE;
		}
	}

	return FALSE;
}

int
main (int argc, char *argv[])
{
	GsfInput *input;
	GsfInfile *ole;
	GError *error = NULL;
	GFile *file;
	GString *out;
	guint8 *doc, *table = NULL;
	gsize doc_len = 0, table_len = 0;
	guint16 flags;
	gboolean have_text = FALSE;

	if (argc < 2) {
		g_printerr ("Need a filename\n");
		return 1;
	}

	file = g_file_new_for_path (argv[1]);
	input = gsf_input_gio_new (file, &error);
	g_object_unref (file);

	if (input == NULL) {
		g_printerr ("Could not open doc file: %s\n", error->message);
		g_error_free (error);
		return 1;
	}

	ole = gsf_infile_msole_new (input, &error);
	g_object_unref (input);

	if (ole == NULL) {
		g_printerr ("Not a Word file: %s\n", error->message);
		g_error_free (error);
		return 1;
	}

	doc = helper_read_stream (ole, "WordDocument", &doc_len);

	if (doc == NULL || doc_len < 0x20 || rd16 (doc) != FIB_IDENT) {
		g_printerr ("No document stream in %s\n", argv[1]);
		g_free (doc);
		g_object_unref (ole);
		return 1;
	}

	flags = rd16 (doc + FIB_FLAGS);

	if (flags & FIB_F_ENCRYPTED) {
		g_free (doc);
		g_object_unref (ole);
		return 0;
	}

	out = g_string_new (NULL);

	if (rd16 (doc + 2) >= FIB_NFIB_97 && doc_len >= FIB_MIN_LEN) {
		table = helper_read_stream (ole, (flags & FIB_F_TABLE_1) ? "1Table" : "0Table", &table_len);

		if (table != NULL) {
			gsize fc = rd32 (doc + FIB_FC_CLX);
			gsize lcb = rd32 (doc + FIB_LCB_CLX);

			if (fc < table_len) {
				have_text = read_pieces (out, doc, doc_len, table + fc, MIN (lcb, table_len - fc));
			}
		}
	}

	if (!have_text) {
		/* Word 6/95, or a piece table that could not be read: the text sits in
		 * one run between fcMin and fcMac. */
		gsize fc_min = rd32 (doc + FIB_FC_MIN);
		gsize fc_mac = rd32 (doc + FIB_FC_MAC);

		if (fc_min < fc_mac && fc_mac <= doc_len) {
			helper_append_cp1252 (out, doc + fc_min, fc_mac - fc_min);
		}
	}

	g_free (table);
	g_free (doc);
	g_object_unref (ole);

	helper_clean (out);
	g_printf ("%s", out->str);
	g_string_free (out, TRUE);

	return 0;
}
