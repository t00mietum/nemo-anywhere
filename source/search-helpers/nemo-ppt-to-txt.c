/* Text out of a binary PowerPoint presentation (.ppt), for content search. The
 * record tree in the PowerPoint Document stream is walked and every text atom
 * printed, wherever it sits: slides, notes, titles, outline text.
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

#define RT_TEXT_CHARS 0x0FA0
#define RT_TEXT_BYTES 0x0FA8
#define RT_CSTRING    0x0FBA

/* Every record is an 8-byte header and a payload; a version nibble of 0xF marks a
 * container whose payload is more records. Atoms of any other kind are stepped
 * over, which is what keeps embedded pictures out of the output. */
static void
walk_records (const guint8 *data, gsize len, GString *out, int depth)
{
	gsize pos = 0;

	while (pos + 8 <= len) {
		guint16 ver_inst = rd16 (data + pos);
		guint16 type = rd16 (data + pos + 2);
		gsize rlen = rd32 (data + pos + 4);

		pos += 8;
		rlen = MIN (rlen, len - pos);

		if ((ver_inst & 0x0F) == 0x0F) {
			if (depth < 64) {
				walk_records (data + pos, rlen, out, depth + 1);
			}
		} else if (type == RT_TEXT_CHARS || type == RT_CSTRING) {
			helper_append_utf16 (out, data + pos, rlen / 2);
			g_string_append_c (out, '\n');
		} else if (type == RT_TEXT_BYTES) {
			helper_append_latin1 (out, data + pos, rlen);
			g_string_append_c (out, '\n');
		}

		pos += rlen;
	}
}

int
main (int argc, char *argv[])
{
	GsfInput *input;
	GsfInfile *ole;
	GError *error = NULL;
	GFile *file;
	GString *out;
	guint8 *data;
	gsize len = 0;

	if (argc < 2) {
		g_printerr ("Need a filename\n");
		return 1;
	}

	file = g_file_new_for_path (argv[1]);
	input = gsf_input_gio_new (file, &error);
	g_object_unref (file);

	if (input == NULL) {
		g_printerr ("Could not open ppt file: %s\n", error->message);
		g_error_free (error);
		return 1;
	}

	ole = gsf_infile_msole_new (input, &error);
	g_object_unref (input);

	if (ole == NULL) {
		g_printerr ("Not a PowerPoint file: %s\n", error->message);
		g_error_free (error);
		return 1;
	}

	data = helper_read_stream (ole, "PowerPoint Document", &len);
	g_object_unref (ole);

	if (data == NULL) {
		g_printerr ("No presentation stream in %s\n", argv[1]);
		return 1;
	}

	out = g_string_new (NULL);
	walk_records (data, len, out, 0);
	g_free (data);

	helper_clean (out);
	g_printf ("%s", out->str);
	g_string_free (out, TRUE);

	return 0;
}
