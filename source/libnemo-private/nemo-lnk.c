/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-lnk.c - read and write Windows .lnk shortcuts without Windows.

   Copyright © 2026 t00mietum (CryptogID: ปʬϝღถɔ4რఠΔթะ9ƾǝu).

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

/* The file layout is Microsoft's [MS-SHLLINK]. Only the parts that say where
 * the target is are read: the header, the LinkInfo block and the strings after
 * it. The item id list is skipped. Shortcuts to files and folders always carry
 * LinkInfo as well, and the id list alone mostly names things like Control
 * Panel pages that mean nothing here. */

#include <config.h>

#include "nemo-lnk.h"
#include "nemo-dir-enum.h"
#include "nemo-link-copy.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <glib/gi18n.h>
#include <glib/gstdio.h>

/* Real shortcuts are a few KiB. Anything past this is extra data blocks, which
   nothing here reads. */
#define LNK_READ_MAX    (64 * 1024)
#define LNK_HEADER_SIZE 0x4C
#define LNK_MAX_HOPS    8
#define ICON_CACHE_MAX  2000

#define FLAG_HAS_ID_LIST        0x0001
#define FLAG_HAS_LINK_INFO      0x0002
#define FLAG_HAS_NAME           0x0004
#define FLAG_HAS_RELATIVE_PATH  0x0008
#define FLAG_HAS_WORKING_DIR    0x0010
#define FLAG_HAS_ARGUMENTS      0x0020
#define FLAG_IS_UNICODE         0x0080
#define FLAG_FORCE_NO_LINK_INFO 0x0100

#define INFO_VOLUME_AND_PATH 0x1
#define INFO_NETWORK         0x2

#define ATTRIBUTE_DIRECTORY 0x10
#define ATTRIBUTE_ARCHIVE   0x20

#define SW_SHOWNORMAL        1
#define NET_TYPE_VALID       0x2
#define NET_PROVIDER_LANMAN  0x00020000

/* Seconds from 1601, where a Windows file time starts, to 1970. */
#define FILETIME_UNIX_OFFSET G_GINT64_CONSTANT (11644473600)

static const guint8 lnk_clsid[16] = {
	0x01, 0x14, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00,
	0xc0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46
};

static char *mountinfo_path;
static char *by_uuid_path;
static char *gvfs_path;

static guint16
get_u16 (const guint8 *p)
{
	return (guint16) (p[0] | (p[1] << 8));
}

static guint32
get_u32 (const guint8 *p)
{
	return (guint32) p[0] | ((guint32) p[1] << 8) |
	       ((guint32) p[2] << 16) | ((guint32) p[3] << 24);
}

/* Code page text. The file does not say which code page wrote it. 1252 is
   right for most, and Latin-1 cannot fail, so there is always an answer. */
static char *
ansi_to_utf8 (const guint8 *text, gsize length)
{
	char *utf8;

	if (g_utf8_validate ((const char *) text, length, NULL)) {
		return g_strndup ((const char *) text, length);
	}

	utf8 = g_convert ((const char *) text, length, "UTF-8", "WINDOWS-1252", NULL, NULL, NULL);
	if (utf8 == NULL) {
		utf8 = g_convert ((const char *) text, length, "UTF-8", "ISO-8859-1", NULL, NULL, NULL);
	}

	return utf8;
}

static char *
utf16_to_utf8 (const guint8 *text, gsize units)
{
	gunichar2 *wide;
	char *utf8;
	gsize i;

	wide = g_new (gunichar2, units + 1);
	for (i = 0; i < units; i++) {
		wide[i] = get_u16 (text + 2 * i);
	}
	utf8 = g_utf16_to_utf8 (wide, units, NULL, NULL, NULL);
	g_free (wide);

	return utf8;
}

/* A NUL-terminated string at offset inside a block of size bytes. */
static char *
ansi_in_block (const guint8 *block, gsize size, guint32 offset)
{
	const guint8 *start, *end;

	if (offset == 0 || offset >= size) {
		return NULL;
	}

	start = block + offset;
	end = memchr (start, 0, size - offset);
	if (end == NULL) {
		return NULL;
	}

	return ansi_to_utf8 (start, end - start);
}

static char *
utf16_in_block (const guint8 *block, gsize size, guint32 offset)
{
	gsize units = 0;

	if (offset == 0 || offset >= size) {
		return NULL;
	}

	while (offset + 2 * units + 1 < size) {
		if (get_u16 (block + offset + 2 * units) == 0) {
			return utf16_to_utf8 (block + offset, units);
		}
		units++;
	}

	return NULL;
}

static char *
join_windows (const char *base, const char *rest)
{
	if (rest == NULL || rest[0] == '\0') {
		return g_strdup (base);
	}
	if (g_str_has_suffix (base, "\\")) {
		return g_strconcat (base, rest, NULL);
	}

	return g_strconcat (base, "\\", rest, NULL);
}

