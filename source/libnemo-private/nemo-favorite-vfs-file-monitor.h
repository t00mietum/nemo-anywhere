/* nemo-favorite-vfs-file-monitor.h - favorites:/// vfs.
 *
 * Adapted from libxapp 2.8.8 (favorite-vfs-file-monitor.h, LGPL-2.1-or-later,
 * © Linux Mint team), relicensed under GPL-2.0 per LGPL-2.1 section 3.
 */

#ifndef __NEMO_FAVORITE_VFS_FILE_MONITOR_H__
#define __NEMO_FAVORITE_VFS_FILE_MONITOR_H__

#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define NEMO_TYPE_NEMO_FAVORITE_VFS_FILE_MONITOR       (nemo_favorite_vfs_file_monitor_get_type ())

G_DECLARE_FINAL_TYPE (NemoFavoriteVfsFileMonitor, nemo_favorite_vfs_file_monitor,
                      NEMO_FAVORITE, VFS_FILE_MONITOR, GFileMonitor)

GFileMonitor *nemo_favorite_vfs_file_monitor_new (void);

G_END_DECLS

#endif /* __NEMO_FAVORITE_VFS_FILE_MONITOR_H__ */
