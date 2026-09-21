/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-thumbnail-db.c - the thumbnail cache store.

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

#include "nemo-thumbnail-db.h"

#include "nemo-file-utilities.h"

#include <glib/gstdio.h>
#include <sqlite3.h>
#include <string.h>

/* Bumped when the tables change in a way an older file cannot be read as. The
 * store is a cache, so an unreadable one is thrown away rather than migrated. */
#define SCHEMA_VERSION 1

/* How long a statement waits for another process to finish writing. Long enough
 * that a busy window wins rather than dropping its thumbnail, short enough that
 * a stuck one does not hold up a draw. */
#define BUSY_TIMEOUT_MS 3000

/* Draw counts are batched rather than written one at a time - see note_render. */
#define RENDER_FLUSH_SECS 30
#define RENDER_FLUSH_MAX  256

struct _NemoThumbnailDb {
	sqlite3     *handle;
	GMutex       lock;

	/* Set when sqlite reports the file is damaged. Everything after that is a
	 * no-op: the next launch finds it at open time and starts over, which is
	 * safer than deleting a file other processes still have open. */
	gboolean     broken;

	/* uri -> draws not yet written. Guarded by `lock`. */
	GHashTable  *pending;
	guint        flush_id;
};

/* A static GMutex needs no initialising, and guards the one-time open. Not
 * g_once_init, which latches for good - closing the store has to leave it
 * able to open again. */
static GMutex           the_db_lock;
static NemoThumbnailDb *the_db = NULL;
static gboolean         the_db_tried = FALSE;

static const char SCHEMA[] =
	"CREATE TABLE IF NOT EXISTS images ("
	"  id     INTEGER PRIMARY KEY,"
	"  bytes  INTEGER NOT NULL,"
	"  mtime  INTEGER NOT NULL,"
	"  digest BLOB,"
	"  size   INTEGER NOT NULL,"
	"  width  INTEGER NOT NULL,"
	"  height INTEGER NOT NULL,"
	"  format INTEGER NOT NULL,"
	"  stored INTEGER NOT NULL,"
	"  image  BLOB NOT NULL);"
	"CREATE INDEX IF NOT EXISTS images_content ON images (bytes, mtime);"
	"CREATE INDEX IF NOT EXISTS images_digest ON images (digest) WHERE digest IS NOT NULL;"
	"CREATE TABLE IF NOT EXISTS paths ("
	"  uri      TEXT PRIMARY KEY,"
	"  image_id INTEGER NOT NULL REFERENCES images (id) ON DELETE CASCADE,"
	"  rendered INTEGER NOT NULL,"
	"  renders  INTEGER NOT NULL DEFAULT 0);"
	"CREATE INDEX IF NOT EXISTS paths_image ON paths (image_id);"
	"CREATE INDEX IF NOT EXISTS paths_rendered ON paths (rendered);";

/* sqlite reports a damaged file several ways depending on where it noticed. */
static gboolean
is_corruption (int rc)
{
	switch (rc & 0xff) {
	case SQLITE_CORRUPT:
	case SQLITE_NOTADB:
	case SQLITE_FORMAT:
		return TRUE;
	default:
		return FALSE;
	}
}

/* Answers whether the call went well, and remembers a damaged file. */
static gboolean
db_ok (NemoThumbnailDb *db, int rc, const char *what)
{
	if (rc == SQLITE_OK || rc == SQLITE_DONE || rc == SQLITE_ROW)
		return TRUE;

	if (is_corruption (rc)) {
		if (!db->broken)
			g_warning ("thumbnail cache is damaged and will be rebuilt next run (%s: %s)",
				   what, sqlite3_errstr (rc));
		db->broken = TRUE;
	} else {
		g_debug ("thumbnail cache %s: %s", what, sqlite3_errstr (rc));
	}

	return FALSE;
}

char *
nemo_thumbnail_db_path (void)
{
	g_autofree char *dir = nemo_get_user_cache_directory ();

	if (dir == NULL)
		return NULL;

	return g_build_filename (dir, "thumbnails.db", NULL);
}

/* Opens the file and puts the schema in it. Answers NULL and leaves nothing
 * behind if it could not, so the caller can wipe and try once more. */
