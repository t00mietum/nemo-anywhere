/* The file cache store. What is worth checking here is not that sqlite works
 * but that the rules layered on top of it do: a file that changed stops
 * matching, a file that moved keeps what was known about it, storing twice
 * leaves one image behind, a checksum folds two records into one, and a file
 * nothing can draw is still a record. */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <libnemo-private/nemo-cache-db.h>

#include "test-scratch.h"
#include "test-check.h"

#define SOURCE_BYTES 4096
#define SOURCE_MTIME ((gint64) 1700000000 * G_USEC_PER_SEC)

static GBytes *
fake_image (char fill, gsize len)
{
	char *buf = g_malloc (len);

	memset (buf, fill, len);

	return g_bytes_new_take (buf, len);
}

static NemoFileId
id_for (gint64 bytes, gint64 mtime, const guint8 *digest)
{
	NemoFileId id = { 0 };

	id.bytes = bytes;
	id.mtime = mtime;

	if (digest != NULL) {
		memcpy (id.digest, digest, sizeof (id.digest));
		id.has_digest = TRUE;
	}

	return id;
}

static NemoThumbnailRecord
record_for (int size)
{
	NemoThumbnailRecord rec = { 0 };

	rec.size   = size;
	rec.width  = size;
	rec.height = size;
	rec.format = NEMO_THUMBNAIL_FORMAT_JPEG;
	rec.stored = 1700000001;

	return rec;
}

/* Store one, read it back, and get the same bytes and the same numbers. */
static void
check_round_trip (NemoCacheDb *db)
{
	NemoFileId id = id_for (SOURCE_BYTES, SOURCE_MTIME, NULL);
	NemoThumbnailRecord in = record_for (256);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('a', 512);
	GBytes *back = NULL;

	check (nemo_cache_db_thumbnail_store (db, "file:///one.png", &id, &in, image));

	check (nemo_cache_db_thumbnail_lookup (db, "file:///one.png", &id, &out, &back));
	check (back != NULL);
	check (g_bytes_equal (back, image));
	check (out.size == 256);
	check (out.width == 256 && out.height == 256);
	check (out.format == NEMO_THUMBNAIL_FORMAT_JPEG);
	check (out.stored == 1700000001);

	g_bytes_unref (back);

	/* Asking without wanting the image is how the draw path finds out what
	 * size is stored before it decides to render a bigger one. */
	memset (&out, 0, sizeof (out));
	check (nemo_cache_db_thumbnail_lookup (db, "file:///one.png", &id, &out, NULL));
	check (out.size == 256);
}

/* A file edited under the same name has to miss, or the old picture sticks. */
static void
check_stale_misses (NemoCacheDb *db)
{
	NemoFileId id = id_for (SOURCE_BYTES, SOURCE_MTIME, NULL);
	NemoFileId later = id_for (SOURCE_BYTES, SOURCE_MTIME + G_USEC_PER_SEC, NULL);
	NemoFileId bigger = id_for (SOURCE_BYTES + 1, SOURCE_MTIME, NULL);
	NemoThumbnailRecord in = record_for (256);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('b', 64);

	check (nemo_cache_db_thumbnail_store (db, "file:///edited.png", &id, &in, image));

	/* Same size, later mtime. */
	check (!nemo_cache_db_thumbnail_lookup (db, "file:///edited.png", &later, &out, NULL));

	/* Same mtime, different size. Some editors keep the timestamp. */
	check (!nemo_cache_db_thumbnail_lookup (db, "file:///edited.png", &bigger, &out, NULL));
}

/* A path nobody stored still hits when another path holds a thumbnail of the
 * same contents. This is the whole point of splitting paths from contents. */
static void
check_moved_file_keeps_thumbnail (NemoCacheDb *db)
{
	NemoFileId id = id_for (11111, SOURCE_MTIME, NULL);
	NemoThumbnailRecord in = record_for (128);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('c', 128);
	GBytes *back = NULL;

	check (nemo_cache_db_thumbnail_store (db, "file:///before/pic.jpg", &id, &in, image));

	check (nemo_cache_db_thumbnail_lookup (db, "file:///after/pic.jpg", &id, &out, &back));
	check (back != NULL);
	check (g_bytes_equal (back, image));
	g_bytes_unref (back);

	/* The new path is linked now, so it answers on its own without the
	 * contents having to be searched for again. */
	check (nemo_cache_db_thumbnail_lookup (db, "file:///after/pic.jpg", &id, &out, NULL));

	/* And the old path still works - a copy has two live names. */
	check (nemo_cache_db_thumbnail_lookup (db, "file:///before/pic.jpg", &id, &out, NULL));
}

