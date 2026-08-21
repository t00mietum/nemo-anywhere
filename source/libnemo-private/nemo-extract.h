/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-extract.h - unpack an archive.

   Copyright (C) 2026 t00mietum.

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

/* Reading reaches much further than writing does. libarchive opens the tar,
 * zip, 7z, rar, cab, lha, cpio, xar and iso families and the bare compressors
 * on their own, so most of what a person double-clicks needs nothing installed.
 * A 7z or rar command is still reached for where libarchive will not open the
 * file at all - a multi-volume set, or headers it cannot decrypt - and that
 * path stages into a temporary folder so the same collision handling applies
 * to both.
 */

#ifndef NEMO_EXTRACT_H
#define NEMO_EXTRACT_H

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef enum {
	NEMO_EXTRACT_HERE,		/* straight into the destination folder */
	NEMO_EXTRACT_TO_SUBFOLDER	/* each archive into a folder of its own */
} NemoExtractLayout;

typedef enum {
	NEMO_EXTRACT_BACKEND_NONE,
	NEMO_EXTRACT_BACKEND_LIBARCHIVE,
	NEMO_EXTRACT_BACKEND_7Z,
	NEMO_EXTRACT_BACKEND_RAR
} NemoExtractBackend;

typedef void (* NemoExtractCallback) (GFile    *destination_dir,
				      gboolean  success,
				      gpointer  callback_data);

/* Whether the name looks like something we can unpack. Deliberately by name:
   it is the only answer available on win32, where a file's "mime type" is its
   extension, and it is what decides whether the menu items appear at all. */
gboolean nemo_extract_is_archive_name (const char *name);

/* The folder an archive unpacks into when each gets its own - the name with
   its archive suffix taken off. Returns NULL when nothing is left. */
char    *nemo_extract_folder_name     (const char *archive_name);

/* "notes.txt" at attempt 1 -> "notes (1).txt". The extension is kept so a
   renamed file still opens with the same program. */
char    *nemo_extract_unique_name     (const char *name,
				       guint       attempt);

/* An archive can name an entry anything at all, including a path that climbs
   out of the folder being unpacked into. Returns a relative, '/'-separated
   path with the root, any drive letter and every ".." component taken off, or
   NULL when nothing usable is left. */
char    *nemo_extract_sanitize_path   (const char *entry_path);

/* The command a 7z or rar backend would be run with. Exposed so the switches
   can be checked without spawning anything. */
char   **nemo_extract_build_command   (NemoExtractBackend  backend,
				       const char         *program,
				       const char         *archive_path,
				       const char         *destination_path,
				       const char         *password);

gboolean nemo_extract_backend_present (NemoExtractBackend backend);

/* Queues the job. archives are GFile *; every one of them is unpacked into
   destination_dir, or into a folder of its own beneath it. */
void nemo_extract_files (GList               *archives,
			 GFile               *destination_dir,
			 NemoExtractLayout    layout,
			 GtkWindow           *parent_window,
			 NemoExtractCallback  done_callback,
			 gpointer             done_callback_data);

#endif /* NEMO_EXTRACT_H */
