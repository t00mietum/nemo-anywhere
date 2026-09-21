/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-thumbnail-db.h - the thumbnail cache store.

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

/* One SQLite file under the user's cache dir, holding the thumbnail images
 * themselves rather than a PNG per file in a shared directory.
 *
 * Two tables. `images` is keyed on what a file's contents are - size, mtime and
 * optionally a checksum - and holds the encoded thumbnail. `paths` maps a uri to
 * one of those images and counts how often it has been drawn. The split is what
 * lets a file that moved, or a second copy of one, find a thumbnail that is
 * already there instead of rendering it again.
 *
 * Every launch is its own process and several can be open at once, so the file
 * is in WAL mode with a busy timeout. Writes are small and a cache is
 * rebuildable, so synchronous is NORMAL rather than FULL - a power cut can cost
 * the last few thumbnails, which is not worth an fsync per row.
 */

#ifndef NEMO_THUMBNAIL_DB_H
#define NEMO_THUMBNAIL_DB_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct _NemoThumbnailDb NemoThumbnailDb;

/* How the stored image is encoded. JPEG has no alpha channel, so a source with
 * transparency has to go out as PNG whatever the setting says. */
typedef enum {
	NEMO_THUMBNAIL_FORMAT_JPEG = 0,
	NEMO_THUMBNAIL_FORMAT_PNG  = 1
} NemoThumbnailFormat;

/* Everything about a stored thumbnail except the image bytes. */
typedef struct {
	gint64              bytes;	/* source file size */
	gint64              mtime;	/* source mtime, microseconds since the epoch */
	guint8              digest[32];	/* checksum of the source file */
	gboolean            has_digest;	/* false when none was computed */
	int                 size;	/* icon size it was rendered for, in pixels */
	int                 width;	/* what the stored image actually is */
	int                 height;
	NemoThumbnailFormat format;
	gint64              stored;	/* when it was rendered, epoch seconds */
} NemoThumbnailRecord;

/* The process-wide store, opened on the first ask. NULL if the cache dir or the
 * database could not be opened at all, which is not fatal - the caller renders
 * as if every lookup had missed. One attempt is made per open store, so a
 * failure is not retried on every draw.
 *
 * Closing writes out what is pending and folds the journal back in. A later
 * get() opens the store again rather than answering NULL for good. */
NemoThumbnailDb *nemo_thumbnail_db_get   (void);
void             nemo_thumbnail_db_close (void);

/* Where the database file is, whether or not it opened. Freed by the caller. */
char *nemo_thumbnail_db_path (void);

/* Looks for a thumbnail of `uri` good for `bytes` and `mtime`.
 *
 * On a hit `record` is filled in and `image` gets the encoded bytes, which the
 * caller owns. Pass NULL for `image` to ask only whether one is there and at
 * what size, which is what the draw path wants before it decides to re-render
 * at a bigger size.
 *
 * A uri that is not in the store is still a hit if some other path holds a
 * thumbnail of the same contents - that is how a moved file keeps its
 * thumbnail. `digest` may be NULL, in which case size and mtime alone have to
 * agree.
 */
gboolean nemo_thumbnail_db_lookup (NemoThumbnailDb     *db,
				   const char          *uri,
				   gint64               bytes,
				   gint64               mtime,
				   const guint8        *digest,
				   NemoThumbnailRecord *record,
				   GBytes             **image);

/* Writes a thumbnail, replacing whatever `uri` pointed at before. Safe to call
 * from a worker thread. Returns false if nothing was written. */
gboolean nemo_thumbnail_db_store (NemoThumbnailDb           *db,
				  const char                *uri,
				  const NemoThumbnailRecord *record,
				  GBytes                    *image);

/* Counts a draw against `uri` and stamps the time. Both are what the pruning
 * rules read, so this is called often and does as little as it can get away
 * with - see the note on batching in the .c file. */
void nemo_thumbnail_db_note_render (NemoThumbnailDb *db, const char *uri);

/* Writes out the draw counts held in memory. Anything that is about to read
 * them - a prune, or the settings page - calls this first. */
void nemo_thumbnail_db_flush (NemoThumbnailDb *db);

/* Draws counted against `uri` and when it was last drawn, epoch seconds. False
 * if the uri has no row. Either out parameter may be NULL. Counts still held in
 * memory are not included, so flush first if that matters. */
gboolean nemo_thumbnail_db_path_stats (NemoThumbnailDb *db,
				       const char      *uri,
				       gint64          *renders,
				       gint64          *rendered);

/* Throws away everything. Returns false if the store could not be emptied. */
gboolean nemo_thumbnail_db_empty (NemoThumbnailDb *db);

/* Rows and bytes currently held, for the settings page. Either may be NULL. */
void nemo_thumbnail_db_usage (NemoThumbnailDb *db, gint64 *n_images, gint64 *bytes);

G_END_DECLS

#endif /* NEMO_THUMBNAIL_DB_H */