/* With a checksum, a file that moved AND was restamped is still recognized.
 * Size and mtime alone cannot do that. */
static void
check_digest_finds_restamped_file (NemoCacheDb *db)
{
	guint8 digest[NEMO_CACHE_DIGEST_LEN];
	guint8 other[NEMO_CACHE_DIGEST_LEN];
	NemoFileId id, moved, wrong;
	NemoThumbnailRecord in = record_for (128);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('d', 96);

	memset (digest, 0x7e, sizeof (digest));
	memset (other, 0x01, sizeof (other));

	id    = id_for (22222, SOURCE_MTIME, digest);
	moved = id_for (22222, SOURCE_MTIME + 99 * G_USEC_PER_SEC, digest);
	wrong = id_for (22222, SOURCE_MTIME + 99 * G_USEC_PER_SEC, other);

	check (nemo_cache_db_thumbnail_store (db, "file:///stamped/a.jpg", &id, &in, image));

	/* Different mtime entirely, same contents. */
	check (nemo_cache_db_thumbnail_lookup (db, "file:///stamped/b.jpg", &moved, &out, NULL));

	/* A different checksum at the same size must not match. */
	check (!nemo_cache_db_thumbnail_lookup (db, "file:///stamped/c.jpg", &wrong, &out, NULL));
}

/* Re-storing at a bigger size replaces the image rather than adding one, and
 * every path that pointed at the old one follows it across. */
static void
check_restore_replaces_and_keeps_paths (NemoCacheDb *db)
{
	NemoFileId id = id_for (33333, SOURCE_MTIME, NULL);
	NemoThumbnailRecord small = record_for (128);
	NemoThumbnailRecord big   = record_for (640);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) small_image = fake_image ('e', 64);
	g_autoptr (GBytes) big_image = fake_image ('f', 256);
	GBytes *back = NULL;
	gint64 before = 0, after = 0;

	check (nemo_cache_db_thumbnail_store (db, "file:///grow/one.jpg", &id, &small, small_image));

	/* A second name for the same contents, linked by the content lookup. */
	check (nemo_cache_db_thumbnail_lookup (db, "file:///grow/two.jpg", &id, &out, NULL));

	nemo_cache_db_usage (db, &before, NULL);

	check (nemo_cache_db_thumbnail_store (db, "file:///grow/one.jpg", &id, &big, big_image));

	nemo_cache_db_usage (db, &after, NULL);
	check (after == before);	/* replaced, not added */

	check (nemo_cache_db_thumbnail_lookup (db, "file:///grow/one.jpg", &id, &out, &back));
	check (out.size == 640);
	check (back != NULL && g_bytes_equal (back, big_image));
	g_bytes_unref (back);
	back = NULL;

	/* The other name did not lose its row to the replacement. */
	memset (&out, 0, sizeof (out));
	check (nemo_cache_db_thumbnail_lookup (db, "file:///grow/two.jpg", &id, &out, &back));
	check (out.size == 640);
	check (back != NULL && g_bytes_equal (back, big_image));
	g_bytes_unref (back);
}

/* What the replacement above really has to protect is the draw history. A
 * lookup on its own recovers from a lost row, because the content search finds
 * the image again - so it looks fine until you ask how often the file has been
 * drawn and the count has gone back to zero. The pruning rules read exactly
 * that. */
static void
check_restore_keeps_draw_counts (NemoCacheDb *db)
{
	NemoFileId id = id_for (88888, SOURCE_MTIME, NULL);
	NemoThumbnailRecord small = record_for (128);
	NemoThumbnailRecord big   = record_for (640);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) small_image = fake_image ('k', 64);
	g_autoptr (GBytes) big_image = fake_image ('l', 128);
	gint64 renders = 0;
	int i;

	check (nemo_cache_db_thumbnail_store (db, "file:///hist/one.jpg", &id, &small, small_image));
	check (nemo_cache_db_thumbnail_lookup (db, "file:///hist/two.jpg", &id, &out, NULL));

	for (i = 0; i < 3; i++)
		nemo_cache_db_note_render (db, "file:///hist/two.jpg");

	nemo_cache_db_flush (db);

	check (nemo_cache_db_thumbnail_stats (db, "file:///hist/two.jpg", &renders, NULL));
	check (renders == 3);

	/* Re-render the first name bigger. The history has to live. */
	check (nemo_cache_db_thumbnail_store (db, "file:///hist/one.jpg", &id, &big, big_image));

	renders = 0;
	check (nemo_cache_db_thumbnail_stats (db, "file:///hist/two.jpg", &renders, NULL));
	check (renders == 3);
}