static sqlite3 *
open_at (const char *path, gboolean *out_corrupt)
{
	sqlite3 *handle = NULL;
	char    *err = NULL;
	int      rc;

	*out_corrupt = FALSE;

	rc = sqlite3_open_v2 (path, &handle,
			      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
			      NULL);
	if (rc != SQLITE_OK) {
		*out_corrupt = is_corruption (rc);
		g_warning ("could not open the thumbnail cache %s: %s", path, sqlite3_errstr (rc));
		sqlite3_close (handle);
		return NULL;
	}

	sqlite3_busy_timeout (handle, BUSY_TIMEOUT_MS);

	/* WAL so a reading window is not blocked by a writing one, and NORMAL
	 * because losing the last few thumbnails to a power cut costs a re-render
	 * and nothing else. */
	rc = sqlite3_exec (handle,
			   "PRAGMA journal_mode = WAL;"
			   "PRAGMA synchronous = NORMAL;"
			   "PRAGMA foreign_keys = ON;",
			   NULL, NULL, &err);
	if (rc != SQLITE_OK) {
		*out_corrupt = is_corruption (rc);
		g_warning ("could not set up the thumbnail cache: %s", err ? err : sqlite3_errstr (rc));
		sqlite3_free (err);
		sqlite3_close (handle);
		return NULL;
	}

	rc = sqlite3_exec (handle, SCHEMA, NULL, NULL, &err);
	if (rc != SQLITE_OK) {
		*out_corrupt = is_corruption (rc);
		g_warning ("could not make the thumbnail cache tables: %s", err ? err : sqlite3_errstr (rc));
		sqlite3_free (err);
		sqlite3_close (handle);
		return NULL;
	}

	sqlite3_exec (handle, "PRAGMA user_version = " G_STRINGIFY (SCHEMA_VERSION) ";",
		      NULL, NULL, NULL);

	return handle;
}

/* WAL keeps two files beside the database, and a wipe that leaves them behind
 * hands the new file someone else's journal. */
static void
remove_db_files (const char *path)
{
	g_autofree char *wal = g_strconcat (path, "-wal", NULL);
	g_autofree char *shm = g_strconcat (path, "-shm", NULL);

	g_unlink (path);
	g_unlink (wal);
	g_unlink (shm);
}

static NemoThumbnailDb *
db_open (void)
{
	NemoThumbnailDb *db;
	sqlite3         *handle;
	gboolean         corrupt = FALSE;
	g_autofree char *path = NULL;

	/* A build of sqlite with threading compiled out would corrupt the file the
	 * first time a worker wrote while the main loop read. Nothing we ship is
	 * built that way, so say so rather than guard every call. */
	if (sqlite3_threadsafe () == 0) {
		g_warning ("sqlite was built without thread support, so the thumbnail cache is off");
		return NULL;
	}

	path = nemo_thumbnail_db_path ();
	if (path == NULL)
		return NULL;

	handle = open_at (path, &corrupt);

	if (handle == NULL && corrupt) {
		g_message ("rebuilding the damaged thumbnail cache at %s", path);
		remove_db_files (path);
		handle = open_at (path, &corrupt);
	}

	if (handle == NULL)
		return NULL;

	db = g_new0 (NemoThumbnailDb, 1);
	db->handle = handle;
	g_mutex_init (&db->lock);
	db->pending = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	return db;
}

NemoThumbnailDb *
nemo_thumbnail_db_get (void)
{
	NemoThumbnailDb *db;

	g_mutex_lock (&the_db_lock);

	/* One attempt per open store. A database that could not be opened stays
	 * shut rather than being retried on every draw. */
	if (!the_db_tried) {
		the_db = db_open ();
		the_db_tried = TRUE;
	}

	db = the_db;

	g_mutex_unlock (&the_db_lock);

	return db;
}

/* Fills `record` from a row laid out as the SELECTs below produce it, and takes
 * the image bytes when the caller asked for them. */
static void
read_row (sqlite3_stmt *stmt, NemoThumbnailRecord *record, GBytes **image)
{
	const void *digest;
	int         digest_len;

	record->bytes  = sqlite3_column_int64 (stmt, 1);
	record->mtime  = sqlite3_column_int64 (stmt, 2);
	record->size   = sqlite3_column_int (stmt, 3);
	record->width  = sqlite3_column_int (stmt, 4);
	record->height = sqlite3_column_int (stmt, 5);
	record->format = (NemoThumbnailFormat) sqlite3_column_int (stmt, 6);
	record->stored = sqlite3_column_int64 (stmt, 7);

	digest = sqlite3_column_blob (stmt, 8);
	digest_len = sqlite3_column_bytes (stmt, 8);
	record->has_digest = (digest != NULL && digest_len == (int) sizeof (record->digest));
	if (record->has_digest)
		memcpy (record->digest, digest, sizeof (record->digest));
	else
		memset (record->digest, 0, sizeof (record->digest));

	if (image != NULL) {
		const void *blob = sqlite3_column_blob (stmt, 9);
		int         len = sqlite3_column_bytes (stmt, 9);

		*image = (blob != NULL && len > 0)
			? g_bytes_new (blob, (gsize) len)
			: NULL;
	}
}

