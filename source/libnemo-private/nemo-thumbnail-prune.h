/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-thumbnail-prune.h - keeping the thumbnail cache from growing forever.

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

/* Nothing ever removed a thumbnail, so the cache only grew. It is swept now,
 * well after startup and on a worker thread, at most once a day.
 *
 * Three rules, in order: a thumbnail whose file is gone goes first, then
 * anything not used for a long time, then oldest-first until the rest fit in
 * the size allowed. Both limits are settings, and either can be turned off.
 *
 * On Linux the cache is the shared one every file manager and image viewer
 * uses, so the defaults match what the desktop's own housekeeping already
 * does - 180 days and 512 MB - and a box that has that running sees no change.
 * Elsewhere nothing else touches the folder and nothing else was cleaning it.
 */

#ifndef NEMO_THUMBNAIL_PRUNE_H
#define NEMO_THUMBNAIL_PRUNE_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	char     *path;		/* the thumbnail file itself */
	gint64    size;		/* bytes it takes up */
	gint64    used;		/* when it was last read or written, epoch seconds */
	gboolean  orphan;	/* the file it was made for is gone */
	gboolean  drop;		/* filled in by the planner */
} NemoThumbnailPruneEntry;

/* Sets `drop` on everything that should go and returns the bytes that frees.
 * `budget_bytes` and `max_age_secs` are each ignored when zero or less. */
gint64 nemo_thumbnail_prune_plan (NemoThumbnailPruneEntry *entries,
				  guint                    n_entries,
				  gint64                   budget_bytes,
				  gint64                   max_age_secs,
				  gint64                   now);

/* Reads one tEXt value out of the front of a PNG. The thumbnail spec keeps the
 * file it was made from in `Thumb::URI`, and the sweep asks that of every file
 * in the cache, so it is read straight out of the header rather than by
 * decoding the image. Returns NULL when the key is not there. */
char *nemo_thumbnail_png_text (const guchar *data,
			       gsize         len,
			       const char   *key);

/* Sweeps `cache_dir` (a `thumbnails` folder) once, on the calling thread, and
 * returns the bytes freed. Exposed for the regression check; the app goes
 * through the scheduler below. */
gint64 nemo_thumbnail_prune_sweep (const char *cache_dir,
				   const char *fail_appname,
				   gint64      budget_bytes,
				   gint64      max_age_secs,
				   gint64      now);

/* Notes that a thumbnail was just used, so the sweep can tell a cache that is
 * being read from one that is only being written to. Cheap: it writes at most
 * once a day per file, and does nothing where the filesystem is already
 * keeping a usable access time. */
void nemo_thumbnail_prune_note_use (const char *thumbnail_path);

/* Arranges for one sweep this session, a minute after startup and only if a
 * day has passed since the last one. Safe to call from every process. */
void nemo_thumbnail_prune_schedule (void);

G_END_DECLS

#endif /* NEMO_THUMBNAIL_PRUNE_H */
