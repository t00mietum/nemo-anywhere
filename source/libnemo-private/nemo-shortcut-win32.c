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

/* Older Windows headers do not carry the unprivileged flag. */
#ifndef SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE
#define SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE 0x2
#endif

/* The reparse point a junction is made of. The real declaration is in ntifs.h,
   a kernel header, so it gets spelled out here the way every user-mode program
   that makes one has to. Only the mount-point arm is needed. */
typedef struct {
	DWORD ReparseTag;
	WORD  ReparseDataLength;
	WORD  Reserved;
	WORD  SubstituteNameOffset;
	WORD  SubstituteNameLength;
	WORD  PrintNameOffset;
	WORD  PrintNameLength;
	WCHAR PathBuffer[1];
} MountPointReparseBuffer;

/* Through PrintNameLength - what precedes the two names in the buffer. */
#define REPARSE_HEADER_BYTES 16

/* A junction can only point at a fully qualified local path - not a relative
   name and not a share. Returns the target spelled the way the reparse point
   wants it, or NULL when a junction is not an option. */
static char *
junction_target (const char *target_path)
{
	char *native;
	char *walk;
	gsize len;

	if (target_path == NULL || !g_ascii_isalpha (target_path[0]) || target_path[1] != ':') {
		return NULL;
	}

	native = g_strdup (target_path);
	for (walk = native; *walk != '\0'; walk++) {
		if (*walk == '/') {
			*walk = '\\';
		}
	}

	/* A trailing separator makes the mount point resolve oddly. */
	len = strlen (native);
	while (len > 3 && native[len - 1] == '\\') {
		native[--len] = '\0';
	}

	return native;
}

/* Make link_path a junction pointing at target_path. Junctions carry no
   privilege requirement, which is the whole reason to prefer one: a folder link
   works with Developer Mode off and without running elevated. */
static gboolean
try_junction (const char *target_path,
              const char *link_path,
              DWORD      *win_error)
{
	char *native = junction_target (target_path);
	char *prefixed;
	gunichar2 *substitute, *print_name, *w_link;
	MountPointReparseBuffer *buffer;
	gsize substitute_bytes, print_bytes, total;
	HANDLE handle;
	DWORD returned = 0;
	gboolean ok;

	*win_error = ERROR_NOT_SUPPORTED;
	if (native == NULL) {
		return FALSE;
	}

	prefixed = g_strconcat ("\\??\\", native, NULL);
	substitute = to_utf16 (prefixed);
	print_name = to_utf16 (native);
	w_link = to_utf16 (link_path);
	g_free (prefixed);
	g_free (native);

	if (substitute == NULL || print_name == NULL || w_link == NULL) {
		g_free (substitute);
		g_free (print_name);
		g_free (w_link);
		return FALSE;
	}

	if (!CreateDirectoryW ((LPCWSTR) w_link, NULL)) {
		*win_error = GetLastError ();
		g_free (substitute);
		g_free (print_name);
		g_free (w_link);
		return FALSE;
	}

	handle = CreateFileW ((LPCWSTR) w_link, GENERIC_WRITE, 0, NULL, OPEN_EXISTING,
	                      FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, NULL);
	if (handle == INVALID_HANDLE_VALUE) {
		*win_error = GetLastError ();
		RemoveDirectoryW ((LPCWSTR) w_link);
		g_free (substitute);
		g_free (print_name);
		g_free (w_link);
		return FALSE;
	}

	/* Both names sit in one buffer, each with a terminator of its own. */
	substitute_bytes = wcslen ((const wchar_t *) substitute) * sizeof (WCHAR);
	print_bytes = wcslen ((const wchar_t *) print_name) * sizeof (WCHAR);
	total = REPARSE_HEADER_BYTES + substitute_bytes + print_bytes + 2 * sizeof (WCHAR);

	buffer = g_malloc0 (total);
	buffer->ReparseTag = IO_REPARSE_TAG_MOUNT_POINT;
	buffer->ReparseDataLength = (WORD) (total - 8);
	buffer->SubstituteNameOffset = 0;
	buffer->SubstituteNameLength = (WORD) substitute_bytes;
	buffer->PrintNameOffset = (WORD) (substitute_bytes + sizeof (WCHAR));
	buffer->PrintNameLength = (WORD) print_bytes;
	wcscpy (buffer->PathBuffer, (const wchar_t *) substitute);
	wcscpy (buffer->PathBuffer + wcslen ((const wchar_t *) substitute) + 1,
	        (const wchar_t *) print_name);

	SetLastError (0);
	ok = DeviceIoControl (handle, FSCTL_SET_REPARSE_POINT, buffer, (DWORD) total,
	                      NULL, 0, &returned, NULL) != 0;
	*win_error = ok ? 0 : GetLastError ();

	CloseHandle (handle);
	if (!ok) {
		RemoveDirectoryW ((LPCWSTR) w_link);
	}

	g_free (buffer);
	g_free (substitute);
	g_free (print_name);
	g_free (w_link);

	return ok;
}

