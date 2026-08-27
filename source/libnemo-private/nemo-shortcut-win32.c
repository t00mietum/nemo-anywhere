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

#include <gio/gio.h>
#include <glib/gi18n.h>

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

gboolean
nemo_shortcut_win32_launch (const char  *lnk_path,
                            GError     **error)
{
	gunichar2 *w_lnk;
	HINSTANCE  res;

	g_return_val_if_fail (lnk_path != NULL, FALSE);

	w_lnk = to_utf16 (lnk_path);
	if (w_lnk == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_FAILED,
				     _("Could not encode the shortcut path."));
		return FALSE;
	}

	/* No verb: the default one, which is what a double-click uses. The shell
	   reads the .lnk itself, so the arguments, working directory, window state
	   and any run-as flag all come along. */
	res = ShellExecuteW (NULL, NULL, (LPCWSTR) w_lnk, NULL, NULL, SW_SHOWNORMAL);
	g_free (w_lnk);

	/* Anything at or below 32 is an error code, not a handle. */
	if ((INT_PTR) res > 32) {
		return TRUE;
	}

	g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
		     _("Could not open the shortcut (error %d)."),
		     (int) (INT_PTR) res);
	return FALSE;
}

#endif /* G_OS_WIN32 */