static void
parse_link_info (const guint8 *info, gsize size, NemoLnk *lnk)
{
	guint32 flags, base_unicode = 0, suffix_unicode = 0;
	char *suffix = NULL;

	if (size < 0x1c) {
		return;
	}

	flags = get_u32 (info + 8);
	if (get_u32 (info + 4) >= 0x24 && size >= 0x24) {
		base_unicode = get_u32 (info + 28);
		suffix_unicode = get_u32 (info + 32);
	}

	/* The UTF-16 copies are there only when the ANSI ones lost something,
	   so they win when present. */
	suffix = utf16_in_block (info, size, suffix_unicode);
	if (suffix == NULL) {
		suffix = ansi_in_block (info, size, get_u32 (info + 24));
	}

	if (flags & INFO_VOLUME_AND_PATH) {
		guint32 volume = get_u32 (info + 12);
		char *base;

		if (volume != 0 && volume <= size - 16) {
			lnk->drive_serial = get_u32 (info + volume + 8);
			lnk->has_serial = TRUE;
		}

		base = utf16_in_block (info, size, base_unicode);
		if (base == NULL) {
			base = ansi_in_block (info, size, get_u32 (info + 16));
		}
		if (base != NULL && base[0] != '\0') {
			lnk->local_path = join_windows (base, suffix);
		}
		g_free (base);
	}

	if (flags & INFO_NETWORK) {
		guint32 net = get_u32 (info + 20);

		if (net != 0 && net <= size - 20) {
			const guint8 *block = info + net;
			gsize block_size = MIN ((gsize) get_u32 (block), size - net);
			guint32 name_offset = get_u32 (block + 8);
			char *share = NULL;

			if (name_offset > 0x14 && block_size >= 0x1c) {
				share = utf16_in_block (block, block_size, get_u32 (block + 20));
			}
			if (share == NULL) {
				share = ansi_in_block (block, block_size, name_offset);
			}

			if (share != NULL && share[0] != '\0') {
				lnk->net_share = share;
				lnk->net_path = g_strdup (suffix != NULL ? suffix : "");
			} else {
				g_free (share);
			}
		}
	}

	g_free (suffix);
}

static gboolean
read_string (const guint8 *bytes, gsize length, gsize *pos,
	     gboolean unicode, char **out)
{
	gsize count, need;

	if (*pos + 2 > length) {
		return FALSE;
	}

	count = get_u16 (bytes + *pos);
	*pos += 2;
	need = unicode ? 2 * count : count;
	if (need > length - *pos) {
		return FALSE;
	}

	*out = unicode ? utf16_to_utf8 (bytes + *pos, count)
		       : ansi_to_utf8 (bytes + *pos, count);
	*pos += need;

	return TRUE;
}

gboolean
nemo_lnk_parse (const guint8 *bytes, gsize length, NemoLnk *lnk)
{
	guint32 flags;
	gboolean unicode;
	gsize pos;

	memset (lnk, 0, sizeof *lnk);

	if (length < LNK_HEADER_SIZE ||
	    get_u32 (bytes) != LNK_HEADER_SIZE ||
	    memcmp (bytes + 4, lnk_clsid, sizeof lnk_clsid) != 0) {
		return FALSE;
	}

	flags = get_u32 (bytes + 20);
	lnk->attributes = get_u32 (bytes + 24);
	pos = LNK_HEADER_SIZE;

	if (flags & FLAG_HAS_ID_LIST) {
		if (pos + 2 > length) {
			return FALSE;
		}
		pos += 2 + get_u16 (bytes + pos);
		if (pos > length) {
			return FALSE;
		}
	}

	if (flags & FLAG_HAS_LINK_INFO) {
		guint32 size;

		if (pos + 4 > length) {
			return FALSE;
		}
		size = get_u32 (bytes + pos);
		if (size < 4 || size > length - pos) {
			return FALSE;
		}
		if (!(flags & FLAG_FORCE_NO_LINK_INFO)) {
			parse_link_info (bytes + pos, size, lnk);
		}
		pos += size;
	}

	/* The strings are extras. A file cut short in them still has its target. */
	unicode = (flags & FLAG_IS_UNICODE) != 0;
	if ((flags & FLAG_HAS_NAME) &&
	    !read_string (bytes, length, &pos, unicode, &lnk->description)) {
		return TRUE;
	}
	if ((flags & FLAG_HAS_RELATIVE_PATH) &&
	    !read_string (bytes, length, &pos, unicode, &lnk->relative_path)) {
		return TRUE;
	}
	if ((flags & FLAG_HAS_WORKING_DIR) &&
	    !read_string (bytes, length, &pos, unicode, &lnk->working_dir)) {
		return TRUE;
	}
	if ((flags & FLAG_HAS_ARGUMENTS) &&
	    !read_string (bytes, length, &pos, unicode, &lnk->arguments)) {
		return TRUE;
	}

	return TRUE;
}

gboolean
nemo_lnk_read (const char *lnk_path, NemoLnk *lnk)
{
	guint8 *buffer;
	gsize got;
	gboolean ok;
	FILE *fp;

	memset (lnk, 0, sizeof *lnk);

	fp = g_fopen (lnk_path, "rb");
	if (fp == NULL) {
		return FALSE;
	}

	buffer = g_malloc (LNK_READ_MAX);
	got = fread (buffer, 1, LNK_READ_MAX, fp);
	fclose (fp);

	ok = nemo_lnk_parse (buffer, got, lnk);
	g_free (buffer);

	return ok;
}

void
nemo_lnk_clear (NemoLnk *lnk)
{
	g_free (lnk->local_path);
	g_free (lnk->net_share);
	g_free (lnk->net_path);
	g_free (lnk->relative_path);
	g_free (lnk->working_dir);
	g_free (lnk->arguments);
	g_free (lnk->description);
	memset (lnk, 0, sizeof *lnk);
}

gboolean
nemo_lnk_is_dir (const NemoLnk *lnk)
{
	return (lnk->attributes & ATTRIBUTE_DIRECTORY) != 0;
}

