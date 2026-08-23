/* Win32-only helpers for nemo-view. Kept in its own file because <windows.h>
   defines macros (DELETE, ERROR, ...) that collide with enum identifiers in the
   main nemo-view.c. */

#include <config.h>

#include <glib.h>

#ifdef G_OS_WIN32

#include "nemo-view-win32.h"

#include <libnemo-private/nemo-config.h>

#include <glib/gi18n.h>
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

/* ShellExecuteW returns >32 on success; anything at or below is an error code.
   Ignoring it meant a refused UAC prompt or a missing shell just did nothing. */
static gboolean
shell_execute_ok (HINSTANCE result, const gchar *what, const gchar *target)
{
	if ((INT_PTR) result > 32) {
		return TRUE;
	}

	g_warning ("%s failed for '%s' (ShellExecute code %d)",
		   what, target, (int) (INT_PTR) result);
	return FALSE;
}

/* Command-line quoting, MSVCRT rules: a run of backslashes immediately before
   the closing quote has to be doubled, or the quote is escaped away. A drive
   root ("C:\") is the common case and used to swallow it. */
gchar *
nemo_view_win32_quote_arg (const gchar *arg)
{
	GString *out = g_string_new ("\"");
	gsize i, backslashes = 0;

	for (i = 0; arg[i] != '\0'; i++) {
		if (arg[i] == '\\') {
			backslashes++;
			continue;
		}

		if (arg[i] == '"') {
			gsize n;

			for (n = 0; n < backslashes * 2 + 1; n++) {
				g_string_append_c (out, '\\');
			}
		} else {
			gsize n;

			for (n = 0; n < backslashes; n++) {
				g_string_append_c (out, '\\');
			}
		}
		backslashes = 0;
		g_string_append_c (out, arg[i]);
	}

	for (i = 0; i < backslashes * 2; i++) {
		g_string_append_c (out, '\\');
	}
	g_string_append_c (out, '"');

	{
		gchar *ret = g_strdup (out->str);

		g_string_free (out, TRUE);
		return ret;
	}
}

void
nemo_view_win32_open_elevated (const gchar *path)
{
	wchar_t exe[MAX_PATH];
	gchar *quoted;
	wchar_t *wpath;
	HINSTANCE res;

	if (GetModuleFileNameW (NULL, exe, MAX_PATH) == 0) {
		g_warning ("Open as Administrator: cannot find our own executable");
		return;
	}

	quoted = nemo_view_win32_quote_arg (path);
	wpath = (wchar_t *) g_utf8_to_utf16 (quoted, -1, NULL, NULL, NULL);
	res = ShellExecuteW (NULL, L"runas", exe, wpath, NULL, SW_SHOWNORMAL);
	shell_execute_ok (res, "Open as Administrator", path);
	g_free (wpath);
	g_free (quoted);
}

void
nemo_view_win32_open_terminal (const gchar *path)
{
	/* The candidate list is a setting so a preferred shell can be put first;
	   the default is Windows Terminal, then PowerShell, then cmd (always
	   present). ShellExecute rather than g_spawn so the child reliably gets a
	   visible console. wt takes the folder via -d, the classic shells via the
	   working directory. */
	gchar **candidates;
	gchar *found = NULL;
	const gchar *exe;
	gboolean is_wt;
	wchar_t *wexe;
	HINSTANCE res;
	guint i;

	candidates = nemo_config_get_strv (nemo_config_get_group ("terminal"), "win32-candidates");

	for (i = 0; candidates != NULL && candidates[i] != NULL; i++) {
		found = g_find_program_in_path (candidates[i]);
		if (found != NULL) {
			break;
		}
	}
	g_strfreev (candidates);

	exe = found ? found : "cmd.exe";
	is_wt = g_str_has_suffix (exe, "wt.exe");

	wexe = (wchar_t *) g_utf8_to_utf16 (exe, -1, NULL, NULL, NULL);

	if (is_wt) {
		gchar *quoted = nemo_view_win32_quote_arg (path);
		gchar *params = g_strconcat ("-d ", quoted, NULL);
		wchar_t *wparams = (wchar_t *) g_utf8_to_utf16 (params, -1, NULL, NULL, NULL);

		res = ShellExecuteW (NULL, L"open", wexe, wparams, NULL, SW_SHOWNORMAL);
		g_free (quoted);
		g_free (params);
		g_free (wparams);
	} else {
		wchar_t *wdir = (wchar_t *) g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);

		res = ShellExecuteW (NULL, L"open", wexe, NULL, wdir, SW_SHOWNORMAL);
		g_free (wdir);
	}

	shell_execute_ok (res, "Open in Terminal", path);

	g_free (found);
	g_free (wexe);
}

/* Reveal @path in its own folder by starting an Explorer of our own. This is
   the route that still works when nemo is running elevated, where talking to
   the desktop's Explorer over COM is refused. */
static gboolean
explorer_select_by_command_line (const gchar *path)
{
	gchar *quoted = nemo_view_win32_quote_arg (path);
	gchar *params = g_strconcat ("/select,", quoted, NULL);
	wchar_t *wparams = (wchar_t *) g_utf8_to_utf16 (params, -1, NULL, NULL, NULL);
	HINSTANCE res;

	res = ShellExecuteW (NULL, L"open", L"explorer.exe", wparams, NULL, SW_SHOWNORMAL);

	g_free (quoted);
	g_free (params);
	g_free (wparams);

	return shell_execute_ok (res, "Open with Explorer", path);
}

/* The one place that deliberately hands work to Explorer, because the item says
   Explorer on it. Nothing else in the tree depends on it. */
void
nemo_view_win32_open_in_explorer (const gchar *path,
				  gboolean     is_directory)
{
	wchar_t *wpath;
	PIDLIST_ABSOLUTE item;
	HRESULT hr;

	g_return_if_fail (path != NULL);

	wpath = (wchar_t *) g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
	if (wpath == NULL) {
		g_warning ("Open with Explorer: cannot convert '%s'", path);
		return;
	}

	if (is_directory) {
		HINSTANCE res = ShellExecuteW (NULL, L"explore", wpath, NULL, NULL, SW_SHOWNORMAL);

		shell_execute_ok (res, "Open with Explorer", path);
		g_free (wpath);
		return;
	}

	/* An item id rather than a command line, so nothing about the name has to
	   survive being re-split. Needs COM on this thread; already-initialised
	   and a different apartment are both fine to carry on from. */
	item = ILCreateFromPathW (wpath);
	g_free (wpath);

	if (item == NULL) {
		explorer_select_by_command_line (path);
		return;
	}

	hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (SUCCEEDED (hr) || hr == RPC_E_CHANGED_MODE) {
		hr = SHOpenFolderAndSelectItems (item, 0, NULL, 0);
	}

	ILFree (item);

	if (FAILED (hr)) {
		explorer_select_by_command_line (path);
	}
}

#endif /* G_OS_WIN32 */
