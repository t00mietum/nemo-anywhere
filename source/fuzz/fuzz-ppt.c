/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fuzz-ppt.c - the PowerPoint record walk, on arbitrary bytes.

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

/* Records nest, so the walk recurses on a length the file gives it. Depth is
 * the only bound. Same arrangement as fuzz-xls.c: the helper source is
 * included whole and its main renamed out of the way. */

#include <stdint.h>

#define main nemo_ppt_to_txt_main
#include "../search-helpers/nemo-ppt-to-txt.c"
#undef main

int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
	GString *out = g_string_new (NULL);

	walk_records (data, size, out, 0);
	helper_clean (out);

	g_string_free (out, TRUE);

	return 0;
}
