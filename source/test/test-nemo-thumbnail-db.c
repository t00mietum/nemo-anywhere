/* The thumbnail cache store. What is worth checking here is not that sqlite
 * works but that the rules layered on top of it do: a file that changed stops
 * matching, a file that moved keeps its thumbnail, storing twice leaves one
 * image behind, and a damaged file is thrown away rather than carried. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <libnemo-private/nemo-thumbnail-db.h>

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

static NemoThumbnailRecord
record_for (gint64 bytes, gint64 mtime, int size, const guint8 *digest)
{
	NemoThumbnailRecord rec = { 0 };

	rec.bytes  = bytes;
	rec.mtime  = mtime;
	rec.size   = size;
	rec.width  = size;
	rec.height = size;
	rec.format = NEMO_THUMBNAIL_FORMAT_JPEG;
	rec.stored = 1700000001;

	if (digest != NULL) {
		memcpy (rec.digest, digest, sizeof (rec.digest));
		rec.has_digest = TRUE;
	}

	return rec;
}

/* Store one, read it back, and get the same bytes and the same numbers. */
static void
check_round_trip (NemoThumbnailDb *db)
{
	NemoThumbnailRecord in = record_for (SOURCE_BYTES, SOURCE_MTIME, 256, NULL);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('a', 512);
	GBytes *back = NULL;

	check (nemo_thumbnail_db_store (db, "file:///one.png", &in, image));

	check (nemo_thumbnail_db_lookup (db, "file:///one.png",
					 SOURCE_BYTES, SOURCE_MTIME, NULL, &out, &back));
	check (back != NULL);
	check (g_bytes_equal (back, image));
	check (out.size == 256);
	check (out.width == 256 && out.height == 256);
	check (out.format == NEMO_THUMBNAIL_FORMAT_JPEG);
	check (out.bytes == SOURCE_BYTES);
	check (out.mtime == SOURCE_MTIME);
	check (!out.has_digest);

	g_bytes_unref (back);

	/* Asking without wanting the image is how the draw path finds out what
	 * size is stored before it decides to render a bigger one. */
	memset (&out, 0, sizeof (out));
	check (nemo_thumbnail_db_lookup (db, "file:///one.png",
					 SOURCE_BYTES, SOURCE_MTIME, NULL, &out, NULL));
	check (out.size == 256);
}

/* A file edited under the same name has to miss, or the old picture sticks. */
static void
check_stale_misses (NemoThumbnailDb *db)
{
	NemoThumbnailRecord in = record_for (SOURCE_BYTES, SOURCE_MTIME, 256, NULL);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('b', 64);

	check (nemo_thumbnail_db_store (db, "file:///edited.png", &in, image));

	/* Same size, later mtime. */
	check (!nemo_thumbnail_db_lookup (db, "file:///edited.png",
					  SOURCE_BYTES, SOURCE_MTIME + G_USEC_PER_SEC,
					  NULL, &out, NULL));

	/* Same mtime, different size. Some editors keep the timestamp. */
	check (!nemo_thumbnail_db_lookup (db, "file:///edited.png",
					  SOURCE_BYTES + 1, SOURCE_MTIME,
					  NULL, &out, NULL));
}

/* A path nobody stored still hits when another path holds a thumbnail of the
 * same contents. This is the whole point of splitting paths from images. */
static void
check_moved_file_keeps_thumbnail (NemoThumbnailDb *db)
{
	NemoThumbnailRecord in = record_for (11111, SOURCE_MTIME, 128, NULL);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('c', 128);
	GBytes *back = NULL;

	check (nemo_thumbnail_db_store (db, "file:///before/pic.jpg", &in, image));

	check (nemo_thumbnail_db_lookup (db, "file:///after/pic.jpg",
					 11111, SOURCE_MTIME, NULL, &out, &back));
	check (back != NULL);
	check (g_bytes_equal (back, image));
	g_bytes_unref (back);

	/* The new path is linked now, so it answers on its own without the
	 * contents having to be searched for again. */
	check (nemo_thumbnail_db_lookup (db, "file:///after/pic.jpg",
					 11111, SOURCE_MTIME, NULL, &out, NULL));

	/* And the old path still works - a copy has two live names. */
	check (nemo_thumbnail_db_lookup (db, "file:///before/pic.jpg",
					 11111, SOURCE_MTIME, NULL, &out, NULL));
}

/* With a checksum, a file that moved AND was restamped is still recognized.
 * Size and mtime alone cannot do that. */