char *
nemo_lnk_display_target (const NemoLnk *lnk)
{
	if (lnk->local_path != NULL) {
		return g_strdup (lnk->local_path);
	}
	if (lnk->net_share != NULL) {
		return join_windows (lnk->net_share, lnk->net_path);
	}

	return g_strdup (lnk->relative_path);
}

void
nemo_lnk_set_system_paths (const char *mountinfo,
			   const char *by_uuid_dir,
			   const char *gvfs_dir)
{
	g_free (mountinfo_path);
	g_free (by_uuid_path);
	g_free (gvfs_path);
	mountinfo_path = g_strdup (mountinfo);
	by_uuid_path = g_strdup (by_uuid_dir);
	gvfs_path = g_strdup (gvfs_dir);
}

typedef struct {
	char *root;
	char *mount_point;
	char *fstype;
	char *source;
} MountEntry;

static void
mount_entry_free (gpointer data)
{
	MountEntry *entry = data;

	g_free (entry->root);
	g_free (entry->mount_point);
	g_free (entry->fstype);
	g_free (entry->source);
	g_free (entry);
}

/* mountinfo writes a space in a name as \040. */
static char *
unescape_mount_field (const char *field)
{
	GString *out = g_string_new (NULL);
	const char *p;

	for (p = field; *p != '\0'; p++) {
		if (p[0] == '\\' &&
		    p[1] >= '0' && p[1] <= '3' &&
		    p[2] >= '0' && p[2] <= '7' &&
		    p[3] >= '0' && p[3] <= '7') {
			g_string_append_c (out, (char) (((p[1] - '0') << 6) |
							((p[2] - '0') << 3) |
							(p[3] - '0')));
			p += 3;
		} else {
			g_string_append_c (out, *p);
		}
	}

	return g_string_free (out, FALSE);
}

static GPtrArray *
read_mounts (void)
{
	GPtrArray *mounts = g_ptr_array_new_with_free_func (mount_entry_free);
	char *text = NULL;
	char **lines;
	int i;

	if (!g_file_get_contents (mountinfo_path != NULL ? mountinfo_path : "/proc/self/mountinfo",
				  &text, NULL, NULL)) {
		return mounts;
	}

	lines = g_strsplit (text, "\n", -1);
	for (i = 0; lines[i] != NULL; i++) {
		char **fields = g_strsplit (lines[i], " ", -1);
		int count = g_strv_length (fields);
		int sep;

		/* Optional fields sit between the mount options and a lone dash. */
		for (sep = 6; sep < count && strcmp (fields[sep], "-") != 0; sep++);

		if (sep + 2 < count) {
			MountEntry *entry = g_new0 (MountEntry, 1);

			entry->root = unescape_mount_field (fields[3]);
			entry->mount_point = unescape_mount_field (fields[4]);
			entry->fstype = g_strdup (fields[sep + 1]);
			entry->source = unescape_mount_field (fields[sep + 2]);
			g_ptr_array_add (mounts, entry);
		}
		g_strfreev (fields);
	}
	g_strfreev (lines);
	g_free (text);

	return mounts;
}

/* Casefolded, since Windows names do not keep case. */
static gboolean
same_name (const char *a, const char *b)
{
	char *fa = g_utf8_casefold (a, -1);
	char *fb = g_utf8_casefold (b, -1);
	gboolean same = strcmp (fa, fb) == 0;

	g_free (fa);
	g_free (fb);

	return same;
}

