/* nemo-shortcut-win32.c - create Windows .lnk shell shortcuts via COM.
 *
 * Copyright © 2026 Bubbles
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by the
 * Free Software Foundation.
 */

#include <config.h>
#include "nemo-shortcut-win32.h"

#ifdef G_OS_WIN32

#include "nemo-launch-win32.h"

#include <string.h>
#include <gio/gio.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <objidl.h>

/* CoInitialize on this thread; returns TRUE if the caller must CoUninitialize. */
static gboolean
com_init (void)
{
	HRESULT hr = CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);
	if (hr == RPC_E_CHANGED_MODE) {
		/* thread already initialised in another mode - shell link still
		 * works, just don't unbalance the ref count */
		return FALSE;
	}
	return TRUE;
}

static gunichar2 *
to_utf16 (const char *s)
{
	if (s == NULL) {
		return NULL;
	}
	return g_utf8_to_utf16 (s, -1, NULL, NULL, NULL);
}

gboolean
nemo_shortcut_win32_create (const char  *target_path,
                            const char  *lnk_path,
                            const char  *working_dir,
                            const char  *arguments,
                            const char  *description,
                            GError     **error)
{
	IShellLinkW *link = NULL;
	IPersistFile *pf = NULL;
	gunichar2 *w_target = NULL, *w_lnk = NULL, *w_dir = NULL, *w_args = NULL, *w_desc = NULL;
	gboolean did_init = FALSE;
	gboolean ok = FALSE;
	HRESULT hr;

	g_return_val_if_fail (target_path != NULL, FALSE);
	g_return_val_if_fail (lnk_path != NULL, FALSE);

	/* IPersistFile::Save has no create-new mode, so without this it silently
	 * writes over whatever is already there. Report the clash and let the caller
	 * uniquify ("another link to ...") the way the symlink path does. The check
	 * is racy - the shell offers nothing atomic - but the race is a rare loss
	 * against an unconditional one. */
	if (g_file_test (lnk_path, G_FILE_TEST_EXISTS)) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_EXISTS,
				     _("A file with that name already exists."));
		goto out;
	}

	w_target = to_utf16 (target_path);
	w_lnk    = to_utf16 (lnk_path);
	if (w_target == NULL || w_lnk == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not encode the shortcut path."));
		goto out;
	}

	did_init = com_init ();

	hr = CoCreateInstance (&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
			       &IID_IShellLinkW, (void **) &link);
	if (FAILED (hr) || link == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not create the shortcut object."));
		goto uninit;
	}

	/* SetPath is MAX_PATH-bound and refuses a longer target outright. Ignoring
	 * that saved a shortcut with no target at all and called it a success, so
	 * "Make Link" produced a dead link with nothing said. */
	hr = IShellLinkW_SetPath (link, w_target);
	if (FAILED (hr)) {
		if (wcslen ((const wchar_t *) w_target) >= MAX_PATH) {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME,
				     _("A Windows shortcut cannot point at \"%s\": the path is too long."),
				     target_path);
		} else {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME,
				     _("A Windows shortcut cannot point at \"%s\"."),
				     target_path);
		}
		goto release;
	}

	if (working_dir != NULL && (w_dir = to_utf16 (working_dir)) != NULL) {
		IShellLinkW_SetWorkingDirectory (link, w_dir);
	}
	if (arguments != NULL && (w_args = to_utf16 (arguments)) != NULL) {
		IShellLinkW_SetArguments (link, w_args);
	}
	if (description != NULL && (w_desc = to_utf16 (description)) != NULL) {
		IShellLinkW_SetDescription (link, w_desc);
	}

	hr = IShellLinkW_QueryInterface (link, &IID_IPersistFile, (void **) &pf);
	if (FAILED (hr) || pf == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not access the shortcut file interface."));
		goto release;
	}

	hr = IPersistFile_Save (pf, w_lnk, TRUE);
	if (FAILED (hr)) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not save the shortcut."));
		goto release;
	}

	ok = TRUE;

release:
	if (pf != NULL) {
		IPersistFile_Release (pf);
	}
	if (link != NULL) {
		IShellLinkW_Release (link);
	}
uninit:
	if (did_init) {
		CoUninitialize ();
	}
out:
	g_free (w_target);
	g_free (w_lnk);
	g_free (w_dir);
	g_free (w_args);
	g_free (w_desc);
	return ok;
}

