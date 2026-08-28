/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-dir-enum.h - directory enumeration that survives a long path on Windows.

   Copyright © 2026 t00mietum.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef NEMO_DIR_ENUM_H
#define NEMO_DIR_ENUM_H

#include <gio/gio.h>

G_BEGIN_DECLS

/* Same contract as g_file_enumerate_children and friends. Everywhere except a
   long local path on Windows these hand straight over to GLib.

   The exception exists because GLib's own directory walk loses its way past
   MAX_PATH: it reports success and then lists the process's working directory
   instead of the folder asked for, which reads as an empty folder here and
   would send a recursive walk into the wrong tree. See design.md. */

GFileEnumerator *nemo_enumerate_children        (GFile                *dir,
                                                 const char           *attributes,
                                                 GFileQueryInfoFlags   flags,
                                                 GCancellable         *cancellable,
                                                 GError              **error);

void             nemo_enumerate_children_async  (GFile                *dir,
                                                 const char           *attributes,
                                                 GFileQueryInfoFlags   flags,
                                                 int                   io_priority,
                                                 GCancellable         *cancellable,
                                                 GAsyncReadyCallback   callback,
                                                 gpointer              user_data);

GFileEnumerator *nemo_enumerate_children_finish (GFile                *dir,
                                                 GAsyncResult         *result,
                                                 GError              **error);

/* TRUE when dir is a local path long enough that GLib's walk cannot be trusted.
   Exposed for the tests; callers should just use the three above. */
gboolean         nemo_dir_enum_path_is_long     (GFile                *dir);

G_END_DECLS

#endif /* NEMO_DIR_ENUM_H */
