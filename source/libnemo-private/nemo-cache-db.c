/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-cache-db.c - what is known about files on disk, kept between runs.

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

#include "nemo-cache-db.h"

#include "nemo-file-utilities.h"

#include <glib/gstdio.h>
#include <sqlite3.h>
#include <string.h>

/* Bumped when the tables change. The store is a cache, so a file written by
 * another version is thrown away rather than migrated. */
#define SCHEMA_VERSION 2

/* How long a statement waits for another process to finish writing. Long enough
 * that a busy window wins rather than dropping its work, short enough that a
 * stuck one does not hold up a draw. */
#define BUSY_TIMEOUT_MS 3000

/* Draw counts are batched rather than written one at a time - see note_render. */
#define RENDER_FLUSH_SECS 30
#define RENDER_FLUSH_MAX  256

struct _NemoCacheDb {
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

/* A static GMutex needs no setup, and guards the one-time open. Not
 * g_once_init, which latches for good - closing the store has to leave it
 * able to open again. */
static GMutex      the_db_lock;
static NemoCacheDb *the_db = NULL;
static gboolean     the_db_tried = FALSE;

static const char SCHEMA[] =
	"CREATE TABLE IF NOT EXISTS files ("
	"  id     INTEGER PRIMARY KEY,"
	"  bytes  INTEGER NOT NULL,"
	"  digest BLOB);"
	"CREATE UNIQUE INDEX IF NOT EXISTS files_digest ON files (digest)"
	" WHERE digest IS NOT NULL;"
	"CREATE TABLE IF NOT EXISTS paths ("
	"  uri     TEXT PRIMARY KEY,"
	"  file_id INTEGER NOT NULL REFERENCES files (id) ON DELETE CASCADE,"
	"  mtime   INTEGER NOT NULL,"
	"  seen    INTEGER NOT NULL);"
	"CREATE INDEX IF NOT EXISTS paths_file ON paths (file_id);"
	"CREATE INDEX IF NOT EXISTS paths_mtime ON paths (mtime);"
	"CREATE TABLE IF NOT EXISTS thumbnails ("
	"  file_id   INTEGER PRIMARY KEY REFERENCES files (id) ON DELETE CASCADE,"
	"  size      INTEGER NOT NULL,"
	"  width     INTEGER NOT NULL,"
	"  height    INTEGER NOT NULL,"
	"  format    INTEGER NOT NULL,"
	"  stored    INTEGER NOT NULL,"
	"  rendered  INTEGER NOT NULL DEFAULT 0,"
	"  renders   INTEGER NOT NULL DEFAULT 0,"
	"  image     BLOB NOT NULL);"
	"CREATE INDEX IF NOT EXISTS thumbnails_rendered ON thumbnails (rendered);";

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
db_ok (NemoCacheDb *db, int rc, const char *what)
{
	if (rc == SQLITE_OK || rc == SQLITE_DONE || rc == SQLITE_ROW)
		return TRUE;

	if (is_corruption (rc)) {
		if (!db->broken)
			g_warning ("the file cache is damaged and will be rebuilt next run (%s: %s)",
				   what, sqlite3_errstr (rc));
		db->broken = TRUE;
	} else {
		g_debug ("file cache %s: %s", what, sqlite3_errstr (rc));
	}

	return FALSE;
}

/* Prepares a statement, or answers NULL having noted why. */
static sqlite3_stmt *
prep (NemoCacheDb *db, const char *sql, const char *what)
{
	sqlite3_stmt *stmt = NULL;

	if (!db_ok (db, sqlite3_prepare_v2 (db->handle, sql, -1, &stmt, NULL), what)) {
		sqlite3_finalize (stmt);
		return NULL;
	}

	return stmt;
}

/* Runs a statement that returns one id, and finalizes it either way. 0 for no
 * row. */
static gint64
step_id (sqlite3_stmt *stmt)
{
	gint64 id = 0;

	if (sqlite3_step (stmt) == SQLITE_ROW)
		id = sqlite3_column_int64 (stmt, 0);

	sqlite3_finalize (stmt);

	return id;
}

static void
bind_digest (sqlite3_stmt *stmt, int pos, const NemoFileId *id)
{
	if (id->has_digest)
		sqlite3_bind_blob (stmt, pos, id->digest, NEMO_CACHE_DIGEST_LEN, SQLITE_STATIC);
	else
		sqlite3_bind_null (stmt, pos);
}

static gboolean
begin (NemoCacheDb *db)
{
	return db_ok (db, sqlite3_exec (db->handle, "BEGIN IMMEDIATE", NULL, NULL, NULL), "begin");
}

static void
rollback (NemoCacheDb *db)
{
	sqlite3_exec (db->handle, "ROLLBACK", NULL, NULL, NULL);
}

static gboolean
commit (NemoCacheDb *db)
{
	if (db_ok (db, sqlite3_exec (db->handle, "COMMIT", NULL, NULL, NULL), "commit"))
		return TRUE;

	rollback (db);
	return FALSE;
}

char *
nemo_cache_db_path (void)
{
	g_autofree char *dir = nemo_get_user_cache_directory ();

	if (dir == NULL)
		return NULL;

	return g_build_filename (dir, "files.db", NULL);
}

static int
read_user_version (sqlite3 *handle)
{
	sqlite3_stmt *stmt = NULL;
	int           version = 0;

	if (sqlite3_prepare_v2 (handle, "PRAGMA user_version", -1, &stmt, NULL) != SQLITE_OK)
		return -1;

	if (sqlite3_step (stmt) == SQLITE_ROW)
		version = sqlite3_column_int (stmt, 0);

	sqlite3_finalize (stmt);

	return version;
}

/* Opens the file and puts the schema in it. Answers NULL and leaves nothing
 * behind if it could not, so the caller can wipe and try once more. */
static sqlite3 *
open_at (const char *path, gboolean *out_rebuild)
{
	sqlite3 *handle = NULL;
	char    *err = NULL;
	int      version;
	int      rc;

	*out_rebuild = FALSE;

	rc = sqlite3_open_v2 (path, &handle,
			      SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX,
			      NULL);
	if (rc != SQLITE_OK) {
		*out_rebuild = is_corruption (rc);
		g_warning ("could not open the file cache %s: %s", path, sqlite3_errstr (rc));
		sqlite3_close (handle);
		return NULL;
	}

	sqlite3_busy_timeout (handle, BUSY_TIMEOUT_MS);

	/* WAL so a reading window is not blocked by a writing one, and NORMAL
	 * because losing the last few rows to a power cut costs a re-read and
	 * nothing else. */
	rc = sqlite3_exec (handle,
			   "PRAGMA journal_mode = WAL;"
			   "PRAGMA synchronous = NORMAL;"
			   "PRAGMA foreign_keys = ON;",
			   NULL, NULL, &err);
	if (rc != SQLITE_OK) {
		*out_rebuild = is_corruption (rc);
		g_warning ("could not set up the file cache: %s", err ? err : sqlite3_errstr (rc));
		sqlite3_free (err);
		sqlite3_close (handle);
		return NULL;
	}

	/* 0 is a file nobody has written tables into yet. Anything else that is
	 * not ours was written by another version, and gets the same treatment as
	 * a damaged file. */
	version = read_user_version (handle);
	if (version != 0 && version != SCHEMA_VERSION) {
		*out_rebuild = TRUE;
		sqlite3_close (handle);
		return NULL;
	}

	rc = sqlite3_exec (handle, SCHEMA, NULL, NULL, &err);
	if (rc != SQLITE_OK) {
		*out_rebuild = is_corruption (rc);
		g_warning ("could not make the file cache tables: %s", err ? err : sqlite3_errstr (rc));
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

static NemoCacheDb *
db_open (void)
{
	NemoCacheDb     *db;
	sqlite3         *handle;
	gboolean         rebuild = FALSE;
	g_autofree char *path = NULL;

	/* A build of sqlite with threading compiled out would corrupt the file the
	 * first time a worker wrote while the main loop read. Nothing we ship is
	 * built that way, so say so rather than guard every call. */
	if (sqlite3_threadsafe () == 0) {
		g_warning ("sqlite was built without thread support, so the file cache is off");
		return NULL;
	}

	path = nemo_cache_db_path ();
	if (path == NULL)
		return NULL;

	handle = open_at (path, &rebuild);

	if (handle == NULL && rebuild) {
		g_message ("starting the file cache at %s over", path);
		remove_db_files (path);
		handle = open_at (path, &rebuild);
	}

	if (handle == NULL)
		return NULL;

	db = g_new0 (NemoCacheDb, 1);
	db->handle = handle;
	g_mutex_init (&db->lock);
	db->pending = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	return db;
}

NemoCacheDb *
nemo_cache_db_get (void)
{
	NemoCacheDb *db;

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

/* The only one of the three that is really about the contents. */
static gint64
file_by_digest (NemoCacheDb *db, const NemoFileId *id)
{
	static const char sql[] = "SELECT id FROM files WHERE digest = ? AND bytes = ?";
	sqlite3_stmt *stmt;

	if (!id->has_digest)
		return 0;

	stmt = prep (db, sql, "find by checksum");
	if (stmt == NULL)
		return 0;

	sqlite3_bind_blob (stmt, 1, id->digest, NEMO_CACHE_DIGEST_LEN, SQLITE_STATIC);
	sqlite3_bind_int64 (stmt, 2, id->bytes);

	return step_id (stmt);
}

/* What this uri held last time, as long as the file under it has not changed.
 * A row carrying a checksum that contradicts the one in hand is not it either,
 * whatever the size and time say. */
static gint64
file_by_uri (NemoCacheDb *db, const char *uri, const NemoFileId *id)
{
	static const char sql[] =
		"SELECT c.id FROM paths p JOIN files c ON c.id = p.file_id"
		" WHERE p.uri = ? AND p.mtime = ? AND c.bytes = ?"
		" AND (? IS NULL OR c.digest IS NULL OR c.digest = ?)";
	sqlite3_stmt *stmt = prep (db, sql, "find by path");

	if (stmt == NULL)
		return 0;

	sqlite3_bind_text (stmt, 1, uri, -1, SQLITE_STATIC);
	sqlite3_bind_int64 (stmt, 2, id->mtime);
	sqlite3_bind_int64 (stmt, 3, id->bytes);
	bind_digest (stmt, 4, id);
	bind_digest (stmt, 5, id);

	return step_id (stmt);
}

/* Some other name with the same size and time. It is a guess, and it is the
 * only one available when nothing has read the file. With a checksum in hand,
 * only a record that has none may be adopted this way - one that has a checksum
 * and matched would already have been found. */
static gint64
file_by_stat (NemoCacheDb *db, const NemoFileId *id)
{
	static const char any[] =
		"SELECT p.file_id FROM paths p JOIN files c ON c.id = p.file_id"
		" WHERE p.mtime = ? AND c.bytes = ? LIMIT 1";
	static const char undigested[] =
		"SELECT p.file_id FROM paths p JOIN files c ON c.id = p.file_id"
		" WHERE p.mtime = ? AND c.bytes = ? AND c.digest IS NULL LIMIT 1";
	sqlite3_stmt *stmt = prep (db, id->has_digest ? undigested : any, "find by size and time");

	if (stmt == NULL)
		return 0;

	sqlite3_bind_int64 (stmt, 1, id->mtime);
	sqlite3_bind_int64 (stmt, 2, id->bytes);

	return step_id (stmt);
}

/* The record for what `id` describes, or 0 if there is none yet. */
static gint64
file_resolve (NemoCacheDb *db, const char *uri, const NemoFileId *id)
{
	gint64 fid;

	fid = file_by_digest (db, id);
	if (fid == 0)
		fid = file_by_uri (db, uri, id);
	if (fid == 0)
		fid = file_by_stat (db, id);

	return fid;
}

static gint64
file_add (NemoCacheDb *db, const NemoFileId *id)
{
	static const char sql[] = "INSERT INTO files (bytes, digest) VALUES (?, ?)";
	sqlite3_stmt *stmt = prep (db, sql, "add file");
	gboolean      ok;

	if (stmt == NULL)
		return 0;

	sqlite3_bind_int64 (stmt, 1, id->bytes);
	bind_digest (stmt, 2, id);

	ok = db_ok (db, sqlite3_step (stmt), "add file");
	sqlite3_finalize (stmt);

	return ok ? sqlite3_last_insert_rowid (db->handle) : 0;
}

/* Fills in a checksum nobody had computed yet. Never replaces one, since two
 * different checksums mean two different files. */
static gboolean
file_learn_digest (NemoCacheDb *db, gint64 file_id, const NemoFileId *id)
{
	static const char sql[] = "UPDATE files SET digest = ? WHERE id = ? AND digest IS NULL";
	sqlite3_stmt *stmt;
	gboolean      ok;

	if (!id->has_digest)
		return TRUE;

	stmt = prep (db, sql, "set checksum");
	if (stmt == NULL)
		return FALSE;

	sqlite3_bind_blob (stmt, 1, id->digest, NEMO_CACHE_DIGEST_LEN, SQLITE_STATIC);
	sqlite3_bind_int64 (stmt, 2, file_id);

	ok = db_ok (db, sqlite3_step (stmt), "set checksum");
	sqlite3_finalize (stmt);

	return ok;
}

/* The record for `id`, made if there is none. */
static gint64
file_for (NemoCacheDb *db, const char *uri, const NemoFileId *id)
{
	gint64 fid = file_resolve (db, uri, id);

	if (fid == 0)
		return file_add (db, id);

	file_learn_digest (db, fid, id);

	return fid;
}

/* Points a uri at a record, so the next lookup goes straight there. */
static gboolean
link_path (NemoCacheDb *db, const char *uri, gint64 file_id, gint64 mtime)
{
	static const char sql[] =
		"INSERT INTO paths (uri, file_id, mtime, seen) VALUES (?, ?, ?, ?)"
		" ON CONFLICT (uri) DO UPDATE SET file_id = excluded.file_id,"
		" mtime = excluded.mtime, seen = excluded.seen";
	sqlite3_stmt *stmt = prep (db, sql, "link");
	gboolean      ok;

	if (stmt == NULL)
		return FALSE;

	sqlite3_bind_text (stmt, 1, uri, -1, SQLITE_STATIC);
	sqlite3_bind_int64 (stmt, 2, file_id);
	sqlite3_bind_int64 (stmt, 3, mtime);
	sqlite3_bind_int64 (stmt, 4, g_get_real_time () / G_USEC_PER_SEC);

	ok = db_ok (db, sqlite3_step (stmt), "link");
	sqlite3_finalize (stmt);

	return ok;
}

/* Two records turned out to be one file. Everything hanging off `from` moves to
 * `to`, and `from` goes. The survivor keeps its own thumbnail if it has one. */
static gboolean
fold_file (NemoCacheDb *db, gint64 from, gint64 to)
{
	static const char take_thumb[] =
		"INSERT OR IGNORE INTO thumbnails"
		" (file_id, size, width, height, format, stored, rendered, renders, image)"
		" SELECT ?, size, width, height, format, stored, rendered, renders, image"
		" FROM thumbnails WHERE file_id = ?";
	static const char move_paths[] = "UPDATE paths SET file_id = ? WHERE file_id = ?";
	static const char drop_old[] = "DELETE FROM files WHERE id = ?";
	sqlite3_stmt *stmt;
	gboolean      ok = TRUE;

	stmt = prep (db, take_thumb, "fold");
	if (stmt == NULL)
		return FALSE;
	sqlite3_bind_int64 (stmt, 1, to);
	sqlite3_bind_int64 (stmt, 2, from);
	ok = db_ok (db, sqlite3_step (stmt), "fold");
	sqlite3_finalize (stmt);
	if (!ok)
		return FALSE;

	stmt = prep (db, move_paths, "fold");
	if (stmt == NULL)
		return FALSE;
	sqlite3_bind_int64 (stmt, 1, to);
	sqlite3_bind_int64 (stmt, 2, from);
	ok = db_ok (db, sqlite3_step (stmt), "fold");
	sqlite3_finalize (stmt);
	if (!ok)
		return FALSE;

	stmt = prep (db, drop_old, "fold");
	if (stmt == NULL)
		return FALSE;
	sqlite3_bind_int64 (stmt, 1, from);
	ok = db_ok (db, sqlite3_step (stmt), "fold");
	sqlite3_finalize (stmt);

	return ok;
}

gboolean
nemo_cache_db_note_file (NemoCacheDb *db, const char *uri, const NemoFileId *id)
{
	gint64   fid;
	gboolean done = FALSE;

	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (id != NULL, FALSE);

	if (db == NULL || db->broken)
		return FALSE;

	g_mutex_lock (&db->lock);

	if (begin (db)) {
		fid = file_for (db, uri, id);
		if (fid != 0 && link_path (db, uri, fid, id->mtime))
			done = commit (db);
		else
			rollback (db);
	}

	g_mutex_unlock (&db->lock);

	return done;
}

gboolean
nemo_cache_db_lookup_digest (NemoCacheDb *db,
			     const char  *uri,
			     gint64       bytes,
			     gint64       mtime,
			     guint8      *digest)
{
	static const char sql[] =
		"SELECT c.digest FROM paths p JOIN files c ON c.id = p.file_id"
		" WHERE p.uri = ? AND p.mtime = ? AND c.bytes = ? AND c.digest IS NOT NULL";
	sqlite3_stmt *stmt;
	gboolean      found = FALSE;

	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (digest != NULL, FALSE);

	if (db == NULL || db->broken)
		return FALSE;

	g_mutex_lock (&db->lock);

	stmt = prep (db, sql, "read checksum");
	if (stmt != NULL) {
		sqlite3_bind_text (stmt, 1, uri, -1, SQLITE_STATIC);
		sqlite3_bind_int64 (stmt, 2, mtime);
		sqlite3_bind_int64 (stmt, 3, bytes);

		if (sqlite3_step (stmt) == SQLITE_ROW) {
			const void *blob = sqlite3_column_blob (stmt, 0);
			int         len = sqlite3_column_bytes (stmt, 0);

			if (blob != NULL && len == NEMO_CACHE_DIGEST_LEN) {
				memcpy (digest, blob, NEMO_CACHE_DIGEST_LEN);
				found = TRUE;
			}
		}

		sqlite3_finalize (stmt);
	}

	g_mutex_unlock (&db->lock);

	return found;
}

gboolean
nemo_cache_db_set_digest (NemoCacheDb *db, const char *uri, const NemoFileId *id)
{
	gint64   target, mine;
	gboolean done = FALSE;

	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (id != NULL, FALSE);
	g_return_val_if_fail (id->has_digest, FALSE);

	if (db == NULL || db->broken)
		return FALSE;

	g_mutex_lock (&db->lock);

	if (!begin (db))
		goto out;

	target = file_by_digest (db, id);
	mine   = file_by_uri (db, uri, id);

	if (mine == 0) {
		/* Nothing known about this path yet, or what was known is stale. */
		if (target == 0)
			target = file_add (db, id);
		done = (target != 0) && link_path (db, uri, target, id->mtime);
	} else if (target == 0 || target == mine) {
		done = file_learn_digest (db, mine, id);
	} else {
		done = fold_file (db, mine, target)
			&& link_path (db, uri, target, id->mtime);
	}

	if (done)
		done = commit (db);
	else
		rollback (db);

out:
	g_mutex_unlock (&db->lock);

	return done;
}

/* Fills `record` and, when asked, the image bytes. Called with the lock held. */
static gboolean
read_thumbnail (NemoCacheDb         *db,
		gint64               file_id,
		NemoThumbnailRecord *record,
		GBytes             **image)
{
	static const char sql[] =
		"SELECT size, width, height, format, stored, image FROM thumbnails"
		" WHERE file_id = ?";
	sqlite3_stmt *stmt = prep (db, sql, "read thumbnail");
	gboolean      found = FALSE;

	if (stmt == NULL)
		return FALSE;

	sqlite3_bind_int64 (stmt, 1, file_id);

	if (sqlite3_step (stmt) == SQLITE_ROW) {
		record->size   = sqlite3_column_int (stmt, 0);
		record->width  = sqlite3_column_int (stmt, 1);
		record->height = sqlite3_column_int (stmt, 2);
		record->format = (NemoThumbnailFormat) sqlite3_column_int (stmt, 3);
		record->stored = sqlite3_column_int64 (stmt, 4);

		if (image != NULL) {
			const void *blob = sqlite3_column_blob (stmt, 5);
			int         len = sqlite3_column_bytes (stmt, 5);

			*image = (blob != NULL && len > 0)
				? g_bytes_new (blob, (gsize) len)
				: NULL;
		}

		found = TRUE;
	}

	sqlite3_finalize (stmt);

	return found;
}

gboolean
nemo_cache_db_thumbnail_lookup (NemoCacheDb         *db,
				const char          *uri,
				const NemoFileId    *id,
				NemoThumbnailRecord *record,
				GBytes             **image)
{
	gint64   fid;
	gboolean known_path;
	gboolean found = FALSE;

	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (id != NULL, FALSE);
	g_return_val_if_fail (record != NULL, FALSE);

	if (image != NULL)
		*image = NULL;

	if (db == NULL || db->broken)
		return FALSE;

	g_mutex_lock (&db->lock);

	fid = file_by_digest (db, id);
	known_path = FALSE;
	if (fid == 0) {
		fid = file_by_uri (db, uri, id);
		known_path = (fid != 0);
	}
	if (fid == 0)
		fid = file_by_stat (db, id);

	if (fid != 0) {
		found = read_thumbnail (db, fid, record, image);

		/* A hit under another name, so point this one at it too and the
		 * next lookup is direct. */
		if (found && !known_path)
			link_path (db, uri, fid, id->mtime);
	}

	g_mutex_unlock (&db->lock);

	return found;
}

gboolean
nemo_cache_db_thumbnail_store (NemoCacheDb               *db,
			       const char                *uri,
			       const NemoFileId          *id,
			       const NemoThumbnailRecord *record,
			       GBytes                    *image)
{
	/* An earlier image for the same contents is overwritten in place rather
	 * than replaced, so its draw history - which is all the pruning rules
	 * have to go on - survives a re-render at a bigger size. */
	static const char sql[] =
		"INSERT INTO thumbnails"
		" (file_id, size, width, height, format, stored, rendered, renders, image)"
		" VALUES (?, ?, ?, ?, ?, ?, 0, 0, ?)"
		" ON CONFLICT (file_id) DO UPDATE SET size = excluded.size,"
		" width = excluded.width, height = excluded.height, format = excluded.format,"
		" stored = excluded.stored, image = excluded.image";
	sqlite3_stmt *stmt;
	gconstpointer data;
	gsize         len = 0;
	gint64        fid;
	gint64        now;
	gboolean      done = FALSE;

	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (id != NULL, FALSE);
	g_return_val_if_fail (record != NULL, FALSE);
	g_return_val_if_fail (image != NULL, FALSE);

	if (db == NULL || db->broken)
		return FALSE;

	data = g_bytes_get_data (image, &len);
	if (data == NULL || len == 0)
		return FALSE;

	now = g_get_real_time () / G_USEC_PER_SEC;

	g_mutex_lock (&db->lock);

	if (!begin (db))
		goto out;

	fid = file_for (db, uri, id);
	if (fid == 0)
		goto fail;

	stmt = prep (db, sql, "store thumbnail");
	if (stmt == NULL)
		goto fail;

	sqlite3_bind_int64 (stmt, 1, fid);
	sqlite3_bind_int (stmt, 2, record->size);
	sqlite3_bind_int (stmt, 3, record->width);
	sqlite3_bind_int (stmt, 4, record->height);
	sqlite3_bind_int (stmt, 5, (int) record->format);
	sqlite3_bind_int64 (stmt, 6, record->stored > 0 ? record->stored : now);
	sqlite3_bind_blob64 (stmt, 7, data, len, SQLITE_STATIC);

	done = db_ok (db, sqlite3_step (stmt), "store thumbnail");
	sqlite3_finalize (stmt);

	if (done)
		done = link_path (db, uri, fid, id->mtime);

	if (done)
		done = commit (db);
	else
		rollback (db);

	goto out;

fail:
	rollback (db);

out:
	g_mutex_unlock (&db->lock);

	return done;
}

/* Every drawn icon in a folder would otherwise be a write, and scrolling a big
 * folder draws the same file over and over. Counts are held in memory and
 * written in one transaction, which is also all the age rule needs - it works
 * in days. Called with the lock held. */
static void
flush_renders (NemoCacheDb *db)
{
	static const char sql[] =
		"UPDATE thumbnails SET rendered = ?, renders = renders + ?"
		" WHERE file_id = (SELECT file_id FROM paths WHERE uri = ?)";
	sqlite3_stmt  *stmt;
	GHashTableIter iter;
	gpointer       uri, count;
	gint64         now;

	if (db->broken || g_hash_table_size (db->pending) == 0)
		return;

	now = g_get_real_time () / G_USEC_PER_SEC;

	if (!begin (db))
		return;

	stmt = prep (db, sql, "draw count");
	if (stmt == NULL) {
		rollback (db);
		return;
	}

	g_hash_table_iter_init (&iter, db->pending);
	while (g_hash_table_iter_next (&iter, &uri, &count)) {
		sqlite3_reset (stmt);
		sqlite3_bind_int64 (stmt, 1, now);
		sqlite3_bind_int64 (stmt, 2, (gint64) GPOINTER_TO_INT (count));
		sqlite3_bind_text (stmt, 3, (const char *) uri, -1, SQLITE_STATIC);
		db_ok (db, sqlite3_step (stmt), "draw count");
	}

	sqlite3_finalize (stmt);

	commit (db);

	g_hash_table_remove_all (db->pending);
}

static gboolean
flush_renders_timeout (gpointer data)
{
	NemoCacheDb *db = data;

	g_mutex_lock (&db->lock);
	db->flush_id = 0;
	flush_renders (db);
	g_mutex_unlock (&db->lock);

	return G_SOURCE_REMOVE;
}

void
nemo_cache_db_note_render (NemoCacheDb *db, const char *uri)
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
nemo_cache_db_flush (NemoCacheDb *db)
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
nemo_cache_db_thumbnail_stats (NemoCacheDb *db,
			       const char  *uri,
			       gint64      *renders,
			       gint64      *rendered)
{
	static const char sql[] =
		"SELECT t.renders, t.rendered FROM paths p"
		" JOIN thumbnails t ON t.file_id = p.file_id WHERE p.uri = ?";
	sqlite3_stmt *stmt;
	gboolean      found = FALSE;

	g_return_val_if_fail (uri != NULL, FALSE);

	if (renders != NULL)
		*renders = 0;
	if (rendered != NULL)
		*rendered = 0;

	if (db == NULL || db->broken)
		return FALSE;

	g_mutex_lock (&db->lock);

	stmt = prep (db, sql, "draw stats");
	if (stmt != NULL) {
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
nemo_cache_db_empty (NemoCacheDb *db)
{
	char    *err = NULL;
	gboolean done;

	if (db == NULL || db->broken)
		return FALSE;

	g_mutex_lock (&db->lock);

	g_hash_table_remove_all (db->pending);

	done = db_ok (db, sqlite3_exec (db->handle,
					"DELETE FROM thumbnails;"
					"DELETE FROM paths;"
					"DELETE FROM files;",
					NULL, NULL, &err), "empty");
	sqlite3_free (err);

	/* Dropping the rows leaves the file its old size, and somebody who just
	 * asked for the cache to be emptied means the disk space too. */
	if (done)
		sqlite3_exec (db->handle, "VACUUM", NULL, NULL, NULL);

	g_mutex_unlock (&db->lock);

	return done;
}

void
nemo_cache_db_usage (NemoCacheDb *db, gint64 *n_thumbnails, gint64 *bytes)
{
	static const char sql[] =
		"SELECT COUNT(*), COALESCE (SUM (LENGTH (image)), 0) FROM thumbnails";
	sqlite3_stmt *stmt;

	if (n_thumbnails != NULL)
		*n_thumbnails = 0;
	if (bytes != NULL)
		*bytes = 0;

	if (db == NULL || db->broken)
		return;

	g_mutex_lock (&db->lock);

	stmt = prep (db, sql, "usage");
	if (stmt != NULL) {
		if (sqlite3_step (stmt) == SQLITE_ROW) {
			if (n_thumbnails != NULL)
				*n_thumbnails = sqlite3_column_int64 (stmt, 0);
			if (bytes != NULL)
				*bytes = sqlite3_column_int64 (stmt, 1);
		}
		sqlite3_finalize (stmt);
	}

	g_mutex_unlock (&db->lock);
}

void
nemo_cache_db_close (void)
{
	NemoCacheDb *db;

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