#define IMAGE_COLUMNS \
	"i.id, i.bytes, i.mtime, i.size, i.width, i.height, i.format, i.stored, i.digest, i.image"

/* The uri we already know about, as long as the file has not changed under it. */
static gboolean
lookup_by_uri (NemoThumbnailDb     *db,
	       const char          *uri,
	       gint64               bytes,
	       gint64               mtime,
	       NemoThumbnailRecord *record,
	       GBytes             **image)
{
	static const char sql[] =
		"SELECT " IMAGE_COLUMNS " FROM paths p JOIN images i ON i.id = p.image_id"
		" WHERE p.uri = ? AND i.bytes = ? AND i.mtime = ?";
	sqlite3_stmt *stmt = NULL;
	gboolean      found = FALSE;

	if (!db_ok (db, sqlite3_prepare_v2 (db->handle, sql, -1, &stmt, NULL), "lookup"))
		return FALSE;

	sqlite3_bind_text (stmt, 1, uri, -1, SQLITE_STATIC);
	sqlite3_bind_int64 (stmt, 2, bytes);
	sqlite3_bind_int64 (stmt, 3, mtime);

	if (sqlite3_step (stmt) == SQLITE_ROW) {
		read_row (stmt, record, image);
		found = TRUE;
	}

	sqlite3_finalize (stmt);
	return found;
}

/* Some other path holding a thumbnail of the same contents. This is what makes
 * a moved or copied file keep the thumbnail that was already made for it. */
static gboolean
lookup_by_content (NemoThumbnailDb     *db,
		   gint64               bytes,
		   gint64               mtime,
		   const guint8        *digest,
		   NemoThumbnailRecord *record,
		   GBytes             **image,
		   gint64              *out_id)
{
	static const char by_digest[] =
		"SELECT " IMAGE_COLUMNS " FROM images i WHERE i.digest = ? AND i.bytes = ?"
		" ORDER BY i.size DESC LIMIT 1";
	static const char by_stat[] =
		"SELECT " IMAGE_COLUMNS " FROM images i WHERE i.bytes = ? AND i.mtime = ?"
		" ORDER BY i.size DESC LIMIT 1";
	sqlite3_stmt *stmt = NULL;
	gboolean      found = FALSE;
	int           rc;

	/* A checksum says two files really are the same however they are stamped.
	 * Without one, size and mtime together are the best that can be done. */
	if (digest != NULL) {
		rc = sqlite3_prepare_v2 (db->handle, by_digest, -1, &stmt, NULL);
		if (!db_ok (db, rc, "lookup by digest"))
			return FALSE;
		sqlite3_bind_blob (stmt, 1, digest, 32, SQLITE_STATIC);
		sqlite3_bind_int64 (stmt, 2, bytes);
	} else {
		rc = sqlite3_prepare_v2 (db->handle, by_stat, -1, &stmt, NULL);
		if (!db_ok (db, rc, "lookup by size and time"))
			return FALSE;
		sqlite3_bind_int64 (stmt, 1, bytes);
		sqlite3_bind_int64 (stmt, 2, mtime);
	}

	if (sqlite3_step (stmt) == SQLITE_ROW) {
		read_row (stmt, record, image);
		*out_id = sqlite3_column_int64 (stmt, 0);
		found = TRUE;
	}

	sqlite3_finalize (stmt);
	return found;
}

/* Points a uri at an image that is already there, so the next lookup is direct. */
static void
link_uri (NemoThumbnailDb *db, const char *uri, gint64 image_id)
{
	static const char sql[] =
		"INSERT INTO paths (uri, image_id, rendered, renders) VALUES (?, ?, ?, 0)"
		" ON CONFLICT (uri) DO UPDATE SET image_id = excluded.image_id";
	sqlite3_stmt *stmt = NULL;

	if (!db_ok (db, sqlite3_prepare_v2 (db->handle, sql, -1, &stmt, NULL), "link"))
		return;

	sqlite3_bind_text (stmt, 1, uri, -1, SQLITE_STATIC);
	sqlite3_bind_int64 (stmt, 2, image_id);
	sqlite3_bind_int64 (stmt, 3, g_get_real_time () / G_USEC_PER_SEC);

	db_ok (db, sqlite3_step (stmt), "link");
	sqlite3_finalize (stmt);
}