/* A file nothing can draw is still a record, which is what a duplicate finder
 * would work from. Its checksum is written once and read back without the file
 * being touched again. */
static void
check_plain_file_is_recorded (NemoCacheDb *db)
{
	guint8 digest[NEMO_CACHE_DIGEST_LEN];
	guint8 back[NEMO_CACHE_DIGEST_LEN];
	NemoFileId id = id_for (99001, SOURCE_MTIME, NULL);
	NemoFileId with_digest;
	NemoThumbnailRecord out = { 0 };

	memset (digest, 0x33, sizeof (digest));
	with_digest = id_for (99001, SOURCE_MTIME, digest);

	check (nemo_cache_db_note_file (db, "file:///notes/plain.txt", &id));

	/* Recording a file stores no image. */
	check (!nemo_cache_db_thumbnail_lookup (db, "file:///notes/plain.txt", &id, &out, NULL));
	check (!nemo_cache_db_thumbnail_stats (db, "file:///notes/plain.txt", NULL, NULL));

	/* No checksum yet, then one. */
	check (!nemo_cache_db_lookup_digest (db, "file:///notes/plain.txt",
					     99001, SOURCE_MTIME, back));

	check (nemo_cache_db_set_digest (db, "file:///notes/plain.txt", &with_digest));

	memset (back, 0, sizeof (back));
	check (nemo_cache_db_lookup_digest (db, "file:///notes/plain.txt",
					    99001, SOURCE_MTIME, back));
	check (memcmp (back, digest, sizeof (digest)) == 0);

	/* A file that changed under the name has no checksum any more. */
	check (!nemo_cache_db_lookup_digest (db, "file:///notes/plain.txt",
					     99001, SOURCE_MTIME + G_USEC_PER_SEC, back));

	/* A path nobody has recorded can still be given one. */
	check (nemo_cache_db_set_digest (db, "file:///notes/fresh.txt", &with_digest));
	check (nemo_cache_db_lookup_digest (db, "file:///notes/fresh.txt",
					    99001, SOURCE_MTIME, back));
}

/* Two copies of one file that nothing has checksummed look like two unrelated
 * files. Once both are checksummed they are one record, and everything hanging
 * off the record that goes - its image, and every other name for it - has to
 * come across to the one that stays. */
static void
check_digest_folds_records (NemoCacheDb *db)
{
	guint8 digest[NEMO_CACHE_DIGEST_LEN];
	NemoFileId plain = id_for (99002, SOURCE_MTIME, NULL);
	NemoFileId drawn = id_for (99002, SOURCE_MTIME + 5 * G_USEC_PER_SEC, NULL);
	NemoFileId plain_d, drawn_d;
	NemoThumbnailRecord in = record_for (256);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('m', 200);
	gint64 before = 0, after = 0;

	memset (digest, 0x44, sizeof (digest));
	plain_d = id_for (99002, SOURCE_MTIME, digest);
	drawn_d = id_for (99002, SOURCE_MTIME + 5 * G_USEC_PER_SEC, digest);

	/* One copy with nothing but a record, one with an image and two names. */
	check (nemo_cache_db_note_file (db, "file:///dup/a.jpg", &plain));
	check (nemo_cache_db_thumbnail_store (db, "file:///dup/b.jpg", &drawn, &in, image));
	check (nemo_cache_db_thumbnail_lookup (db, "file:///dup/c.jpg", &drawn, &out, NULL));

	/* Same size, different timestamps, so nothing connects them yet. */
	check (!nemo_cache_db_thumbnail_lookup (db, "file:///dup/a.jpg", &plain, &out, NULL));

	nemo_cache_db_usage (db, &before, NULL);

	check (nemo_cache_db_set_digest (db, "file:///dup/a.jpg", &plain_d));
	check (nemo_cache_db_set_digest (db, "file:///dup/b.jpg", &drawn_d));

	/* c.jpg was never named in either call, so its row only survives if the
	 * fold carried it across. Reading the checksum back is the way to ask,
	 * since a thumbnail lookup would put a missing row back and hide it. */
	{
		guint8 back[NEMO_CACHE_DIGEST_LEN];

		check (nemo_cache_db_lookup_digest (db, "file:///dup/c.jpg", 99002,
						    SOURCE_MTIME + 5 * G_USEC_PER_SEC, back));
		check (memcmp (back, digest, sizeof (digest)) == 0);
	}

	/* The record that stays is a.jpg's, the one that had no image. Both of
	 * these go through the checksum, so they only answer if the image really
	 * moved onto it. */
	memset (&out, 0, sizeof (out));
	check (nemo_cache_db_thumbnail_lookup (db, "file:///dup/a.jpg", &plain_d, &out, NULL));
	check (out.size == 256);
	check (nemo_cache_db_thumbnail_lookup (db, "file:///dup/c.jpg", &drawn_d, &out, NULL));
	check (nemo_cache_db_thumbnail_lookup (db, "file:///dup/b.jpg", &drawn_d, &out, NULL));

	/* Moved, not copied. */
	nemo_cache_db_usage (db, &after, NULL);
	check (after == before);
}