static GFileEnumerator *
open_dir (const char *dir)
{
	GFile *file = g_file_new_for_path (dir);
	GFileEnumerator *children;

	children = nemo_enumerate_children (file, G_FILE_ATTRIBUTE_STANDARD_NAME,
					    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
	g_object_unref (file);

	return children;
}

/* The one entry in dir named name when case is ignored. Two of them on a file
   system that keeps case is a question this cannot answer. */
static char *
find_ignoring_case (const char *dir, const char *name)
{
	GFileEnumerator *children = open_dir (dir);
	GFileInfo *info;
	char *found = NULL;
	int matches = 0;

	if (children == NULL) {
		return NULL;
	}

	while ((info = g_file_enumerator_next_file (children, NULL, NULL)) != NULL) {
		const char *child = g_file_info_get_name (info);

		if (same_name (child, name)) {
			matches++;
			g_free (found);
			found = g_build_filename (dir, child, NULL);
		}
		g_object_unref (info);
	}
	g_object_unref (children);

	if (matches != 1) {
		g_clear_pointer (&found, g_free);
	}

	return found;
}

/* Walk a Windows path down from base, one name at a time, each one taken as
   spelled or else by case. NULL as soon as a step is not there. */
static char *
walk_path (const char *base, const char *windows_rest)
{
	char **parts = g_strsplit_set (windows_rest != NULL ? windows_rest : "", "\\/", -1);
	char *current = g_strdup (base);
	int i;

	for (i = 0; parts[i] != NULL && current != NULL; i++) {
		char *next;

		if (parts[i][0] == '\0' || strcmp (parts[i], ".") == 0) {
			continue;
		}
		if (strcmp (parts[i], "..") == 0) {
			next = g_path_get_dirname (current);
		} else {
			next = g_build_filename (current, parts[i], NULL);
			if (!g_file_test (next, G_FILE_TEST_EXISTS)) {
				g_free (next);
				next = find_ignoring_case (current, parts[i]);
			}
		}
		g_free (current);
		current = next;
	}
	g_strfreev (parts);

	if (current != NULL && !g_file_test (current, G_FILE_TEST_EXISTS)) {
		g_clear_pointer (&current, g_free);
	}

	return current;
}

static gboolean
is_hex (const char *text, gsize length)
{
	gsize i;

	for (i = 0; i < length; i++) {
		if (!g_ascii_isxdigit (text[i])) {
			return FALSE;
		}
	}

	return TRUE;
}

/* The volume serial Windows keeps for a drive, from the name the system gives
   its volume id: sixteen hex digits for NTFS, whose low half is the serial,
   and XXXX-XXXX for FAT and exFAT. Anything else is some other file system. */
static gboolean
serial_from_uuid (const char *name, guint32 *serial)
{
	gsize length = strlen (name);

	if (length == 16 && is_hex (name, 16)) {
		*serial = (guint32) strtoul (name + 8, NULL, 16);
		return TRUE;
	}
	if (length == 9 && name[4] == '-' && is_hex (name, 4) && is_hex (name + 5, 4)) {
		*serial = (guint32) ((strtoul (name, NULL, 16) << 16) | strtoul (name + 5, NULL, 16));
		return TRUE;
	}

	return FALSE;
}

static gboolean
same_device (const char *a, const char *b)
{
	char *real_a = realpath (a, NULL);
	char *real_b = realpath (b, NULL);
	gboolean same = real_a != NULL && real_b != NULL && strcmp (real_a, real_b) == 0;

	free (real_a);
	free (real_b);

	return same;
}

/* Where the drive with this serial is mounted, whole. */
static char *
mount_point_for_serial (guint32 serial, GPtrArray *mounts)
{
	const char *dir = by_uuid_path != NULL ? by_uuid_path : "/dev/disk/by-uuid";
	GFileEnumerator *children = open_dir (dir);
	GFileInfo *info;
	char *found = NULL;

	if (children == NULL) {
		return NULL;
	}

	while (found == NULL &&
	       (info = g_file_enumerator_next_file (children, NULL, NULL)) != NULL) {
		guint32 candidate;

		if (serial_from_uuid (g_file_info_get_name (info), &candidate) &&
		    candidate == serial) {
			char *device = g_build_filename (dir, g_file_info_get_name (info), NULL);
			guint i;

			for (i = 0; i < mounts->len && found == NULL; i++) {
				MountEntry *entry = g_ptr_array_index (mounts, i);

				if (strcmp (entry->root, "/") == 0 &&
				    same_device (entry->source, device)) {
					found = g_strdup (entry->mount_point);
				}
			}
			g_free (device);
		}
		g_object_unref (info);
	}
	g_object_unref (children);

	return found;
}

static gboolean
is_drive_path (const char *path)
{
	return path != NULL && g_ascii_isalpha (path[0]) && path[1] == ':';
}

static char *
path_on_volume (const char *windows_path, guint32 serial, GPtrArray *mounts)
{
	char *mount_point, *found;

	if (!is_drive_path (windows_path)) {
		return NULL;
	}

	mount_point = mount_point_for_serial (serial, mounts);
	if (mount_point == NULL) {
		return NULL;
	}

	found = walk_path (mount_point, windows_path + 2);
	g_free (mount_point);

	return found;
}

/* "\\server\share\more" into server, share and the rest. Takes forward
   slashes too, for the //server/share a mount table writes. */
static gboolean
split_share (const char *text, char **server, char **share, char **rest)
{
	const char *p = text, *sep, *end;

	while (*p == '\\' || *p == '/') {
		p++;
	}
	sep = strpbrk (p, "\\/");
	if (sep == NULL || sep == p) {
		return FALSE;
	}

	end = strpbrk (sep + 1, "\\/");
	if (end == sep + 1 || sep[1] == '\0') {
		return FALSE;
	}

	*server = g_strndup (p, sep - p);
	*share = end != NULL ? g_strndup (sep + 1, end - sep - 1) : g_strdup (sep + 1);
	*rest = g_strdup (end != NULL ? end + 1 : "");

	return TRUE;
}

/* What is left of path once prefix is taken off the front, by whole names and
   ignoring case, or NULL when path is not under prefix. */
static char *
strip_prefix (const char *path, const char *prefix)
{
	char **want = g_strsplit_set (prefix, "\\/", -1);
	char **have = g_strsplit_set (path, "\\/", -1);
	char *left = NULL;
	int w = 0, h = 0;

	for (;;) {
		while (want[w] != NULL && want[w][0] == '\0') {
			w++;
		}
		while (have[h] != NULL && have[h][0] == '\0') {
			h++;
		}
		if (want[w] == NULL) {
			left = g_strjoinv ("\\", have + h);
			break;
		}
		if (have[h] == NULL || !same_name (want[w], have[h])) {
			break;
		}
		w++;
		h++;
	}
	g_strfreev (want);
	g_strfreev (have);

	return left;
}

/* A share this machine already has mounted: by the kernel's SMB client, or by
   gvfs under its FUSE folder. */
static char *
path_on_share (const char *server, const char *share, const char *rest,
	       GPtrArray *mounts)
{
	GFileEnumerator *children;
	GFileInfo *info;
	char *gvfs_dir;
	char *found = NULL;
	guint i;

	for (i = 0; i < mounts->len && found == NULL; i++) {
		MountEntry *entry = g_ptr_array_index (mounts, i);
		char *mount_server, *mount_share, *mount_rest, *under;

		if (strcmp (entry->fstype, "cifs") != 0 &&
		    strcmp (entry->fstype, "smb3") != 0 &&
		    strcmp (entry->fstype, "smbfs") != 0) {
			continue;
		}
		if (!split_share (entry->source, &mount_server, &mount_share, &mount_rest)) {
			continue;
		}

		if (same_name (mount_server, server) && same_name (mount_share, share)) {
			/* A mount of a folder inside the share covers only what is
			   under that folder. */
			under = strip_prefix (rest, mount_rest);
			if (under != NULL) {
				found = walk_path (entry->mount_point, under);
			}
			g_free (under);
		}
		g_free (mount_server);
		g_free (mount_share);
		g_free (mount_rest);
	}

	if (found != NULL) {
		return found;
	}

	gvfs_dir = gvfs_path != NULL ? g_strdup (gvfs_path)
				     : g_build_filename (g_get_user_runtime_dir (), "gvfs", NULL);
	children = open_dir (gvfs_dir);

	while (children != NULL && found == NULL &&
	       (info = g_file_enumerator_next_file (children, NULL, NULL)) != NULL) {
		const char *name = g_file_info_get_name (info);

		if (g_str_has_prefix (name, "smb-share:")) {
			char **pairs = g_strsplit (name + strlen ("smb-share:"), ",", -1);
			const char *mount_server = NULL, *mount_share = NULL;
			int p;

			for (p = 0; pairs[p] != NULL; p++) {
				if (g_str_has_prefix (pairs[p], "server=")) {
					mount_server = pairs[p] + strlen ("server=");
				} else if (g_str_has_prefix (pairs[p], "share=")) {
					mount_share = pairs[p] + strlen ("share=");
				}
			}

			if (mount_server != NULL && mount_share != NULL &&
			    same_name (mount_server, server) && same_name (mount_share, share)) {
				char *mount_point = g_build_filename (gvfs_dir, name, NULL);

				found = walk_path (mount_point, rest);
				g_free (mount_point);
			}
			g_strfreev (pairs);
		}
		g_object_unref (info);
	}
	g_clear_object (&children);
	g_free (gvfs_dir);

	return found;
}

static gboolean
smb_supported (void)
{
	const gchar * const *schemes = g_vfs_get_supported_uri_schemes (g_vfs_get_default ());

	return schemes != NULL && g_strv_contains (schemes, "smb");
}

static char *
smb_uri (const char *server, const char *share, const char *rest)
{
	GString *uri = g_string_new ("smb://");
	char **parts = g_strsplit_set (rest, "\\/", -1);
	char *escaped;
	int i;

	escaped = g_uri_escape_string (server, NULL, FALSE);
	g_string_append (uri, escaped);
	g_free (escaped);

	g_string_append_c (uri, '/');
	escaped = g_uri_escape_string (share, NULL, FALSE);
	g_string_append (uri, escaped);
	g_free (escaped);

	for (i = 0; parts[i] != NULL; i++) {
		if (parts[i][0] == '\0') {
			continue;
		}
		g_string_append_c (uri, '/');
		escaped = g_uri_escape_string (parts[i], NULL, FALSE);
		g_string_append (uri, escaped);
		g_free (escaped);
	}
	g_strfreev (parts);

	return g_string_free (uri, FALSE);
}

/* The order Windows itself tries: the absolute path first, then the path
   relative to the shortcut. */
char *
nemo_lnk_resolve (const char *lnk_path, const NemoLnk *lnk)
{
	GPtrArray *mounts = read_mounts ();
	char *server = NULL, *share = NULL, *rest = NULL;
	char *found = NULL, *uri = NULL;
	gboolean have_share;

	have_share = lnk->net_share != NULL &&
		     split_share (lnk->net_share, &server, &share, &rest);
	if (have_share && lnk->net_path != NULL && lnk->net_path[0] != '\0') {
		char *joined = join_windows (rest, lnk->net_path);

		g_free (rest);
		rest = joined;
	}

	if (lnk->local_path != NULL && lnk->local_path[0] == '/' &&
	    g_file_test (lnk->local_path, G_FILE_TEST_EXISTS)) {
		found = g_strdup (lnk->local_path);
	}
	if (found == NULL && lnk->local_path != NULL && lnk->has_serial) {
		found = path_on_volume (lnk->local_path, lnk->drive_serial, mounts);
	}
	if (found == NULL && have_share) {
		found = path_on_share (server, share, rest, mounts);
	}
	if (found == NULL && lnk->relative_path != NULL && lnk->relative_path[0] != '\0') {
		char *dir = g_path_get_dirname (lnk_path);

		found = walk_path (dir, lnk->relative_path);
		g_free (dir);
	}

	if (found != NULL) {
		uri = g_filename_to_uri (found, NULL, NULL);
	} else if (have_share && smb_supported ()) {
		uri = smb_uri (server, share, rest);
	}

	g_free (found);
	g_free (server);
	g_free (share);
	g_free (rest);
	g_ptr_array_unref (mounts);

	return uri;
}

char *
nemo_lnk_resolve_dir (const NemoLnk *lnk, const char *windows_path)
{
	GPtrArray *mounts;
	char *found = NULL;

	if (windows_path == NULL || windows_path[0] == '\0') {
		return NULL;
	}

	mounts = read_mounts ();

	/* Only a folder on the target's own drive can be placed: that is the one
	   drive whose serial is known. */
	if (is_drive_path (windows_path) && lnk->has_serial &&
	    is_drive_path (lnk->local_path) &&
	    g_ascii_tolower (windows_path[0]) == g_ascii_tolower (lnk->local_path[0])) {
		found = path_on_volume (windows_path, lnk->drive_serial, mounts);
	} else if (g_str_has_prefix (windows_path, "\\\\")) {
		char *server, *share, *rest;

		if (split_share (windows_path, &server, &share, &rest)) {
			found = path_on_share (server, share, rest, mounts);
			g_free (server);
			g_free (share);
			g_free (rest);
		}
	}
	g_ptr_array_unref (mounts);

	if (found != NULL && !g_file_test (found, G_FILE_TEST_IS_DIR)) {
		g_clear_pointer (&found, g_free);
	}

	return found;
}

static void
put_u16 (GByteArray *out, guint16 value)
{
	guint8 bytes[2] = { value & 0xff, value >> 8 };

	g_byte_array_append (out, bytes, 2);
}

static void
put_u32 (GByteArray *out, guint32 value)
{
	guint8 bytes[4] = { value & 0xff, (value >> 8) & 0xff, (value >> 16) & 0xff, value >> 24 };

	g_byte_array_append (out, bytes, 4);
}

static void
set_u32 (GByteArray *out, gsize at, guint32 value)
{
	out->data[at] = value & 0xff;
	out->data[at + 1] = (value >> 8) & 0xff;
	out->data[at + 2] = (value >> 16) & 0xff;
	out->data[at + 3] = value >> 24;
}

static void
put_filetime (GByteArray *out, gint64 unix_seconds)
{
	guint64 ticks = unix_seconds > 0
		? (guint64) (unix_seconds + FILETIME_UNIX_OFFSET) * 10000000
		: 0;

	put_u32 (out, (guint32) ticks);
	put_u32 (out, (guint32) (ticks >> 32));
}

/* The ANSI copy only has to stand in when the UTF-16 one is not read, so
   anything past ASCII is a question mark there rather than a guess at the
   code page. */
static void
put_ansi (GByteArray *out, const char *text)
{
	const guint8 *p;

	for (p = (const guint8 *) text; *p != '\0'; p++) {
		guint8 c = *p < 0x80 ? *p : '?';

		/* One mark per character, not per byte. */
		if (*p >= 0x80 && (*p & 0xc0) == 0x80) {
			continue;
		}
		g_byte_array_append (out, &c, 1);
	}
	g_byte_array_append (out, (const guint8 *) "", 1);
}

static void
put_utf16 (GByteArray *out, const char *text, gboolean terminate)
{
	glong units = 0;
	gunichar2 *wide = g_utf8_to_utf16 (text, -1, NULL, &units, NULL);
	glong i;

	for (i = 0; wide != NULL && i < units; i++) {
		put_u16 (out, wide[i]);
	}
	if (terminate) {
		put_u16 (out, 0);
	}
	g_free (wide);
}

static glong
utf16_length (const char *text)
{
	glong units = 0;
	gunichar2 *wide = g_utf8_to_utf16 (text, -1, NULL, &units, NULL);

	if (wide == NULL) {
		return -1;
	}
	g_free (wide);
	return units;
}

static char *
to_backslashes (const char *path)
{
	char *out = g_strdup (path);

	g_strdelimit (out, "/", '\\');
	return out;
}

/* What is left of path under dir, or NULL when it is not under it. By whole
   names, so /mnt/share does not cover /mnt/shared. */
static const char *
path_under (const char *path, const char *dir)
{
	gsize length = strlen (dir);

	while (length > 1 && dir[length - 1] == '/') {
		length--;
	}
	if (strncmp (path, dir, length) != 0) {
		return NULL;
	}
	if (path[length] == '\0') {
		return path + length;
	}
	if (path[length] != '/') {
		return NULL;
	}

	return path + length + 1;
}

/* The \\server\share a local path sits on, and the rest of it under that, when
   it is on a share mounted by the kernel or by gvfs. The deepest mount wins. */
static gboolean
share_for_path (const char *path, GPtrArray *mounts, char **unc, char **suffix)
{
	const char *under, *best_under = NULL;
	MountEntry *best = NULL;
	char *gvfs_dir, *server = NULL, *share = NULL, *rest = NULL;
	guint i;

	for (i = 0; i < mounts->len; i++) {
		MountEntry *entry = g_ptr_array_index (mounts, i);

		if (strcmp (entry->fstype, "cifs") != 0 &&
		    strcmp (entry->fstype, "smb3") != 0 &&
		    strcmp (entry->fstype, "smbfs") != 0) {
			continue;
		}
		under = path_under (path, entry->mount_point);
		if (under != NULL &&
		    (best == NULL || strlen (entry->mount_point) > strlen (best->mount_point))) {
			best = entry;
			best_under = under;
		}
	}

	if (best != NULL && split_share (best->source, &server, &share, &rest)) {
		char *joined = g_build_path ("/", rest, best_under, NULL);

		g_free (rest);
		rest = joined;
	}

	gvfs_dir = gvfs_path != NULL ? g_strdup (gvfs_path)
				     : g_build_filename (g_get_user_runtime_dir (), "gvfs", NULL);
	under = server == NULL ? path_under (path, gvfs_dir) : NULL;
	if (under != NULL && g_str_has_prefix (under, "smb-share:")) {
		const char *slash = strchr (under, '/');
		char *name = slash != NULL ? g_strndup (under, slash - under) : g_strdup (under);
		char **pairs = g_strsplit (name + strlen ("smb-share:"), ",", -1);
		int p;

		for (p = 0; pairs[p] != NULL; p++) {
			if (g_str_has_prefix (pairs[p], "server=")) {
				g_free (server);
				server = g_uri_unescape_string (pairs[p] + strlen ("server="), NULL);
			} else if (g_str_has_prefix (pairs[p], "share=")) {
				g_free (share);
				share = g_uri_unescape_string (pairs[p] + strlen ("share="), NULL);
			}
		}
		if (server != NULL && share != NULL) {
			rest = g_strdup (slash != NULL ? slash + 1 : "");
		} else {
			g_clear_pointer (&server, g_free);
			g_clear_pointer (&share, g_free);
		}
		g_strfreev (pairs);
		g_free (name);
	}
	g_free (gvfs_dir);

	if (server == NULL || share == NULL || server[0] == '\0' || share[0] == '\0') {
		g_free (server);
		g_free (share);
		g_free (rest);
		return FALSE;
	}

	*unc = g_strconcat ("\\\\", server, "\\", share, NULL);
	*suffix = to_backslashes (rest != NULL ? rest : "");
	/* A share mounted at a folder inside it leaves a leading slash. */
	while ((*suffix)[0] == '\\') {
		memmove (*suffix, *suffix + 1, strlen (*suffix));
	}
	g_free (server);
	g_free (share);
	g_free (rest);

	return TRUE;
}

/* LinkInfo with only the network part, and the UTF-16 copies Windows reads
   first. [MS-SHLLINK] 2.3 and 2.3.2. */
static void
put_network_link_info (GByteArray *out, const char *unc, const char *suffix)
{
	gsize start = out->len, net_start, suffix_at, suffix_unicode_at;

	put_u32 (out, 0);                 /* size, set below */
	put_u32 (out, 0x24);              /* header size with the UTF-16 offsets */
	put_u32 (out, INFO_NETWORK);
	put_u32 (out, 0);                 /* no VolumeID */
	put_u32 (out, 0);                 /* no LocalBasePath */
	put_u32 (out, 0x24);              /* CommonNetworkRelativeLink */
	put_u32 (out, 0);                 /* CommonPathSuffix, set below */
	put_u32 (out, 0);                 /* no LocalBasePathUnicode */
	put_u32 (out, 0);                 /* CommonPathSuffixUnicode, set below */

	net_start = out->len;
	put_u32 (out, 0);                 /* size, set below */
	put_u32 (out, NET_TYPE_VALID);
	put_u32 (out, 0x1c);              /* NetName */
	put_u32 (out, 0);                 /* no DeviceName */
	put_u32 (out, NET_PROVIDER_LANMAN);
	put_u32 (out, 0);                 /* NetNameUnicode, set below */
	put_u32 (out, 0);                 /* no DeviceNameUnicode */
	put_ansi (out, unc);
	set_u32 (out, net_start + 20, out->len - net_start);
	put_utf16 (out, unc, TRUE);
	set_u32 (out, net_start, out->len - net_start);

	suffix_at = out->len - start;
	put_ansi (out, suffix);
	suffix_unicode_at = out->len - start;
	put_utf16 (out, suffix, TRUE);

	set_u32 (out, start + 24, suffix_at);
	set_u32 (out, start + 32, suffix_unicode_at);
	set_u32 (out, start, out->len - start);
}

gboolean
nemo_lnk_write (const char  *lnk_path,
		const char  *target_path,
		GError     **error)
{
	GStatBuf info;
	GByteArray *out;
	GPtrArray *mounts;
	GFile *file;
	GFileOutputStream *stream;
	char *dir, *relative, *relative_windows = NULL, *real, *unc = NULL, *suffix = NULL;
	guint32 flags = FLAG_IS_UNICODE;
	gboolean is_dir, ok;
	glong units;

	if (g_stat (target_path, &info) != 0) {
		g_set_error (error, G_IO_ERROR, g_io_error_from_errno (errno),
			     _("Could not read \"%s\"."), target_path);
		return FALSE;
	}
	is_dir = S_ISDIR (info.st_mode);

	/* Written the way Windows writes it, from the folder the shortcut is in. */
	dir = g_path_get_dirname (lnk_path);
	relative = nemo_link_relative_target (target_path, dir);
	g_free (dir);
	units = relative != NULL ? utf16_length (relative) : -1;
	if (units >= 0 && units <= G_MAXUINT16) {
		relative_windows = to_backslashes (relative);
		if (!g_str_has_prefix (relative, "..")) {
			char *dotted = g_strconcat (".\\", relative_windows, NULL);

			g_free (relative_windows);
			relative_windows = dotted;
		}
		flags |= FLAG_HAS_RELATIVE_PATH;
	}
	g_free (relative);

	mounts = read_mounts ();
	real = realpath (target_path, NULL);
	if (real != NULL && share_for_path (real, mounts, &unc, &suffix)) {
		flags |= FLAG_HAS_LINK_INFO;
	}
	free (real);
	g_ptr_array_unref (mounts);

	if (!(flags & (FLAG_HAS_RELATIVE_PATH | FLAG_HAS_LINK_INFO))) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			     _("No shortcut to \"%s\" can be made here."), target_path);
		return FALSE;
	}

	out = g_byte_array_new ();
	put_u32 (out, LNK_HEADER_SIZE);
	g_byte_array_append (out, lnk_clsid, sizeof lnk_clsid);
	put_u32 (out, flags);
	put_u32 (out, is_dir ? ATTRIBUTE_DIRECTORY : ATTRIBUTE_ARCHIVE);
	put_filetime (out, info.st_ctime);
	put_filetime (out, info.st_atime);
	put_filetime (out, info.st_mtime);
	put_u32 (out, is_dir ? 0 : (guint32) info.st_size);
	put_u32 (out, 0);                 /* icon index */
	put_u32 (out, SW_SHOWNORMAL);
	put_u16 (out, 0);                 /* hotkey */
	put_u16 (out, 0);
	put_u32 (out, 0);
	put_u32 (out, 0);

	if (flags & FLAG_HAS_LINK_INFO) {
		put_network_link_info (out, unc, suffix);
	}
	if (flags & FLAG_HAS_RELATIVE_PATH) {
		put_u16 (out, (guint16) utf16_length (relative_windows));
		put_utf16 (out, relative_windows, FALSE);
	}
	put_u32 (out, 0);                 /* no extra data blocks */

	file = g_file_new_for_path (lnk_path);
	stream = g_file_create (file, G_FILE_CREATE_NONE, NULL, error);
	ok = stream != NULL &&
	     g_output_stream_write_all (G_OUTPUT_STREAM (stream), out->data, out->len, NULL, NULL, error) &&
	     g_output_stream_close (G_OUTPUT_STREAM (stream), NULL, error);
	if (stream != NULL && !ok) {
		g_file_delete (file, NULL, NULL);
	}
	g_clear_object (&stream);
	g_object_unref (file);

	g_byte_array_unref (out);
	g_free (relative_windows);
	g_free (unc);
	g_free (suffix);

	return ok;
}

