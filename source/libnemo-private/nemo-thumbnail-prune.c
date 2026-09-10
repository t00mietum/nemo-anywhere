/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-thumbnail-prune.c - keeping the thumbnail cache from growing forever.

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

#include <config.h>

#include "nemo-thumbnail-prune.h"

#include "nemo-desktop-thumbnail.h"
#include "nemo-dir-enum.h"
#include "nemo-global-preferences.h"

#include <gio/gio.h>
#include <glib/gstdio.h>
#include <stdio.h>
#include <utime.h>
#include <string.h>

#define DEBUG_FLAG NEMO_DEBUG_THUMBNAILS
#include "nemo-debug.h"

/* The sizes the spec names. Missing ones cost a failed open each. */
static const char *const size_dirs[] = { "normal", "large", "x-large", "xx-large" };

/* Enough of a PNG to reach the text chunks a thumbnailer writes; they sit
   between the header and the image data. */
#define HEADER_BYTES 8192

/* How long the sweep will spend opening files to find orphans. Past that it
   works from the file dates alone and picks the rest up tomorrow. */
#define ORPHAN_BUDGET_USEC (20 * G_USEC_PER_SEC)

/* One sweep a day is plenty, and every window is its own process now. */
#define SWEEP_INTERVAL_SECS (24 * 60 * 60)

/* Under an hour of slack, so a busy session is not rewriting dates all day. */
#define USE_STAMP_SLACK_SECS (60 * 60)

static guint32
read_be32 (const guchar *p)
{
	return ((guint32) p[0] << 24) | ((guint32) p[1] << 16) |
	       ((guint32) p[2] << 8)  | (guint32) p[3];
}

char *
nemo_thumbnail_png_text (const guchar *data,
			 gsize         len,
			 const char   *key)
{
	static const guchar signature[8] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n' };
	gsize key_len;
	gsize pos;

	g_return_val_if_fail (key != NULL, NULL);

	if (data == NULL || len < sizeof (signature) ||
	    memcmp (data, signature, sizeof (signature)) != 0) {
		return NULL;
	}

	key_len = strlen (key);
	pos = sizeof (signature);

	while (pos + 8 <= len) {
		guint32 chunk_len = read_be32 (data + pos);
		const guchar *type = data + pos + 4;
		const guchar *body = data + pos + 8;

		/* A length that overflows the buffer means the header was cut
		   short, which is the ordinary case here - only the front of
		   the file was read. */
		if (chunk_len > len - pos - 8) {
			return NULL;
		}

		if (memcmp (type, "IDAT", 4) == 0) {
			return NULL;
		}

		if (memcmp (type, "tEXt", 4) == 0 &&
		    chunk_len > key_len + 1 &&
		    memcmp (body, key, key_len) == 0 &&
		    body[key_len] == '\0') {
			const char *value = (const char *) body + key_len + 1;
			gsize value_len = chunk_len - key_len - 1;

			return g_strndup (value, value_len);
		}

		pos += 12 + chunk_len;
	}

	return NULL;
}

static gint
compare_by_use (gconstpointer a, gconstpointer b, gpointer user_data)
{
	const NemoThumbnailPruneEntry *entries = user_data;
	const NemoThumbnailPruneEntry *first = &entries[*(const guint *) a];
	const NemoThumbnailPruneEntry *second = &entries[*(const guint *) b];

	if (first->used != second->used) {
		return first->used < second->used ? -1 : 1;
	}

	/* Same second: the bigger one buys more room. */
	if (first->size != second->size) {
		return first->size > second->size ? -1 : 1;
	}

	return 0;
}

gint64
nemo_thumbnail_prune_plan (NemoThumbnailPruneEntry *entries,
			   guint                    n_entries,
			   gint64                   budget_bytes,
			   gint64                   max_age_secs,
			   gint64                   now)
{
	gint64 freed = 0;
	gint64 kept = 0;
	guint *order;
	guint n_order = 0;
	guint i;

	g_return_val_if_fail (entries != NULL || n_entries == 0, 0);

	for (i = 0; i < n_entries; i++) {
		entries[i].drop = entries[i].orphan ||
			(max_age_secs > 0 && now - entries[i].used > max_age_secs);

		if (entries[i].drop) {
			freed += entries[i].size;
		} else {
			kept += entries[i].size;
		}
	}

	if (budget_bytes <= 0 || kept <= budget_bytes) {
		return freed;
	}

	order = g_new0 (guint, n_entries);
	for (i = 0; i < n_entries; i++) {
		if (!entries[i].drop) {
			order[n_order++] = i;
		}
	}

	g_qsort_with_data (order, n_order, sizeof (guint), compare_by_use, entries);

	for (i = 0; i < n_order && kept > budget_bytes; i++) {
		NemoThumbnailPruneEntry *entry = &entries[order[i]];

		entry->drop = TRUE;
		kept -= entry->size;
		freed += entry->size;
	}

	g_free (order);

	return freed;
}