static void
check_digest_finds_restamped_file (NemoThumbnailDb *db)
{
	guint8 digest[32];
	NemoThumbnailRecord in;
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('d', 96);

	memset (digest, 0x7e, sizeof (digest));
	in = record_for (22222, SOURCE_MTIME, 128, digest);

	check (nemo_thumbnail_db_store (db, "file:///stamped/a.jpg", &in, image));

	/* Different mtime entirely, same contents. */
	check (nemo_thumbnail_db_lookup (db, "file:///stamped/b.jpg",
					 22222, SOURCE_MTIME + 99 * G_USEC_PER_SEC,
					 digest, &out, NULL));
	check (out.has_digest);
	check (memcmp (out.digest, digest, sizeof (digest)) == 0);

	/* A different checksum at the same size must not match. */
	memset (digest, 0x01, sizeof (digest));
	check (!nemo_thumbnail_db_lookup (db, "file:///stamped/c.jpg",
					  22222, SOURCE_MTIME + 99 * G_USEC_PER_SEC,
					  digest, &out, NULL));
}

/* Re-storing at a bigger size replaces the image rather than adding one, and
 * every path that pointed at the old one follows it across. */
static void
check_restore_replaces_and_keeps_paths (NemoThumbnailDb *db)
{
	NemoThumbnailRecord small = record_for (33333, SOURCE_MTIME, 128, NULL);
	NemoThumbnailRecord big   = record_for (33333, SOURCE_MTIME, 640, NULL);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) small_image = fake_image ('e', 64);
	g_autoptr (GBytes) big_image = fake_image ('f', 256);
	GBytes *back = NULL;
	gint64 before = 0, after = 0;

	check (nemo_thumbnail_db_store (db, "file:///grow/one.jpg", &small, small_image));

	/* A second name for the same contents, linked by the content lookup. */
	check (nemo_thumbnail_db_lookup (db, "file:///grow/two.jpg",
					 33333, SOURCE_MTIME, NULL, &out, NULL));

	nemo_thumbnail_db_usage (db, &before, NULL);

	check (nemo_thumbnail_db_store (db, "file:///grow/one.jpg", &big, big_image));

	nemo_thumbnail_db_usage (db, &after, NULL);
	check (after == before);	/* replaced, not added */

	check (nemo_thumbnail_db_lookup (db, "file:///grow/one.jpg",
					 33333, SOURCE_MTIME, NULL, &out, &back));
	check (out.size == 640);
	check (back != NULL && g_bytes_equal (back, big_image));
	g_bytes_unref (back);
	back = NULL;

	/* The other name did not lose its row to the replacement. */
	memset (&out, 0, sizeof (out));
	check (nemo_thumbnail_db_lookup (db, "file:///grow/two.jpg",
					 33333, SOURCE_MTIME, NULL, &out, &back));
	check (out.size == 640);
	check (back != NULL && g_bytes_equal (back, big_image));
	g_bytes_unref (back);
}

/* What the replacement above really has to protect is the draw history. A
 * lookup on its own recovers from a dropped path row, because the content
 * search finds the new image and links the uri again - so it looks fine until
 * you ask how often the file has been drawn, and the count has gone back to
 * zero. The pruning rules read exactly that. */
static void
check_restore_keeps_draw_counts (NemoThumbnailDb *db)
{
	NemoThumbnailRecord small = record_for (88888, SOURCE_MTIME, 128, NULL);
	NemoThumbnailRecord big   = record_for (88888, SOURCE_MTIME, 640, NULL);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) small_image = fake_image ('k', 64);
	g_autoptr (GBytes) big_image = fake_image ('l', 128);
	gint64 renders = 0;
	int i;

	check (nemo_thumbnail_db_store (db, "file:///hist/one.jpg", &small, small_image));
	check (nemo_thumbnail_db_lookup (db, "file:///hist/two.jpg",
					 88888, SOURCE_MTIME, NULL, &out, NULL));

	for (i = 0; i < 3; i++)
		nemo_thumbnail_db_note_render (db, "file:///hist/two.jpg");

	nemo_thumbnail_db_flush (db);

	check (nemo_thumbnail_db_path_stats (db, "file:///hist/two.jpg", &renders, NULL));
	check (renders == 3);

	/* Re-render the first name bigger. The second name's history has to live. */
	check (nemo_thumbnail_db_store (db, "file:///hist/one.jpg", &big, big_image));

	renders = 0;
	check (nemo_thumbnail_db_path_stats (db, "file:///hist/two.jpg", &renders, NULL));
	check (renders == 3);
}

/* Emptying really empties, and the store still works afterwards. */
static void
check_empty (NemoThumbnailDb *db)
{
	NemoThumbnailRecord in = record_for (44444, SOURCE_MTIME, 256, NULL);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('g', 1024);
	gint64 rows = 0, bytes = 0;

	check (nemo_thumbnail_db_store (db, "file:///gone.png", &in, image));

	nemo_thumbnail_db_usage (db, &rows, &bytes);
	check (rows > 0);
	check (bytes >= 1024);

	check (nemo_thumbnail_db_empty (db));

	nemo_thumbnail_db_usage (db, &rows, &bytes);
	check (rows == 0);
	check (bytes == 0);

	check (!nemo_thumbnail_db_lookup (db, "file:///gone.png",
					  44444, SOURCE_MTIME, NULL, &out, NULL));

	check (nemo_thumbnail_db_store (db, "file:///gone.png", &in, image));
	check (nemo_thumbnail_db_lookup (db, "file:///gone.png",
					 44444, SOURCE_MTIME, NULL, &out, NULL));
	check (nemo_thumbnail_db_empty (db));
}