static gboolean
path_is_lnk (const char *path)
{
	gsize length = strlen (path);

	return length > 4 && g_ascii_strcasecmp (path + length - 4, ".lnk") == 0;
}

char *
nemo_lnk_follow (const char *lnk_path, NemoLnk *lnk_out)
{
	GHashTable *seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	char *current = g_strdup (lnk_path);
	char *uri = NULL;
	int hop;

	memset (lnk_out, 0, sizeof *lnk_out);

	for (hop = 0; hop < LNK_MAX_HOPS; hop++) {
		NemoLnk lnk;
		char *path;

		if (g_hash_table_contains (seen, current)) {
			g_clear_pointer (&uri, g_free);
			break;
		}
		g_hash_table_insert (seen, g_strdup (current), NULL);

		if (!nemo_lnk_read (current, &lnk)) {
			/* Not a shortcut after all, however it is named: the chain
			   ends at it. Only the first file has to be one. */
			if (hop == 0) {
				g_clear_pointer (&uri, g_free);
			}
			break;
		}

		g_free (uri);
		uri = nemo_lnk_resolve (current, &lnk);
		nemo_lnk_clear (lnk_out);
		*lnk_out = lnk;

		path = uri != NULL ? g_filename_from_uri (uri, NULL, NULL) : NULL;
		if (path == NULL || !path_is_lnk (path)) {
			g_free (path);
			break;
		}

		g_free (current);
		current = path;
	}

	/* Out of hops while still on a shortcut. */
	if (hop == LNK_MAX_HOPS) {
		g_clear_pointer (&uri, g_free);
	}

	g_free (current);
	g_hash_table_destroy (seen);

	return uri;
}

