/* Win32-only: follow the Windows system light/dark app-theme setting. Isolated
   from GTK sources so <windows.h>'s macro pollution stays contained. */

#ifndef NEMO_WIN32_APPEARANCE_H
#define NEMO_WIN32_APPEARANCE_H

#include <glib.h>

/* Called (on the main loop) with the current setting: TRUE = prefer dark. */
typedef void (*NemoWin32DarkChanged) (gboolean dark, gpointer user_data);

/* TRUE if Windows is set to a dark app theme (AppsUseLightTheme == 0). */
gboolean nemo_win32_prefers_dark (void);

/* Start watching the setting; @cb fires on the main loop whenever it changes.
   Call once. Reads the current value with nemo_win32_prefers_dark () yourself
   for the initial state. */
void nemo_win32_watch_dark (NemoWin32DarkChanged cb, gpointer user_data);

#endif /* NEMO_WIN32_APPEARANCE_H */