/* The file a thumbnail was made for, or NULL when the header does not say. */
static char *
read_source_uri (const char *path)
{
	guchar buffer[HEADER_BYTES];
	FILE *stream;
	gsize got;

	stream = g_fopen (path, "rb");
	if (stream == NULL) {
		return NULL;
	}

	got = fread (buffer, 1, sizeof (buffer), stream);
	fclose (stream);

	return nemo_thumbnail_png_text (buffer, got, "Thumb::URI");
}

static gboolean
source_is_gone (const char *path)
{
	char *uri;
	GFile *file;
	gboolean gone;

	uri = read_source_uri (path);
	if (uri == NULL) {
		/* Nothing to check it against. The date rules still apply. */
		return FALSE;
	}

	/* Only a local file answers quickly enough to ask about. A share that
	   is not responding costs twenty seconds a file. */
	if (!g_str_has_prefix (uri, "file:")) {
		g_free (uri);
		return FALSE;
	}

	file = g_file_new_for_uri (uri);
	gone = !g_file_query_exists (file, NULL);

	g_object_unref (file);
	g_free (uri);

	return gone;
}

static void
scan_one_dir (const char *dir_path,
	      GArray     *entries,
	      gint64      orphan_deadline)
{
	GFile *dir;
	GFileEnumerator *walk;
	GFileInfo *info;

	dir = g_file_new_for_path (dir_path);
	walk = nemo_enumerate_children (dir,
					G_FILE_ATTRIBUTE_STANDARD_NAME ","
					G_FILE_ATTRIBUTE_STANDARD_SIZE ","
					G_FILE_ATTRIBUTE_TIME_MODIFIED ","
					G_FILE_ATTRIBUTE_TIME_ACCESS,
					G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					NULL, NULL);
	if (walk == NULL) {
		g_object_unref (dir);
		return;
	}

	while ((info = g_file_enumerator_next_file (walk, NULL, NULL)) != NULL) {
		const char *name = g_file_info_get_name (info);
		NemoThumbnailPruneEntry entry = { 0 };
		guint64 modified, accessed;

		if (name == NULL || !g_str_has_suffix (name, ".png")) {
			g_object_unref (info);
			continue;
		}

		modified = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_MODIFIED);
		accessed = g_file_info_get_attribute_uint64 (info, G_FILE_ATTRIBUTE_TIME_ACCESS);

		entry.path = g_build_filename (dir_path, name, NULL);
		entry.size = (gint64) g_file_info_get_size (info);
		entry.used = (gint64) MAX (modified, accessed);
		entry.orphan = g_get_monotonic_time () < orphan_deadline &&
			source_is_gone (entry.path);

		g_array_append_val (entries, entry);
		g_object_unref (info);
	}

	g_object_unref (walk);
	g_object_unref (dir);
}

gint64
nemo_thumbnail_prune_sweep (const char *cache_dir,
			    const char *fail_appname,
			    gint64      budget_bytes,
			    gint64      max_age_secs,
			    gint64      now)
{
	GArray *entries;
	gint64 orphan_deadline;
	gint64 freed;
	guint i;

	g_return_val_if_fail (cache_dir != NULL, 0);

	entries = g_array_new (FALSE, TRUE, sizeof (NemoThumbnailPruneEntry));
	orphan_deadline = g_get_monotonic_time () + ORPHAN_BUDGET_USEC;

	for (i = 0; i < G_N_ELEMENTS (size_dirs); i++) {
		char *path = g_build_filename (cache_dir, size_dirs[i], NULL);

		scan_one_dir (path, entries, orphan_deadline);
		g_free (path);
	}

	/* Only our own failure records - another program's are its business. */
	if (fail_appname != NULL) {
		char *path = g_build_filename (cache_dir, "fail", fail_appname, NULL);

		scan_one_dir (path, entries, orphan_deadline);
		g_free (path);
	}

	freed = nemo_thumbnail_prune_plan ((NemoThumbnailPruneEntry *) entries->data,
					   entries->len, budget_bytes, max_age_secs, now);

	for (i = 0; i < entries->len; i++) {
		NemoThumbnailPruneEntry *entry =
			&g_array_index (entries, NemoThumbnailPruneEntry, i);

		if (entry->drop) {
			g_unlink (entry->path);
		}

		g_free (entry->path);
	}

	DEBUG ("thumbnail sweep: %u files, freed %" G_GINT64_FORMAT " bytes",
	       entries->len, freed);

	g_array_free (entries, TRUE);

	return freed;
}

