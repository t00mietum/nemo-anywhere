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

	IShellLinkW_SetPath (link, w_target);

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

#endif /* G_OS_WIN32 */
