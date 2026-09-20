/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fuzz-doc.c - the Word piece table, on arbitrary bytes.

   Copyright © 2026 t00mietum (CryptogID: ปʬϝღถɔ4რఠΔթะ9ƾǝu).

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
   Boston, MA 02110-1335, USA.
*/

/* The piece table sits in one stream and points into another, so the text a
 * .doc yields comes from two buffers that need not agree with each other. The
 * first two bytes of the input say where to cut, which lets the search move
 * the boundary as well as the contents. Same arrangement as fuzz-xls.c. */

#include <stdint.h>

#define main nemo_doc_to_txt_main
#include "../search-helpers/nemo-doc-to-txt.c"
#undef main

int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
	GString *out;
	const guint8 *body;
	gsize body_len, split;

	if (size < 2) {
		return 0;
	}

	body = (const guint8 *) data + 2;
	body_len = size - 2;
	split = MIN ((gsize) rd16 ((const guint8 *) data), body_len);

	out = g_string_new (NULL);

	read_pieces (out, body, split, body + split, body_len - split);
	helper_clean (out);

	g_string_free (out, TRUE);

	return 0;
}