/* Emptying really empties, and the store still works afterwards. */
static void
check_empty (NemoCacheDb *db)
{
	NemoFileId id = id_for (44444, SOURCE_MTIME, NULL);
	NemoThumbnailRecord in = record_for (256);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('g', 1024);
	gint64 rows = 0, bytes = 0;

	check (nemo_cache_db_thumbnail_store (db, "file:///gone.png", &id, &in, image));

	nemo_cache_db_usage (db, &rows, &bytes);
	check (rows > 0);
	check (bytes >= 1024);

	check (nemo_cache_db_empty (db));

	nemo_cache_db_usage (db, &rows, &bytes);
	check (rows == 0);
	check (bytes == 0);

	check (!nemo_cache_db_thumbnail_lookup (db, "file:///gone.png", &id, &out, NULL));

	check (nemo_cache_db_thumbnail_store (db, "file:///gone.png", &id, &in, image));
	check (nemo_cache_db_thumbnail_lookup (db, "file:///gone.png", &id, &out, NULL));
	check (nemo_cache_db_empty (db));
}

/* Counting a draw must not lose the thumbnail, whether the count is still in
 * memory or has been written out. */
static void
check_note_render (NemoCacheDb *db)
{
	NemoFileId id = id_for (55555, SOURCE_MTIME, NULL);
	NemoThumbnailRecord in = record_for (256);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('h', 32);
	gint64 renders = 0, rendered = 0;
	int i;

	check (nemo_cache_db_thumbnail_store (db, "file:///drawn.png", &id, &in, image));

	for (i = 0; i < 5; i++)
		nemo_cache_db_note_render (db, "file:///drawn.png");

	check (nemo_cache_db_thumbnail_lookup (db, "file:///drawn.png", &id, &out, NULL));

	/* Counts are batched, so nothing is written until the flush. */
	check (nemo_cache_db_thumbnail_stats (db, "file:///drawn.png", &renders, &rendered));
	check (renders == 0);

	nemo_cache_db_flush (db);

	check (nemo_cache_db_thumbnail_stats (db, "file:///drawn.png", &renders, &rendered));
	check (renders == 5);
	check (rendered > 0);

	/* A uri with no row of its own is counted and thrown away at the flush,
	 * rather than being an error at the call. */
	nemo_cache_db_note_render (db, "file:///never-stored.png");
	nemo_cache_db_flush (db);
	check (!nemo_cache_db_thumbnail_stats (db, "file:///never-stored.png", NULL, NULL));
}

/* A damaged file is rebuilt at open rather than taking the cache down. */
static void
check_corrupt_file_is_rebuilt (void)
{
	g_autofree char *path = NULL;
	NemoCacheDb *db;
	NemoFileId id = id_for (66666, SOURCE_MTIME, NULL);
	NemoThumbnailRecord in = record_for (256);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('i', 48);

	path = nemo_cache_db_path ();
	check (path != NULL);

	nemo_cache_db_close ();

	/* Not a database at all. sqlite notices at the first read. */
	check (g_file_set_contents (path, "this is not a database", -1, NULL));

	db = nemo_cache_db_get ();
	check (db != NULL);
	check (nemo_cache_db_thumbnail_store (db, "file:///after-wipe.png", &id, &in, image));
	check (nemo_cache_db_thumbnail_lookup (db, "file:///after-wipe.png", &id, &out, NULL));
}

/* A file some other version of the schema wrote is thrown away the same way,
 * since nothing in here cannot be worked out again. */