typedef struct {
	gint64 mtime;
	gboolean readable;
	gboolean is_dir;
	char *target_name;
} IconEntry;

static void
icon_entry_free (gpointer data)
{
	IconEntry *entry = data;

	g_free (entry->target_name);
	g_free (entry);
}

static GHashTable *icon_cache;
G_LOCK_DEFINE_STATIC (icon_cache);

static char *
windows_basename (const char *path)
{
	const char *slash;

	if (path == NULL) {
		return NULL;
	}
	slash = strrchr (path, '\\');
	if (slash == NULL) {
		slash = strrchr (path, '/');
	}

	return g_strdup (slash != NULL ? slash + 1 : path);
}

GIcon *
nemo_lnk_icon_for_path (const char *lnk_path, gint64 mtime)
{
	IconEntry *entry;
	GIcon *icon = NULL;

	G_LOCK (icon_cache);

	if (icon_cache == NULL) {
		icon_cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, icon_entry_free);
	}

	entry = g_hash_table_lookup (icon_cache, lnk_path);
	if (entry == NULL || entry->mtime != mtime) {
		NemoLnk lnk;
		char *target;

		if (g_hash_table_size (icon_cache) >= ICON_CACHE_MAX) {
			g_hash_table_remove_all (icon_cache);
		}

		entry = g_new0 (IconEntry, 1);
		entry->mtime = mtime;
		entry->readable = nemo_lnk_read (lnk_path, &lnk);
		if (entry->readable) {
			entry->is_dir = nemo_lnk_is_dir (&lnk);
			target = nemo_lnk_display_target (&lnk);
			entry->target_name = windows_basename (target);
			g_free (target);
			nemo_lnk_clear (&lnk);
		}
		g_hash_table_replace (icon_cache, g_strdup (lnk_path), entry);
	}

	if (entry->readable && entry->is_dir) {
		icon = g_themed_icon_new ("folder");
	} else if (entry->readable && entry->target_name != NULL && entry->target_name[0] != '\0') {
		char *type = g_content_type_guess (entry->target_name, NULL, 0, NULL);

		icon = g_content_type_get_icon (type);
		g_free (type);
	}

	G_UNLOCK (icon_cache);

	return icon;
}