gboolean
nemo_shortcut_win32_read (const char  *lnk_path,
                          char       **target_path,
                          GError     **error)
{
	IShellLinkW *link = NULL;
	IPersistFile *pf = NULL;
	gunichar2 *w_lnk = NULL;
	/* Long-path sized: at MAX_PATH a longer target was silently cut short and
	   then opened, which is a different file. */
	wchar_t buf[32768];
	gboolean did_init = FALSE;
	gboolean ok = FALSE;
	HRESULT hr;

	g_return_val_if_fail (lnk_path != NULL, FALSE);
	g_return_val_if_fail (target_path != NULL, FALSE);

	*target_path = NULL;

	w_lnk = to_utf16 (lnk_path);
	if (w_lnk == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not encode the shortcut path."));
		goto out;
	}

	did_init = com_init ();

	hr = CoCreateInstance (&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
			       &IID_IShellLinkW, (void **) &link);
	if (FAILED (hr) || link == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not create the shortcut object."));
		goto uninit;
	}

	hr = IShellLinkW_QueryInterface (link, &IID_IPersistFile, (void **) &pf);
	if (FAILED (hr) || pf == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not access the shortcut file interface."));
		goto release;
	}

	hr = IPersistFile_Load (pf, w_lnk, STGM_READ);
	if (FAILED (hr)) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not load the shortcut."));
		goto release;
	}

	buf[0] = L'\0';
	hr = IShellLinkW_GetPath (link, buf, G_N_ELEMENTS (buf), NULL, 0);
	if (FAILED (hr) || buf[0] == L'\0') {
		/* No file-system target - e.g. a shortcut to a virtual item that
		 * stores only an ID list. Nothing to follow. */
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND,
				     _("The shortcut has no file target."));
		goto release;
	}

	*target_path = g_utf16_to_utf8 ((const gunichar2 *) buf, -1, NULL, NULL, NULL);
	ok = (*target_path != NULL);
	if (!ok) {
		/* Every other failure here leaves an error behind; this one did not. */
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_INVALID_DATA,
				     _("The shortcut's target could not be read."));
	}

release:
	if (pf != NULL) {
		IPersistFile_Release (pf);
	}
	if (link != NULL) {
		IShellLinkW_Release (link);
	}
uninit:
	if (did_init) {
		CoUninitialize ();
	}
out:
	g_free (w_lnk);
	return ok;
}

/* Opens a shortcut for reading or rewriting; the caller releases both. */
static gboolean
load_link (const char    *lnk_path,
           DWORD          mode,
           IShellLinkW  **link,
           IPersistFile **pf,
           GError       **error)
{
	gunichar2 *w_lnk = to_utf16 (lnk_path);
	HRESULT hr;

	*link = NULL;
	*pf = NULL;

	if (w_lnk == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not encode the shortcut path."));
		return FALSE;
	}

	hr = CoCreateInstance (&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
			       &IID_IShellLinkW, (void **) link);
	if (SUCCEEDED (hr) && *link != NULL) {
		hr = IShellLinkW_QueryInterface (*link, &IID_IPersistFile, (void **) pf);
	}
	if (SUCCEEDED (hr) && *pf != NULL) {
		hr = IPersistFile_Load (*pf, w_lnk, mode);
	}

	g_free (w_lnk);

	if (FAILED (hr) || *pf == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not load the shortcut."));
		if (*pf != NULL) {
			IPersistFile_Release (*pf);
		}
		if (*link != NULL) {
			IShellLinkW_Release (*link);
		}
		*link = NULL;
		*pf = NULL;
		return FALSE;
	}

	return TRUE;
}

static char *
from_wide (const wchar_t *s)
{
	return g_utf16_to_utf8 ((const gunichar2 *) s, -1, NULL, NULL, NULL);
}

void
nemo_shortcut_info_clear (NemoShortcutInfo *info)
{
	g_clear_pointer (&info->target, g_free);
	g_clear_pointer (&info->arguments, g_free);
	g_clear_pointer (&info->working_dir, g_free);
	g_clear_pointer (&info->description, g_free);
}

/* Unlike nemo_shortcut_win32_read, a shortcut with no file target is not an
 * error here: the editor still has to open on it. Every field comes back set,
 * empty where the shortcut says nothing. */
gboolean
nemo_shortcut_win32_read_info (const char        *lnk_path,
                               NemoShortcutInfo  *info,
                               GError           **error)
{
	IShellLinkW *link;
	IPersistFile *pf;
	wchar_t buf[32768];
	gboolean did_init;

	g_return_val_if_fail (lnk_path != NULL, FALSE);
	g_return_val_if_fail (info != NULL, FALSE);

	memset (info, 0, sizeof *info);

	did_init = com_init ();

	if (!load_link (lnk_path, STGM_READ, &link, &pf, error)) {
		if (did_init) {
			CoUninitialize ();
		}
		return FALSE;
	}

	buf[0] = L'\0';
	IShellLinkW_GetPath (link, buf, G_N_ELEMENTS (buf), NULL, 0);
	info->target = from_wide (buf);

	buf[0] = L'\0';
	IShellLinkW_GetArguments (link, buf, G_N_ELEMENTS (buf));
	info->arguments = from_wide (buf);

	buf[0] = L'\0';
	IShellLinkW_GetWorkingDirectory (link, buf, G_N_ELEMENTS (buf));
	info->working_dir = from_wide (buf);

	buf[0] = L'\0';
	IShellLinkW_GetDescription (link, buf, G_N_ELEMENTS (buf));
	info->description = from_wide (buf);

	IPersistFile_Release (pf);
	IShellLinkW_Release (link);

	if (did_init) {
		CoUninitialize ();
	}

	return TRUE;
}