static void
check_wrong_version_is_rebuilt (void)
{
	g_autofree char *path = nemo_cache_db_path ();
	g_autofree char *before = NULL;
	NemoCacheDb *db;
	NemoFileId id = id_for (67777, SOURCE_MTIME, NULL);
	NemoThumbnailRecord in = record_for (256);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('n', 48);
	gsize len = 0;
	FILE *fp;
	/* The schema version sits at offset 60 of the file header, four bytes
	 * big endian. SQLite's format has said so since 3.0. */
	const unsigned char bogus[4] = { 0, 0, 0, 99 };

	check (path != NULL);

	db = nemo_cache_db_get ();
	check (db != NULL);
	check (nemo_cache_db_thumbnail_store (db, "file:///old-schema.png", &id, &in, image));
	nemo_cache_db_close ();

	check (g_file_get_contents (path, &before, &len, NULL));
	check (len > 64);

	fp = g_fopen (path, "r+b");
	check (fp != NULL);
	if (fp == NULL)
		return;
	check (fseek (fp, 60, SEEK_SET) == 0);
	check (fwrite (bogus, 1, sizeof (bogus), fp) == sizeof (bogus));
	fclose (fp);

	db = nemo_cache_db_get ();
	check (db != NULL);

	/* Started over, so what was in it is gone and it still works. */
	check (!nemo_cache_db_thumbnail_lookup (db, "file:///old-schema.png", &id, &out, NULL));
	check (nemo_cache_db_thumbnail_store (db, "file:///old-schema.png", &id, &in, image));
	check (nemo_cache_db_thumbnail_lookup (db, "file:///old-schema.png", &id, &out, NULL));
}

/* Two connections at once, which is what several windows really are. The
 * second one has to see what the first wrote and be able to write itself. */
static void
check_second_process_sees_writes (void)
{
	NemoCacheDb *db = nemo_cache_db_get ();
	NemoFileId id = id_for (77777, SOURCE_MTIME, NULL);
	NemoThumbnailRecord in = record_for (256);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('j', 80);
	g_autofree char *path = nemo_cache_db_path ();
	g_autofree char *quoted = NULL;
	g_autofree char *out_text = NULL;
	int status = 0;

	check (db != NULL);
	check (nemo_cache_db_thumbnail_store (db, "file:///shared.png", &id, &in, image));
	check (nemo_cache_db_thumbnail_lookup (db, "file:///shared.png", &id, &out, NULL));

	{
		g_autofree char *tool = g_find_program_in_path ("sqlite3");

		if (tool == NULL) {
			g_print ("no sqlite3 command, skipping the second-reader check\n");
			return;
		}
	}

	/* A separate process, reading through its own connection while ours is
	 * still open in WAL mode. */
	quoted = g_shell_quote (path);
	{
		g_autofree char *cmd = g_strdup_printf (
			"sqlite3 %s \"SELECT COUNT(*) FROM paths WHERE uri = 'file:///shared.png'\"",
			quoted);

		if (!g_spawn_command_line_sync (cmd, &out_text, NULL, &status, NULL)) {
			g_print ("could not run sqlite3, skipping the second-reader check\n");
			return;
		}
	}

	check (status == 0);
	check (out_text != NULL && g_ascii_strtoll (out_text, NULL, 10) == 1);
}

int
main (int argc, char *argv[])
{
	g_autofree char *scratch = NULL;
	NemoCacheDb *db;

	(void) argc;
	(void) argv;

	/* Before anything reads a cache root, since that answer is cached. */
	scratch = test_scratch_config_home ("nemo-cachedb-XXXXXX");
	if (scratch == NULL) {
		g_printerr ("could not make a scratch dir\n");
		return 77;
	}

	db = nemo_cache_db_get ();
	if (db == NULL) {
		g_printerr ("could not open a file cache in %s\n", scratch);
		return 77;
	}

	check_round_trip (db);
	check_stale_misses (db);
	check_moved_file_keeps_thumbnail (db);
	check_digest_finds_restamped_file (db);
	check_restore_replaces_and_keeps_paths (db);
	check_restore_keeps_draw_counts (db);
	check_plain_file_is_recorded (db);
	check_digest_folds_records (db);
	check_empty (db);
	check_note_render (db);

	check_corrupt_file_is_rebuilt ();
	check_wrong_version_is_rebuilt ();
	check_second_process_sees_writes ();

	nemo_cache_db_close ();

	if (failures > 0) {
		g_printerr ("%d check(s) failed\n", failures);
		return 1;
	}

	g_print ("OK\n");
	return 0;
}
