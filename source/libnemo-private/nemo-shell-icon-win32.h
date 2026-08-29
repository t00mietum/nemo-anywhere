/* nemo-shell-icon-win32.h - the icon the Windows shell would draw for a file
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef NEMO_SHELL_ICON_WIN32_H
#define NEMO_SHELL_ICON_WIN32_H

#include <glib.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

/* The shell's icon for the file at path, exactly pixel_size square: taken
 * from the system image list of that size (16, 32, 48, 256) or scaled down
 * from the next one up. NULL when the shell has none. Cached by path, size
 * and the file's modification time. */
GdkPixbuf *nemo_shell_icon_win32_for_path (const gchar *path,
					   gint         pixel_size,
					   gint64       mtime);

void       nemo_shell_icon_win32_clear_cache (void);

#endif /* NEMO_SHELL_ICON_WIN32_H */