gboolean
nemo_thumbnail_db_lookup (NemoThumbnailDb     *db,
			  const char          *uri,
			  gint64               bytes,
			  gint64               mtime,
			  const guint8        *digest,
			  NemoThumbnailRecord *record,
			  GBytes             **image)
{
	gboolean found;
	gint64   image_id = 0;

	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (record != NULL, FALSE);

	if (image != NULL)
		*image = NULL;

	if (db == NULL || db->broken)
		return FALSE;

	g_mutex_lock (&db->lock);

	found = lookup_by_uri (db, uri, bytes, mtime, record, image);

	if (!found) {
		found = lookup_by_content (db, bytes, mtime, digest, record, image, &image_id);
		if (found)
			link_uri (db, uri, image_id);
	}

	g_mutex_unlock (&db->lock);

	return found;
}

gboolean
nemo_thumbnail_db_store (NemoThumbnailDb           *db,
			 const char                *uri,
			 const NemoThumbnailRecord *record,
			 GBytes                    *image)
{
	static const char insert_image[] =
		"INSERT INTO images (bytes, mtime, digest, size, width, height, format, stored, image)"
		" VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)";
	/* Anything already describing these contents is replaced, but paths that
	 * pointed at it move across first so a second copy of the file does not
	 * lose its row to the cascade. */
	static const char repoint[] =
		"UPDATE paths SET image_id = ? WHERE image_id IN"
		" (SELECT id FROM images WHERE bytes = ? AND mtime = ? AND id <> ?)";
	static const char drop_old[] =
		"DELETE FROM images WHERE bytes = ? AND mtime = ? AND id <> ?";
	static const char set_path[] =
		"INSERT INTO paths (uri, image_id, rendered, renders) VALUES (?, ?, ?, 0)"
		" ON CONFLICT (uri) DO UPDATE SET image_id = excluded.image_id";

	sqlite3_stmt *stmt = NULL;
	gconstpointer data;
	gsize         len = 0;
	gint64        image_id;
	gint64        now;
	gboolean      done = FALSE;

	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (record != NULL, FALSE);
	g_return_val_if_fail (image != NULL, FALSE);

	if (db == NULL || db->broken)
		return FALSE;

	data = g_bytes_get_data (image, &len);
	if (data == NULL || len == 0)
		return FALSE;

	now = g_get_real_time () / G_USEC_PER_SEC;

	g_mutex_lock (&db->lock);

	if (!db_ok (db, sqlite3_exec (db->handle, "BEGIN IMMEDIATE", NULL, NULL, NULL), "begin"))
		goto out;

	if (!db_ok (db, sqlite3_prepare_v2 (db->handle, insert_image, -1, &stmt, NULL), "store"))
		goto rollback;

	sqlite3_bind_int64 (stmt, 1, record->bytes);
	sqlite3_bind_int64 (stmt, 2, record->mtime);
	if (record->has_digest)
		sqlite3_bind_blob (stmt, 3, record->digest, sizeof (record->digest), SQLITE_STATIC);
	else
		sqlite3_bind_null (stmt, 3);
	sqlite3_bind_int (stmt, 4, record->size);
	sqlite3_bind_int (stmt, 5, record->width);
	sqlite3_bind_int (stmt, 6, record->height);
	sqlite3_bind_int (stmt, 7, (int) record->format);
	sqlite3_bind_int64 (stmt, 8, record->stored > 0 ? record->stored : now);
	sqlite3_bind_blob64 (stmt, 9, data, len, SQLITE_STATIC);

	if (!db_ok (db, sqlite3_step (stmt), "store")) {
		sqlite3_finalize (stmt);
		goto rollback;
	}
	sqlite3_finalize (stmt);
	stmt = NULL;

	image_id = sqlite3_last_insert_rowid (db->handle);

	if (db_ok (db, sqlite3_prepare_v2 (db->handle, repoint, -1, &stmt, NULL), "store")) {
		sqlite3_bind_int64 (stmt, 1, image_id);
		sqlite3_bind_int64 (stmt, 2, record->bytes);
		sqlite3_bind_int64 (stmt, 3, record->mtime);
		sqlite3_bind_int64 (stmt, 4, image_id);
		db_ok (db, sqlite3_step (stmt), "store");
		sqlite3_finalize (stmt);
		stmt = NULL;
	}

	if (db_ok (db, sqlite3_prepare_v2 (db->handle, drop_old, -1, &stmt, NULL), "store")) {
		sqlite3_bind_int64 (stmt, 1, record->bytes);
		sqlite3_bind_int64 (stmt, 2, record->mtime);
		sqlite3_bind_int64 (stmt, 3, image_id);
		db_ok (db, sqlite3_step (stmt), "store");
		sqlite3_finalize (stmt);
		stmt = NULL;
	}

	if (!db_ok (db, sqlite3_prepare_v2 (db->handle, set_path, -1, &stmt, NULL), "store"))
		goto rollback;

	sqlite3_bind_text (stmt, 1, uri, -1, SQLITE_STATIC);
	sqlite3_bind_int64 (stmt, 2, image_id);
	sqlite3_bind_int64 (stmt, 3, now);

	if (!db_ok (db, sqlite3_step (stmt), "store")) {
		sqlite3_finalize (stmt);
		goto rollback;
	}
	sqlite3_finalize (stmt);
	stmt = NULL;

	done = db_ok (db, sqlite3_exec (db->handle, "COMMIT", NULL, NULL, NULL), "commit");
	if (!done)
		sqlite3_exec (db->handle, "ROLLBACK", NULL, NULL, NULL);
	goto out;

rollback:
	sqlite3_exec (db->handle, "ROLLBACK", NULL, NULL, NULL);

out:
	g_mutex_unlock (&db->lock);
	return done;
}

