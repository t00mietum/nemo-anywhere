/* nemo-favorite-vfs-file-enumerator.h - favorites:/// vfs.
 *
 * Adapted from libxapp 2.8.8 (favorite-vfs-file-enumerator.h, LGPL-2.1-or-later,
 * © Linux Mint team), relicensed under GPL-2.0 per LGPL-2.1 section 3.
 */

#ifndef NEMO_FAVORITE_VFS_FILE_ENUMERATOR_H
#define NEMO_FAVORITE_VFS_FILE_ENUMERATOR_H

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NEMO_TYPE_NEMO_FAVORITE_VFS_FILE_ENUMERATOR nemo_favorite_vfs_file_enumerator_get_type()

G_DECLARE_FINAL_TYPE (NemoFavoriteVfsFileEnumerator, nemo_favorite_vfs_file_enumerator, \
                      NEMO_FAVORITE, VFS_FILE_ENUMERATOR, \
                      GFileEnumerator)

GFileEnumerator *
nemo_favorite_vfs_file_enumerator_new (GFile               *file,
                                  const gchar         *attributes,
                                  GFileQueryInfoFlags  flags,
                                  GList               *favorites);

G_END_DECLS

#endif // NEMO_FAVORITE_VFS_FILE_ENUMERATOR_H