void
nemo_thumbnail_prune_note_use (const char *thumbnail_path)
{
	GStatBuf info;
	gint64 now;

	if (thumbnail_path == NULL || g_stat (thumbnail_path, &info) != 0) {
		return;
	}

	now = g_get_real_time () / G_USEC_PER_SEC;

	/* Whichever date the filesystem is keeping up to date is enough. Most
	   Linux mounts still touch the access time once a day; Windows stopped
	   years ago, and so does a noatime mount, which is what this is for. */
	if (now - (gint64) info.st_mtime < USE_STAMP_SLACK_SECS ||
	    now - (gint64) info.st_atime < USE_STAMP_SLACK_SECS) {
		return;
	}

	g_utime (thumbnail_path, NULL);
}

static char *
stamp_path (void)
{
	return g_build_filename (g_get_user_cache_dir (), NEMO_APP_SLUG,
				 "thumbnail-prune", NULL);
}

/* TRUE when a sweep is due, and claims it by moving the stamp forward first so
   two copies starting together do not both scan. */
static gboolean
claim_sweep (void)
{
	char *path = stamp_path ();
	GStatBuf info;
	gint64 now = g_get_real_time () / G_USEC_PER_SEC;
	gboolean due = TRUE;

	if (g_stat (path, &info) == 0 &&
	    now - (gint64) info.st_mtime < SWEEP_INTERVAL_SECS) {
		due = FALSE;
	}

	if (due) {
		char *dir = g_path_get_dirname (path);

		g_mkdir_with_parents (dir, 0700);
		g_free (dir);

		if (!g_file_set_contents (path, "", 0, NULL)) {
			/* Nowhere to record it, so sweeping would repeat every
			   launch. Leave the cache alone instead. */
			due = FALSE;
		}
	}

	g_free (path);

	return due;
}

typedef struct {
	gint64 budget_bytes;
	gint64 max_age_secs;
} SweepLimits;

static void
sweep_in_thread (GTask        *task,
		 gpointer      source,
		 gpointer      task_data,
		 GCancellable *cancellable)
{
	const SweepLimits *limits = task_data;
	char *cache_dir;

	cache_dir = g_build_filename (g_get_user_cache_dir (), "thumbnails", NULL);

	nemo_thumbnail_prune_sweep (cache_dir, NEMO_DESKTOP_THUMBNAIL_FAIL_APPNAME,
				    limits->budget_bytes, limits->max_age_secs,
				    g_get_real_time () / G_USEC_PER_SEC);

	g_free (cache_dir);
	g_task_return_boolean (task, TRUE);
}

static gboolean
start_sweep (gpointer user_data)
{
	SweepLimits *limits;
	GTask *task;
	gint budget_mb, max_days;

	budget_mb = nemo_config_get_int (nemo_preferences,
					 NEMO_PREFERENCES_THUMBNAIL_CACHE_MAX_MB);
	max_days = nemo_config_get_int (nemo_preferences,
					NEMO_PREFERENCES_THUMBNAIL_CACHE_MAX_DAYS);

	if ((budget_mb <= 0 && max_days <= 0) || !claim_sweep ()) {
		return G_SOURCE_REMOVE;
	}

	limits = g_new0 (SweepLimits, 1);
	limits->budget_bytes = (gint64) MAX (budget_mb, 0) * 1024 * 1024;
	limits->max_age_secs = (gint64) MAX (max_days, 0) * 24 * 60 * 60;

	task = g_task_new (NULL, NULL, NULL, NULL);
	g_task_set_task_data (task, limits, g_free);
	g_task_set_priority (task, G_PRIORITY_LOW);
	g_task_run_in_thread (task, sweep_in_thread);
	g_object_unref (task);

	return G_SOURCE_REMOVE;
}

void
nemo_thumbnail_prune_schedule (void)
{
	static gsize scheduled = 0;

	if (g_once_init_enter (&scheduled)) {
		/* Well clear of startup - nothing here is urgent. */
		g_timeout_add_seconds (60, start_sweep, NULL);
		g_once_init_leave (&scheduled, 1);
	}
}