/* Every drawn icon in a folder would otherwise be a write, and scrolling a big
 * folder draws the same file over and over. Counts are held in memory and
 * written in one transaction, which is also the only thing the age rule needs -
 * it works in days. Called with the lock held. */
static void
flush_renders (NemoThumbnailDb *db)
{
	static const char sql[] =
		"UPDATE paths SET rendered = ?, renders = renders + ? WHERE uri = ?";
	sqlite3_stmt  *stmt = NULL;
	GHashTableIter iter;
	gpointer       uri, count;
	gint64         now;

	if (db->broken || g_hash_table_size (db->pending) == 0)
		return;

	now = g_get_real_time () / G_USEC_PER_SEC;

	if (!db_ok (db, sqlite3_exec (db->handle, "BEGIN IMMEDIATE", NULL, NULL, NULL), "begin"))
		return;

	if (!db_ok (db, sqlite3_prepare_v2 (db->handle, sql, -1, &stmt, NULL), "render count")) {
		sqlite3_exec (db->handle, "ROLLBACK", NULL, NULL, NULL);
		return;
	}

	g_hash_table_iter_init (&iter, db->pending);
	while (g_hash_table_iter_next (&iter, &uri, &count)) {
		sqlite3_reset (stmt);
		sqlite3_bind_int64 (stmt, 1, now);
		sqlite3_bind_int64 (stmt, 2, (gint64) GPOINTER_TO_INT (count));
		sqlite3_bind_text (stmt, 3, (const char *) uri, -1, SQLITE_STATIC);
		db_ok (db, sqlite3_step (stmt), "render count");
	}

	sqlite3_finalize (stmt);

	if (!db_ok (db, sqlite3_exec (db->handle, "COMMIT", NULL, NULL, NULL), "commit"))
		sqlite3_exec (db->handle, "ROLLBACK", NULL, NULL, NULL);

	g_hash_table_remove_all (db->pending);
}

static gboolean
flush_renders_timeout (gpointer data)
{
	NemoThumbnailDb *db = data;

	g_mutex_lock (&db->lock);
	db->flush_id = 0;
	flush_renders (db);
	g_mutex_unlock (&db->lock);

	return G_SOURCE_REMOVE;
}

void
nemo_thumbnail_db_note_render (NemoThumbnailDb *db, const char *uri)
{
	gpointer old;

	g_return_if_fail (uri != NULL);

	if (db == NULL || db->broken)
		return;

	g_mutex_lock (&db->lock);

	old = g_hash_table_lookup (db->pending, uri);
	if (old != NULL)
		g_hash_table_replace (db->pending, g_strdup (uri),
				      GINT_TO_POINTER (GPOINTER_TO_INT (old) + 1));
	else
		g_hash_table_insert (db->pending, g_strdup (uri), GINT_TO_POINTER (1));

	if (g_hash_table_size (db->pending) >= RENDER_FLUSH_MAX)
		flush_renders (db);
	else if (db->flush_id == 0)
		db->flush_id = g_timeout_add_seconds (RENDER_FLUSH_SECS, flush_renders_timeout, db);

	g_mutex_unlock (&db->lock);
}