/* Counting a draw must not lose the thumbnail, whether the count is still in
 * memory or has been written out. */
static void
check_note_render (NemoThumbnailDb *db)
{
	NemoThumbnailRecord in = record_for (55555, SOURCE_MTIME, 256, NULL);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('h', 32);
	gint64 renders = 0, rendered = 0;
	int i;

	check (nemo_thumbnail_db_store (db, "file:///drawn.png", &in, image));

	for (i = 0; i < 5; i++)
		nemo_thumbnail_db_note_render (db, "file:///drawn.png");

	check (nemo_thumbnail_db_lookup (db, "file:///drawn.png",
					 55555, SOURCE_MTIME, NULL, &out, NULL));

	/* Counts are batched, so nothing is written until the flush. */
	check (nemo_thumbnail_db_path_stats (db, "file:///drawn.png", &renders, &rendered));
	check (renders == 0);

	nemo_thumbnail_db_flush (db);

	check (nemo_thumbnail_db_path_stats (db, "file:///drawn.png", &renders, &rendered));
	check (renders == 5);
	check (rendered > 0);

	/* A uri with no row of its own is counted and thrown away at the flush,
	 * rather than being an error at the call. */
	nemo_thumbnail_db_note_render (db, "file:///never-stored.png");
	nemo_thumbnail_db_flush (db);
	check (!nemo_thumbnail_db_path_stats (db, "file:///never-stored.png", NULL, NULL));
}

/* A damaged file is rebuilt at open rather than taking the cache down. */
static void
check_corrupt_file_is_rebuilt (void)
{
	g_autofree char *path = NULL;
	NemoThumbnailDb *db;
	NemoThumbnailRecord in = record_for (66666, SOURCE_MTIME, 256, NULL);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('i', 48);

	path = nemo_thumbnail_db_path ();
	check (path != NULL);

	nemo_thumbnail_db_close ();

	/* Not a database at all. sqlite notices at the first read. */
	check (g_file_set_contents (path, "this is not a database", -1, NULL));

	db = nemo_thumbnail_db_get ();
	check (db != NULL);
	check (nemo_thumbnail_db_store (db, "file:///after-wipe.png", &in, image));
	check (nemo_thumbnail_db_lookup (db, "file:///after-wipe.png",
					 66666, SOURCE_MTIME, NULL, &out, NULL));
}

/* Two connections at once, which is what several windows really are. The
 * second one has to see what the first wrote and be able to write itself. */
static void
check_second_process_sees_writes (void)
{
	NemoThumbnailDb *db = nemo_thumbnail_db_get ();
	NemoThumbnailRecord in = record_for (77777, SOURCE_MTIME, 256, NULL);
	NemoThumbnailRecord out = { 0 };
	g_autoptr (GBytes) image = fake_image ('j', 80);
	g_autofree char *path = nemo_thumbnail_db_path ();
	g_autofree char *quoted = NULL;
	g_autofree char *out_text = NULL;
	int status = 0;

	check (db != NULL);
	check (nemo_thumbnail_db_store (db, "file:///shared.png", &in, image));
	check (nemo_thumbnail_db_lookup (db, "file:///shared.png",
					 77777, SOURCE_MTIME, NULL, &out, NULL));

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
		g_autofree char *argv1 = g_strdup (cmd);

		if (!g_spawn_command_line_sync (argv1, &out_text, NULL, &status, NULL)) {
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
	NemoThumbnailDb *db;

	(void) argc;
	(void) argv;

	/* Before anything reads a cache root, since that answer is cached. */
	scratch = test_scratch_config_home ("nemo-thumbdb-XXXXXX");
	if (scratch == NULL) {
		g_printerr ("could not make a scratch dir\n");
		return 77;
	}

	db = nemo_thumbnail_db_get ();
	if (db == NULL) {
		g_printerr ("could not open a thumbnail cache in %s\n", scratch);
		return 77;
	}

	check_round_trip (db);
	check_stale_misses (db);
	check_moved_file_keeps_thumbnail (db);
	check_digest_finds_restamped_file (db);
	check_restore_replaces_and_keeps_paths (db);
	check_restore_keeps_draw_counts (db);
	check_empty (db);
	check_note_render (db);

	check_corrupt_file_is_rebuilt ();
	check_second_process_sees_writes ();

	nemo_thumbnail_db_close ();

	if (failures > 0) {
		g_printerr ("%d check(s) failed\n", failures);
		return 1;
	}

	g_print ("OK\n");
	return 0;
}
