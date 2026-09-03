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

/* Everything a shortcut says about what it runs. Any field may be empty. */
typedef struct {
	char *target;
	char *arguments;
	char *working_dir;
	char *description;
} NemoShortcutInfo;

gboolean nemo_shortcut_win32_read_info (const char        *lnk_path,
                                        NemoShortcutInfo  *info,
                                        GError           **error);
gboolean nemo_shortcut_win32_update    (const char             *lnk_path,
                                        const NemoShortcutInfo *info,
                                        GError                **error);
void     nemo_shortcut_info_clear      (NemoShortcutInfo *info);

/* Whether the shortcut points at a folder, answered from what the .lnk itself
 * records rather than by looking at the target. mtime keys the cache. */
gboolean nemo_shortcut_win32_target_is_dir (const char *lnk_path,
                                            gint64      mtime);

/* Open a .lnk the way a double-click in the shell would: the target's
 * associated program for a document, the program itself for an executable,
 * carrying the arguments, working directory and window state stored in the
 * shortcut. Handing over the shortcut rather than its target is the point -
 * none of that is reachable from the target path alone. */
gboolean nemo_shortcut_win32_launch (const char  *lnk_path,
                                     GError     **error);

G_END_DECLS

#endif /* NEMO_SHORTCUT_WIN32_H */