void
nemo_thumbnail_db_flush (NemoThumbnailDb *db)
{
	if (db == NULL || db->broken)
		return;

	g_mutex_lock (&db->lock);

	if (db->flush_id != 0) {
		g_source_remove (db->flush_id);
		db->flush_id = 0;
	}
	flush_renders (db);

	g_mutex_unlock (&db->lock);
}

gboolean
nemo_thumbnail_db_path_stats (NemoThumbnailDb *db,
			      const char      *uri,
			      gint64          *renders,
			      gint64          *rendered)
{
	static const char sql[] = "SELECT renders, rendered FROM paths WHERE uri = ?";
	sqlite3_stmt *stmt = NULL;
	gboolean      found = FALSE;

	g_return_val_if_fail (uri != NULL, FALSE);

	if (renders != NULL)
		*renders = 0;
	if (rendered != NULL)
		*rendered = 0;

	if (db == NULL || db->broken)
		return FALSE;

	g_mutex_lock (&db->lock);

	if (db_ok (db, sqlite3_prepare_v2 (db->handle, sql, -1, &stmt, NULL), "path stats")) {
		sqlite3_bind_text (stmt, 1, uri, -1, SQLITE_STATIC);

		if (sqlite3_step (stmt) == SQLITE_ROW) {
			if (renders != NULL)
				*renders = sqlite3_column_int64 (stmt, 0);
			if (rendered != NULL)
				*rendered = sqlite3_column_int64 (stmt, 1);
			found = TRUE;
		}

		sqlite3_finalize (stmt);
	}

	g_mutex_unlock (&db->lock);

	return found;
}

gboolean
nemo_thumbnail_db_empty (NemoThumbnailDb *db)
{
	char    *err = NULL;
	gboolean done;

	if (db == NULL || db->broken)
		return FALSE;

	g_mutex_lock (&db->lock);

	g_hash_table_remove_all (db->pending);

	/* Dropping the rows leaves the file its old size, and somebody who just
	 * asked for the cache to be emptied means the disk space too. */
	done = db_ok (db, sqlite3_exec (db->handle,
					"DELETE FROM paths; DELETE FROM images;",
					NULL, NULL, &err), "empty");
	sqlite3_free (err);

	if (done)
		sqlite3_exec (db->handle, "VACUUM", NULL, NULL, NULL);

	g_mutex_unlock (&db->lock);

	return done;
}

void
nemo_thumbnail_db_usage (NemoThumbnailDb *db, gint64 *n_images, gint64 *bytes)
{
	static const char sql[] = "SELECT COUNT(*), COALESCE (SUM (LENGTH (image)), 0) FROM images";
	sqlite3_stmt *stmt = NULL;

	if (n_images != NULL)
		*n_images = 0;
	if (bytes != NULL)
		*bytes = 0;

	if (db == NULL || db->broken)
		return;

	g_mutex_lock (&db->lock);

	if (db_ok (db, sqlite3_prepare_v2 (db->handle, sql, -1, &stmt, NULL), "usage")) {
		if (sqlite3_step (stmt) == SQLITE_ROW) {
			if (n_images != NULL)
				*n_images = sqlite3_column_int64 (stmt, 0);
			if (bytes != NULL)
				*bytes = sqlite3_column_int64 (stmt, 1);
		}
		sqlite3_finalize (stmt);
	}

	g_mutex_unlock (&db->lock);
}

void
nemo_thumbnail_db_close (void)
{
	NemoThumbnailDb *db;

	g_mutex_lock (&the_db_lock);
	db = the_db;
	the_db = NULL;
	the_db_tried = FALSE;
	g_mutex_unlock (&the_db_lock);

	if (db == NULL)
		return;

	g_mutex_lock (&db->lock);

	if (db->flush_id != 0) {
		g_source_remove (db->flush_id);
		db->flush_id = 0;
	}
	flush_renders (db);

	/* Folds the WAL back in, so the next launch opens one file rather than
	 * replaying a journal that could be most of the cache. */
	sqlite3_exec (db->handle, "PRAGMA wal_checkpoint (TRUNCATE)", NULL, NULL, NULL);
	sqlite3_close (db->handle);
	db->handle = NULL;

	g_hash_table_destroy (db->pending);

	g_mutex_unlock (&db->lock);
	g_mutex_clear (&db->lock);

	g_free (db);
}
