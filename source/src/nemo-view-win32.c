/* Win32-only helpers for nemo-view. Kept in its own file because <windows.h>
   defines macros (DELETE, ERROR, ...) that collide with enum identifiers in the
   main nemo-view.c. */

#include <config.h>

#include <glib.h>

#ifdef G_OS_WIN32

#include "nemo-view-win32.h"

#include <windows.h>
#include <shellapi.h>

void
nemo_view_win32_open_elevated (const gchar *path)
{
	wchar_t exe[MAX_PATH];

	if (GetModuleFileNameW (NULL, exe, MAX_PATH) == 0) {
		return;
	}

	wchar_t *wpath = (wchar_t *) g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
	ShellExecuteW (NULL, L"runas", exe, wpath, NULL, SW_SHOWNORMAL);
	g_free (wpath);
}

void
nemo_view_win32_open_terminal (const gchar *path)
{
	/* Prefer Windows Terminal, then PowerShell, then cmd (always present).
	   ShellExecute (not g_spawn) so the child reliably gets a visible console;
	   wt takes the folder via -d, the classic shells via the working dir. */
	const gchar *candidates[] = { "wt.exe", "pwsh.exe", "powershell.exe", "cmd.exe" };
	gchar *found = NULL;
	guint i;

	for (i = 0; i < G_N_ELEMENTS (candidates); i++) {
		found = g_find_program_in_path (candidates[i]);
		if (found != NULL) {
			break;
		}
	}

	const gchar *exe = found ? found : "cmd.exe";
	gboolean is_wt = g_str_has_suffix (exe, "wt.exe");

	wchar_t *wexe = (wchar_t *) g_utf8_to_utf16 (exe, -1, NULL, NULL, NULL);

	if (is_wt) {
		gchar *params = g_strdup_printf ("-d \"%s\"", path);
		wchar_t *wparams = (wchar_t *) g_utf8_to_utf16 (params, -1, NULL, NULL, NULL);
		ShellExecuteW (NULL, L"open", wexe, wparams, NULL, SW_SHOWNORMAL);
		g_free (params);
		g_free (wparams);
	} else {
		wchar_t *wdir = (wchar_t *) g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
		ShellExecuteW (NULL, L"open", wexe, NULL, wdir, SW_SHOWNORMAL);
		g_free (wdir);
	}

	g_free (found);
	g_free (wexe);
}

#endif /* G_OS_WIN32 */
