/* A refresh that changes nothing must report nothing changed.
 *
 * On Windows gio hands every file the same flat icon, so nemo swaps in a themed
 * one derived from the type. The changed-check used to compare the incoming gio
 * icon against the themed one already stored, which never matches - so every
 * file in the folder reported as changed on every single refresh, and each one
 * dragged a re-sort, a redraw and a thumbnail re-check behind it.
 *
 * nemo_file_update_info is documented to "return FALSE if no change", so that
 * contract is what is checked here: feed the same freshly-queried info twice and
 * the second call must be quiet. Windows-only; Linux compiles it out. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#ifdef G_OS_WIN32

#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-private.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

/* Everything nemo reads off a file during a normal folder refresh. Asking for
 * less would let a churning attribute hide behind an attribute never fetched. */
#define WANTED \
	"standard::*,access::*,mountable::*,time::*,unix::*,owner::*," \
	"selinux::*,thumbnail::*,id::filesystem,trash::orig-path,trash::deletion-date"

static GFileInfo *
fresh_info (const char *path)
{
	GFile *location = g_file_new_for_path (path);
	GFileInfo *info = g_file_query_info (location, WANTED,
					     G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					     NULL, NULL);

	g_object_unref (location);
	return info;
}

/* Refresh @path @rounds times over and count how many said "changed". */
static int
churn (const char *path, int rounds)
{
	GFile *location = g_file_new_for_path (path);
	NemoFile *file = nemo_file_get (location);
	int changes = 0;
	int i;

	g_object_unref (location);

	if (file == NULL) {
		g_printerr ("  no NemoFile for %s\n", path);
		return -1;
	}

	for (i = 0; i < rounds; i++) {
		GFileInfo *info = fresh_info (path);

		if (info == NULL) {
			g_printerr ("  could not query %s\n", path);
			nemo_file_unref (file);
			return -1;
		}

		if (nemo_file_update_info (file, info)) {
			changes++;
		}
		g_object_unref (info);
	}

	nemo_file_unref (file);
	return changes;
}

static void
write_file (const char *path, const char *contents)
{
	GError *error = NULL;

	if (!g_file_set_contents (path, contents, -1, &error)) {
		g_printerr ("  could not write %s: %s\n", path, error->message);
		g_clear_error (&error);
	}
}

int
main (int argc, char *argv[])
{
	char *dir;
	/* One per icon family, because the themed icon is derived from the type
	 * and a single type could pass by accident. */
	const char *names[] = { "note.txt", "shot.png", "tune.mp3", "sheet.csv", "blob.zzz" };
	char *paths[G_N_ELEMENTS (names)];
	guint i;

	/* Only so the icon theme nemo-file hooks on startup has a screen to hang
	 * off. Nothing here needs a window, and a box with no display still runs
	 * the checks - it just grumbles on the way in. */
	gtk_init_check (&argc, &argv);

	dir = g_dir_make_tmp ("nemo-churn-XXXXXX", NULL);
	g_assert (dir != NULL);

	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		paths[i] = g_build_filename (dir, names[i], NULL);
		write_file (paths[i], "x");
	}

	/* Five refreshes. The first legitimately reports a change - it is the
	 * first time the file has been seen - and the rest must be silent. */
	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		int changes = churn (paths[i], 5);

		if (changes != 1) {
			g_printerr ("  %s: %d of 5 refreshes reported a change\n",
				    names[i], changes);
		}
		check (changes == 1);
	}

	/* A directory too: it takes the other branch of the icon override, the
	 * one that leaves gio's icon alone. */
	{
		char *sub = g_build_filename (dir, "subdir", NULL);
		int changes;

		g_mkdir (sub, 0700);
		changes = churn (sub, 5);
		if (changes != 1) {
			g_printerr ("  subdir: %d of 5 refreshes reported a change\n", changes);
		}
		check (changes == 1);

		g_rmdir (sub);
		g_free (sub);
	}

	/* A real change must still come through, or the check above could be
	 * satisfied by a file that reports nothing ever. */
	{
		char *path = g_build_filename (dir, "grows.txt", NULL);
		GFile *location;
		NemoFile *file;
		GFileInfo *info;

		write_file (path, "small");
		location = g_file_new_for_path (path);
		file = nemo_file_get (location);
		g_object_unref (location);

		info = fresh_info (path);
		check (info != NULL);
		if (info != NULL) {
			nemo_file_update_info (file, info);   /* first sighting */
			g_object_unref (info);
		}

		info = fresh_info (path);
		check (info != NULL && !nemo_file_update_info (file, info));
		g_clear_object (&info);

		write_file (path, "very much bigger than it was before");

		info = fresh_info (path);
		check (info != NULL && nemo_file_update_info (file, info));
		g_clear_object (&info);

		nemo_file_unref (file);
		g_unlink (path);
		g_free (path);
	}

	for (i = 0; i < G_N_ELEMENTS (names); i++) {
		g_unlink (paths[i]);
		g_free (paths[i]);
	}
	g_rmdir (dir);
	g_free (dir);

	if (failures == 0) {
		g_print ("file-win32-churn: all checks passed\n");
	}
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#else /* !G_OS_WIN32 */

int
main (int argc, char *argv[])
{
	return EXIT_SUCCESS;
}

#endif
