/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fuzz-xls.c - the BIFF record walker, on arbitrary bytes.

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

/* A .xls is whatever the file the search touched happens to contain, and the
 * record walk below it reads lengths and offsets straight out of that. It has
 * been wrong once already: the shared-string loop trusted a count read from
 * the file and tested the wrong end, so a truncated workbook spun for four
 * billion passes.
 *
 * The helper source is included whole rather than linked, because the parser
 * is static and there is no reason to open it up for a fuzz target. Its main
 * is renamed out of the way; the driver supplies the real one. */

#include <stdint.h>

#define main nemo_xls_to_txt_main
#include "../search-helpers/nemo-xls-to-txt.c"
#undef main

int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
	GString *out = g_string_new (NULL);

	parse_biff (data, size, out);
	helper_clean (out);

	g_string_free (out, TRUE);

	return 0;
}
