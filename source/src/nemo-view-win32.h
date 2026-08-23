/* Win32-only helpers for nemo-view, isolated so <windows.h>'s macro pollution
   (DELETE, ERROR, ...) stays out of the main view sources. */

#ifndef NEMO_VIEW_WIN32_H
#define NEMO_VIEW_WIN32_H

#include <glib.h>

/* Quote @arg as one command-line argument under MSVCRT rules. Exposed because
 * both callers below hand the result to a process that re-splits it, and the
 * trailing-backslash case (a drive root) has no unattended end-to-end path. */
gchar *nemo_view_win32_quote_arg (const gchar *arg);

/* Relaunch the running executable elevated ("runas"/UAC) at @path. */
void nemo_view_win32_open_elevated (const gchar *path);

/* Open the native console (Windows Terminal, PowerShell, or cmd) at @path. */
void nemo_view_win32_open_terminal (const gchar *path);

/* Hand @path to Explorer: a folder opens, anything else is revealed and picked
 * out inside its own folder. @is_directory says which. */
void nemo_view_win32_open_in_explorer (const gchar *path, gboolean is_directory);

#endif /* NEMO_VIEW_WIN32_H */
