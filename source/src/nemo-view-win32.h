/* Win32-only helpers for nemo-view, isolated so <windows.h>'s macro pollution
   (DELETE, ERROR, ...) stays out of the main view sources. */

#ifndef NEMO_VIEW_WIN32_H
#define NEMO_VIEW_WIN32_H

#include <glib.h>

/* Relaunch the running executable elevated ("runas"/UAC) at @path. */
void nemo_view_win32_open_elevated (const gchar *path);

/* Open the native console (Windows Terminal, PowerShell, or cmd) at @path. */
void nemo_view_win32_open_terminal (const gchar *path);

#endif /* NEMO_VIEW_WIN32_H */
