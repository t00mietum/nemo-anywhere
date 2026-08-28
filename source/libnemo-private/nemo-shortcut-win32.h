/* nemo-shortcut-win32.h - create Windows .lnk shell shortcuts (the analog of
 * POSIX symlinks / .desktop launchers). Empty on non-Windows.
 *
 * Copyright © 2026 Bubbles
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by the
 * Free Software Foundation.
 */

#ifndef NEMO_SHORTCUT_WIN32_H
#define NEMO_SHORTCUT_WIN32_H

#include <glib.h>

G_BEGIN_DECLS

/* Write a .lnk shortcut at lnk_path pointing at target_path (both absolute
 * native Windows paths). working_dir/arguments/description may be NULL. */
gboolean nemo_shortcut_win32_create (const char  *target_path,
                                     const char  *lnk_path,
                                     const char  *working_dir,
                                     const char  *arguments,
                                     const char  *description,
                                     GError     **error);

/* Read the file-system target a .lnk points at. Returns FALSE (and leaves
 * *target_path NULL) if the shortcut has no path target - e.g. it points at a
 * virtual shell item. Caller frees *target_path. */
gboolean nemo_shortcut_win32_read   (const char  *lnk_path,
                                     char       **target_path,
                                     GError     **error);

/* Open a .lnk the way a double-click in the shell would: the target's
 * associated program for a document, the program itself for an executable,
 * carrying the arguments, working directory and window state stored in the
 * shortcut. Handing over the shortcut rather than its target is the point -
 * none of that is reachable from the target path alone. */
gboolean nemo_shortcut_win32_launch (const char  *lnk_path,
                                     GError     **error);

/* Make a real NTFS symlink at link_path pointing at target_path. A different
 * thing from a .lnk: the file system follows it, so every program sees the
 * target rather than a document that happens to point somewhere. */
gboolean nemo_shortcut_win32_create_symlink (const char  *target_path,
                                             const char  *link_path,
                                             GError     **error);

/* Whether this process is allowed to make symlinks at all. Windows wants either
 * Developer Mode or an elevated run, so the answer is found by trying once. */
gboolean nemo_shortcut_win32_symlinks_allowed (void);

G_END_DECLS

#endif /* NEMO_SHORTCUT_WIN32_H */
