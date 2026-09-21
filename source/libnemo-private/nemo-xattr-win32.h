/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-xattr-win32.h - alternate data streams, the Windows answer to xattrs.

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

/* Use nemo-file-xattr.h instead. This is the half of it that only exists on
 * Windows.
 *
 * A stream is named `path:stream`, and opening one on a file system that has
 * no streams fails rather than doing something surprising. That covers FAT and
 * exFAT, which is what a USB stick usually is, and most network shares.
 */

#ifndef NEMO_XATTR_WIN32_H
#define NEMO_XATTR_WIN32_H

#include <glib.h>

G_BEGIN_DECLS

char     *nemo_xattr_win32_get (const char *path, const char *name);
gboolean  nemo_xattr_win32_set (const char *path, const char *name, const char *value);

G_END_DECLS

#endif /* NEMO_XATTR_WIN32_H */
