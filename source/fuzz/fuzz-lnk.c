/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* fuzz-lnk.c - the Windows shortcut reader, on arbitrary bytes.

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

/* Any file named .lnk is read for its icon when a folder is listed, so this
 * reads whatever turns up on a share. Only the parse: where the target is
 * would mean looking at this machine's mounts. */

#include <config.h>

#include <stdint.h>

#include <glib.h>

#include <libnemo-private/nemo-lnk.h>

int LLVMFuzzerTestOneInput (const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput (const uint8_t *data, size_t size)
{
	NemoLnk lnk;

	if (nemo_lnk_parse (data, size, &lnk)) {
		g_free (nemo_lnk_display_target (&lnk));
		nemo_lnk_clear (&lnk);
	}

	return 0;
}