static gboolean
try_symlink (const char *target_path,
             const char *link_path,
             DWORD       extra_flags,
             DWORD      *win_error)
{
	gunichar2 *w_target = to_utf16 (target_path);
	gunichar2 *w_link = to_utf16 (link_path);
	DWORD flags = extra_flags;
	gboolean ok;

	if (g_file_test (target_path, G_FILE_TEST_IS_DIR)) {
		flags |= SYMBOLIC_LINK_FLAG_DIRECTORY;
	}

	SetLastError (0);
	ok = CreateSymbolicLinkW ((LPCWSTR) w_link, (LPCWSTR) w_target, flags) != 0;
	*win_error = ok ? 0 : GetLastError ();

	g_free (w_target);
	g_free (w_link);

	return ok;
}

gboolean
nemo_shortcut_win32_create_symlink (const char  *target_path,
                                    const char  *link_path,
                                    GError     **error)
{
	DWORD win_error = 0;

	/* A folder gets a junction wherever one will do. It needs no privilege, so
	 * a folder link works on a machine where a symlink is refused outright, and
	 * nothing downstream can tell the two apart. */
	if (g_file_test (target_path, G_FILE_TEST_IS_DIR)) {
		if (try_junction (target_path, link_path, &win_error)) {
			return TRUE;
		}
		if (win_error == ERROR_ALREADY_EXISTS || win_error == ERROR_FILE_EXISTS) {
			g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_EXISTS,
					     _("A file with that name already exists."));
			return FALSE;
		}
	}

	/* The unprivileged flag is what makes this work under Developer Mode.
	 * Windows before 1703 refuses the flag itself rather than the call, so a
	 * second attempt without it covers those. */
	if (try_symlink (target_path, link_path, SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE, &win_error)) {
		return TRUE;
	}

	if (win_error == ERROR_INVALID_PARAMETER &&
	    try_symlink (target_path, link_path, 0, &win_error)) {
		return TRUE;
	}

	if (win_error == ERROR_PRIVILEGE_NOT_HELD) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_PERMISSION_DENIED,
				     _("Windows allows symlinks only with Developer Mode turned on, or when running as administrator."));
	} else if (win_error == ERROR_ALREADY_EXISTS || win_error == ERROR_FILE_EXISTS) {
		/* Spelled as EXISTS so the caller can uniquify the name and try again,
		   the way the POSIX path does. */
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_EXISTS,
				     _("A file with that name already exists."));
	} else {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED,
			     _("Could not create the symlink (error %lu)."),
			     (unsigned long) win_error);
	}

	return FALSE;
}

gboolean
nemo_shortcut_win32_symlinks_allowed (void)
{
	static gsize answer = 0;   /* 1 no, 2 yes */

	if (g_once_init_enter (&answer)) {
		gsize allowed = 1;
		char *dir = g_dir_make_tmp ("nemo-symlink-check-XXXXXX", NULL);

		/* Asking Windows whether the privilege is held means reading a token
		 * and a registry key and getting both right; making one and throwing
		 * it away answers the same question with no room for doubt. */
		if (dir != NULL) {
			char *target = g_build_filename (dir, "target", NULL);
			char *link = g_build_filename (dir, "link", NULL);

			if (g_file_set_contents (target, "", 0, NULL) &&
			    nemo_shortcut_win32_create_symlink (target, link, NULL)) {
				allowed = 2;
			}

			g_remove (link);
			g_remove (target);
			g_rmdir (dir);
			g_free (target);
			g_free (link);
			g_free (dir);
		}

		g_once_init_leave (&answer, allowed);
	}

	return answer == 2;
}

#endif /* G_OS_WIN32 */
