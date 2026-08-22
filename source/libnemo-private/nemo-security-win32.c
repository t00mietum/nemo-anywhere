/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-security-win32.c - where a file's Windows permissions come from.

   Copyright (C) 2026 t00mietum.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
   Boston, MA 02110-1335, USA.
*/

#include <config.h>
#include <glib.h>

#ifdef G_OS_WIN32

#include "nemo-security-win32.h"

#include <windows.h>
#include <aclapi.h>

/* Which way the DACL's entries point: all carried down from a parent folder,
   all set on the file itself, or a mix of the two. That is the whole answer
   the Permissions source column shows, so nothing else is fetched. */
NemoWin32PermSource
nemo_security_win32_permissions_source (const char *path)
{
	wchar_t *wide_path;
	PACL dacl = NULL;
	PSECURITY_DESCRIPTOR descriptor = NULL;
	DWORD status;
	DWORD i;
	guint inherited_count = 0;
	guint local_count = 0;
	NemoWin32PermSource source;

	if (path == NULL) {
		return NEMO_WIN32_PERM_SOURCE_NONE;
	}

	wide_path = (wchar_t *) g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
	if (wide_path == NULL) {
		return NEMO_WIN32_PERM_SOURCE_NONE;
	}

	status = GetNamedSecurityInfoW (wide_path, SE_FILE_OBJECT,
					DACL_SECURITY_INFORMATION,
					NULL, NULL, &dacl, NULL, &descriptor);
	g_free (wide_path);

	if (status != ERROR_SUCCESS) {
		return NEMO_WIN32_PERM_SOURCE_NONE;
	}

	if (dacl != NULL) {
		for (i = 0; i < dacl->AceCount; i++) {
			ACE_HEADER *ace = NULL;

			if (!GetAce (dacl, i, (LPVOID *) &ace)) {
				continue;
			}

			if (ace->AceFlags & INHERITED_ACE) {
				inherited_count++;
			} else {
				local_count++;
			}
		}
	}

	if (inherited_count > 0 && local_count > 0) {
		source = NEMO_WIN32_PERM_SOURCE_MIXED;
	} else if (inherited_count > 0) {
		source = NEMO_WIN32_PERM_SOURCE_INHERITED;
	} else if (local_count > 0) {
		source = NEMO_WIN32_PERM_SOURCE_LOCAL;
	} else {
		/* A missing or empty DACL: nothing was inherited, and nothing was
		   set either - a FAT volume, mostly. Shown as blank. */
		source = NEMO_WIN32_PERM_SOURCE_NONE;
	}

	LocalFree (descriptor);

	return source;
}

#endif /* G_OS_WIN32 */
