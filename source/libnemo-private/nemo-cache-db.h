/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-cache-db.h - what is known about files on disk, kept between runs.

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

/* One SQLite file under the user's cache dir. Thumbnails are the first thing it
 * holds, but not the only kind, so the tables are about files rather than about
 * pictures.
 *
 * Three tables:
 *
 *   files       one row per distinct set of file contents - the size, and the
 *               checksum once anything has bothered to compute one. No image
 *               here, whether the file is a picture or not
 *   paths       one row per uri, pointing at the files row it holds, with its
 *               own mtime. Several paths on one files row means copies, or a
 *               file that moved
 *   thumbnails  at most one per files row, holding the encoded image
 *
 * Splitting paths from files is what lets a file that moved, or a second copy
 * of one, find a thumbnail that is already there instead of rendering it again.
 * It is also the part a future duplicate finder needs: every file it has seen is
 * a files row, and the copies of one are the paths hanging off it. A file with
 * no thumbnail - a text file, an archive - has no thumbnails entry.
 *
 * Everything in the file is rebuildable from the disk, so nothing is migrated.
 * A schema from another version, or a damaged file, is thrown away and made
 * again.
 *
 * Every launch is its own process and several can be open at once, so the file
 * is in WAL mode with a busy timeout. Writes are small and the whole thing is a
 * cache, so synchronous is NORMAL rather than FULL - a power cut can cost the
 * last few rows, which is not worth an fsync apiece.
 */

#ifndef NEMO_CACHE_DB_H
#define NEMO_CACHE_DB_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _NemoCacheDb NemoCacheDb;

/* blake3, which is 256 bits like SHA-256. */
#define NEMO_CACHE_DIGEST_LEN 32

/* What a file is, as far as anything here can tell. Size and mtime are free
 * with the stat every listing does anyway; the checksum costs a read of the
 * whole file, so it is often absent. */
typedef struct {
	gint64   bytes;
	gint64   mtime;		/* microseconds since the epoch */
	guint8   digest[NEMO_CACHE_DIGEST_LEN];
	gboolean has_digest;
} NemoFileId;

/* How a stored thumbnail is encoded. JPEG has no alpha channel, so a source
 * with transparency has to go out as PNG whatever the setting says. */
typedef enum {
	NEMO_THUMBNAIL_FORMAT_JPEG = 0,
	NEMO_THUMBNAIL_FORMAT_PNG  = 1
} NemoThumbnailFormat;

/* A stored thumbnail, minus the image bytes. A width of 0 is a render that
 * failed, with no image behind it. */
typedef struct {
	int                 size;	/* icon size it was rendered for, in pixels */
	int                 width;	/* what the stored image actually is */
	int                 height;
	NemoThumbnailFormat format;
	gint64              stored;	/* when it was rendered, epoch seconds */
} NemoThumbnailRecord;

/* The process-wide store, opened on the first ask. NULL if the cache dir or the
 * database could not be opened at all, which is not fatal - the caller carries
 * on as if every lookup had missed. One attempt is made per open store, so a
 * failure is not retried on every draw.
 *
 * Closing writes out what is pending and folds the journal back in. A later
 * get() opens the store again rather than answering NULL for good. */
NemoCacheDb *nemo_cache_db_get   (void);
void         nemo_cache_db_close (void);

/* Where the database file is, whether or not it opened. Freed by the caller. */
char *nemo_cache_db_path (void);

/* Records what `uri` holds, with no thumbnail. This is how a file nothing can
 * draw gets into the store at all. Calling it again with a different size or
 * time replaces what was known. */
gboolean nemo_cache_db_note_file (NemoCacheDb      *db,
				  const char       *uri,
				  const NemoFileId *id);

/* Reads back a checksum computed earlier, so a file does not have to be read
 * again to know what it is. False unless a checksum is stored for this uri at
 * exactly this size and time. */
gboolean nemo_cache_db_lookup_digest (NemoCacheDb *db,
				      const char  *uri,
				      gint64       bytes,
				      gint64       mtime,
				      guint8      *digest);

/* Attaches a checksum to what `uri` holds. If some other record already carries
 * the same checksum then the two are the same file and are folded into one,
 * which is what makes a moved file keep everything known about it. */
gboolean nemo_cache_db_set_digest (NemoCacheDb      *db,
				   const char       *uri,
				   const NemoFileId *id);

/* Looks for a thumbnail of what `uri` currently holds.
 *
 * On a hit `record` is filled in and `image` gets the encoded bytes, which the
 * caller owns. Pass NULL for `image` to ask only whether one is there and at
 * what size, which is what the draw path wants before it decides to re-render
 * bigger.
 *
 * A uri nothing has stored is still a hit when the same contents are known
 * under another name. Without a checksum in `id`, size and mtime together are
 * all that can be gone on. */
gboolean nemo_cache_db_thumbnail_lookup (NemoCacheDb         *db,
					 const char          *uri,
					 const NemoFileId    *id,
					 NemoThumbnailRecord *record,
					 GBytes             **image);

/* Writes a thumbnail, replacing any earlier one for the same contents and
 * leaving its draw history alone. Safe to call from a worker thread. Returns
 * false if nothing was written.
 *
 * A NULL `image` records that the render failed, so it is not tried again
 * until the file changes. */
gboolean nemo_cache_db_thumbnail_store (NemoCacheDb               *db,
					const char                *uri,
					const NemoFileId          *id,
					const NemoThumbnailRecord *record,
					GBytes                    *image);

/* Drops the thumbnail of what `uri` points at, for a refresh asked for by
 * hand. The record of the file itself stays. */
gboolean nemo_cache_db_thumbnail_forget (NemoCacheDb *db, const char *uri);

/* Counts a draw against whatever `uri` points at and stamps the time. Both are
 * what the pruning rules read, so this is called often and does as little as it
 * can get away with - see the note on batching in the .c file. */
void nemo_cache_db_note_render (NemoCacheDb *db, const char *uri);

/* Writes out the draw counts held in memory. Anything about to read them - a
 * prune, or the settings page - calls this first. */
void nemo_cache_db_flush (NemoCacheDb *db);

/* Draws counted against `uri` and when it was last drawn, epoch seconds. False
 * if there is no thumbnail behind that uri. Either out parameter may be NULL.
 * Counts still held in memory are not included, so flush first if that
 * matters. */
gboolean nemo_cache_db_thumbnail_stats (NemoCacheDb *db,
					const char  *uri,
					gint64      *renders,
					gint64      *rendered);

/* Throws away everything. Returns false if the store could not be emptied. */
gboolean nemo_cache_db_empty (NemoCacheDb *db);

/* Thumbnails held and the bytes they take, for the settings page. Either may be
 * NULL. */
void nemo_cache_db_usage (NemoCacheDb *db, gint64 *n_thumbnails, gint64 *bytes);

G_END_DECLS

#endif /* NEMO_CACHE_DB_H */