/* How many shortcut answers to keep before starting over. */
#define TARGET_CACHE_LIMIT 2000

/* Whether the shortcut points at a folder, read from the attributes the .lnk
 * stores rather than from the target itself - a shortcut to a share that is not
 * answering costs about twenty seconds, and this is asked from the draw path.
 * Kept for the same reason: it is a COM load and a file read each time. */
gboolean
nemo_shortcut_win32_target_is_dir (const char *lnk_path,
                                   gint64      mtime)
{
	static GHashTable *cache = NULL;
	IShellLinkW *link;
	IPersistFile *pf;
	WIN32_FIND_DATAW found;
	wchar_t buf[MAX_PATH];
	gboolean did_init;
	gboolean is_dir = FALSE;
	char *key;
	gpointer cached;

	g_return_val_if_fail (lnk_path != NULL, FALSE);

	if (cache == NULL) {
		cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	} else if (g_hash_table_size (cache) >= TARGET_CACHE_LIMIT) {
		g_hash_table_remove_all (cache);
	}

	key = g_strdup_printf ("%s|%lld", lnk_path, (long long) mtime);

	if (g_hash_table_lookup_extended (cache, key, NULL, &cached)) {
		g_free (key);
		return GPOINTER_TO_INT (cached) != 0;
	}

	did_init = com_init ();

	if (load_link (lnk_path, STGM_READ, &link, &pf, NULL)) {
		memset (&found, 0, sizeof found);
		buf[0] = L'\0';

		if (SUCCEEDED (IShellLinkW_GetPath (link, buf, G_N_ELEMENTS (buf),
						    &found, SLGP_RAWPATH))) {
			is_dir = (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
		}

		IPersistFile_Release (pf);
		IShellLinkW_Release (link);
	}

	if (did_init) {
		CoUninitialize ();
	}

	g_hash_table_insert (cache, key, GINT_TO_POINTER (is_dir ? 1 : 0));

	return is_dir;
}

/* Rewrites the shortcut in place with every field from info. */
gboolean
nemo_shortcut_win32_update (const char             *lnk_path,
                            const NemoShortcutInfo *info,
                            GError                **error)
{
	IShellLinkW *link;
	IPersistFile *pf;
	gunichar2 *w_target, *w_args, *w_dir, *w_desc, *w_lnk;
	gboolean did_init;
	gboolean ok = FALSE;
	HRESULT hr;

	g_return_val_if_fail (lnk_path != NULL, FALSE);
	g_return_val_if_fail (info != NULL, FALSE);

	did_init = com_init ();

	if (!load_link (lnk_path, STGM_READWRITE, &link, &pf, error)) {
		if (did_init) {
			CoUninitialize ();
		}
		return FALSE;
	}

	w_target = to_utf16 (info->target != NULL ? info->target : "");
	w_args   = to_utf16 (info->arguments != NULL ? info->arguments : "");
	w_dir    = to_utf16 (info->working_dir != NULL ? info->working_dir : "");
	w_desc   = to_utf16 (info->description != NULL ? info->description : "");
	w_lnk    = to_utf16 (lnk_path);

	hr = IShellLinkW_SetPath (link, w_target);
	if (FAILED (hr)) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_INVALID_FILENAME,
			     _("A Windows shortcut cannot point at \"%s\"."), info->target);
		goto out;
	}

	IShellLinkW_SetArguments (link, w_args);
	IShellLinkW_SetWorkingDirectory (link, w_dir);
	IShellLinkW_SetDescription (link, w_desc);

	hr = IPersistFile_Save (pf, w_lnk, TRUE);
	if (FAILED (hr)) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not save the shortcut."));
		goto out;
	}

	ok = TRUE;

out:
	g_free (w_target);
	g_free (w_args);
	g_free (w_dir);
	g_free (w_desc);
	g_free (w_lnk);
	IPersistFile_Release (pf);
	IShellLinkW_Release (link);

	if (did_init) {
		CoUninitialize ();
	}

	return ok;
}

gboolean
nemo_shortcut_win32_launch (const char  *lnk_path,
                            GError     **error)
{
	g_return_val_if_fail (lnk_path != NULL, FALSE);

	/* No verb: the default one, which is what a double-click uses. The shell
	   reads the .lnk itself, so the arguments, working directory, window state
	   and any run-as flag all come along. */
	return nemo_launch_win32_open_path (lnk_path, NULL, error);
}

#endif /* G_OS_WIN32 */
