/* Edit link: a new target or name for a link, and never a change to what it
 * pointed at before.
 */

#include <config.h>

#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <libnemo-private/nemo-delete-guard.h>
#include <libnemo-private/nemo-link-copy.h>
#include <libnemo-private/nemo-link-edit.h>
#include <libnemo-private/nemo-lnk.h>

#include "test-scratch.h"
#include "test-check.h"

static char *
target_of (const char *path)
{
	GFile *file = g_file_new_for_path (path);
	char *target = NULL;

	if (!nemo_link_read_target (file, &target, NULL)) {
		target = NULL;
	}
	g_object_unref (file);

	return target;
}

static gboolean
reads_as (const char *path, const char *want)
{
	char *contents = NULL;
	gboolean same = g_file_get_contents (path, &contents, NULL, NULL) && g_strcmp0 (contents, want) == 0;

	g_free (contents);
	return same;
}

static void
check_file_links (const char *dir)
{
	char *one = g_build_filename (dir, "one.txt", NULL);
	char *two = g_build_filename (dir, "two.txt", NULL);
	char *link = g_build_filename (dir, "link", NULL);
	char *renamed = g_build_filename (dir, "renamed", NULL);
	char *taken = g_build_filename (dir, "taken", NULL);
	char *target;
	GError *error = NULL;

	check (g_file_set_contents (one, "1", -1, NULL));
	check (g_file_set_contents (two, "2", -1, NULL));
	check (g_file_set_contents (taken, "t", -1, NULL));
	check (nemo_link_create ("one.txt", link, dir, NEMO_LINK_FILE_SYMLINK, NULL));

	/* A new target under the same name, spelled as typed. */
	check (nemo_link_edit_symlink (link, "link", "two.txt", &error));
	g_clear_error (&error);
	target = target_of (link);
	check (g_strcmp0 (target, "two.txt") == 0);
	g_free (target);
	check (reads_as (link, "2"));

	/* A name only. */
	check (nemo_link_edit_symlink (link, "renamed", "two.txt", NULL));
	check (!g_file_test (link, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_SYMLINK));
	target = target_of (renamed);
	check (g_strcmp0 (target, "two.txt") == 0);
	g_free (target);

	/* Both, back again. */
	check (nemo_link_edit_symlink (renamed, "link", "one.txt", NULL));
	check (!g_file_test (renamed, G_FILE_TEST_IS_SYMLINK));
	check (reads_as (link, "1"));

	/* Neither target was touched along the way. */
	check (reads_as (one, "1") && reads_as (two, "2"));

	/* Refused, and the link stays as it was. */
	check (!nemo_link_edit_symlink (link, "taken", "two.txt", &error));
	check (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS));
	g_clear_error (&error);
	check (reads_as (taken, "t"));
	check (!nemo_link_edit_symlink (link, "a/b", "two.txt", &error));
	check (error != NULL);
	g_clear_error (&error);
	check (!nemo_link_edit_symlink (link, "link", "", &error));
	check (error != NULL);
	g_clear_error (&error);
	check (!nemo_link_edit_symlink (one, "one.txt", "two.txt", &error));
	check (error != NULL);
	g_clear_error (&error);
	check (reads_as (link, "1") && reads_as (one, "1"));

	/* A target that is not there yet is allowed, as it is for any symlink. */
	check (nemo_link_edit_symlink (link, "link", "later.txt", NULL));
	target = target_of (link);
	check (g_strcmp0 (target, "later.txt") == 0);
	g_free (target);

	g_remove (link);
	g_free (one);
	g_free (two);
	g_free (link);
	g_free (renamed);
	g_free (taken);
}

/* A folder link, of each kind this machine can make. Whatever was in the
   folder it used to point at is still there. */
static void
check_folder_link (const char *dir, NemoLinkKind kind)
{
	char *first = g_build_filename (dir, "first", NULL);
	char *second = g_build_filename (dir, "second", NULL);
	char *inside = g_build_filename (first, "keep.txt", NULL);
	char *link = g_build_filename (dir, "folder-link", NULL);
	char *moved = g_build_filename (dir, "folder-link-2", NULL);
	char *through = g_build_filename (moved, "keep.txt", NULL);
	GFile *file;

	g_mkdir_with_parents (first, 0700);
	g_mkdir_with_parents (second, 0700);
	check (g_file_set_contents (inside, "k", -1, NULL));
	check (nemo_link_create (first, link, dir, kind, NULL));

	check (nemo_link_edit_symlink (link, "folder-link", second, NULL));
	check (reads_as (inside, "k"));
	check (nemo_link_edit_symlink (link, "folder-link-2", first, NULL));
	check (reads_as (inside, "k"));
	check (reads_as (through, "k"));

	file = g_file_new_for_path (moved);
	check (nemo_link_kind (file, NULL) == kind);
	g_object_unref (file);

#ifdef G_OS_WIN32
	g_rmdir (moved);
#else
	g_remove (moved);
#endif
	g_free (first);
	g_free (second);
	g_free (inside);
	g_free (link);
	g_free (moved);
	g_free (through);
}

static void
check_shortcut (const char *dir)
{
	char *target = g_build_filename (dir, "target.txt", NULL);
	char *lnk = g_build_filename (dir, "short.lnk", NULL);
	char *renamed = g_build_filename (dir, "other.lnk", NULL);
	char *clash = g_build_filename (dir, "clash.lnk", NULL);
	GError *error = NULL;
	NemoLnk read;

	check (g_file_set_contents (target, "x", -1, NULL));
	check (nemo_lnk_write (lnk, target, NEMO_LNK_ALL_PARTS, NULL));
	check (g_file_set_contents (clash, "c", -1, NULL));

	/* ".lnk" goes back on, and the paths are left alone when not asked. */
	check (nemo_link_edit_shortcut (lnk, "other", FALSE, NULL, NULL, NULL, &error));
	g_clear_error (&error);
	check (!g_file_test (lnk, G_FILE_TEST_EXISTS));
	check (nemo_lnk_read (renamed, &read));
	check (read.relative_path != NULL);
	nemo_lnk_clear (&read);

	check (nemo_link_edit_shortcut (renamed, "other.lnk", TRUE, "C:\\x\\y.txt", "", "", NULL));
	check (nemo_lnk_read (renamed, &read));
	check (g_strcmp0 (read.local_path, "C:\\x\\y.txt") == 0 && read.relative_path == NULL);
	nemo_lnk_clear (&read);

	check (!nemo_link_edit_shortcut (renamed, "clash", FALSE, NULL, NULL, NULL, &error));
	check (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS));
	g_clear_error (&error);
	check (reads_as (clash, "c"));

	/* A bad path change leaves the name as it was too. */
	check (!nemo_link_edit_shortcut (renamed, "third", TRUE, "", "", "", &error));
	g_clear_error (&error);
	check (g_file_test (renamed, G_FILE_TEST_EXISTS));

	g_free (target);
	g_free (lnk);
	g_free (renamed);
	g_free (clash);
}

/* The old link is taken away through the guard, which takes nothing else. */
static void
check_guard_takes_only_links (const char *dir)
{
	char *plain = g_build_filename (dir, "plain.txt", NULL);
	char *folder = g_build_filename (dir, "real-folder", NULL);
	GFile *file;
	GError *error = NULL;

	check (g_file_set_contents (plain, "p", -1, NULL));
	g_mkdir_with_parents (folder, 0700);

	file = g_file_new_for_path (plain);
	check (!nemo_delete_guard_remove_link (file, &error));
	check (error != NULL);
	g_clear_error (&error);
	g_object_unref (file);

	file = g_file_new_for_path (folder);
	check (!nemo_delete_guard_remove_link (file, NULL));
	g_object_unref (file);

	check (reads_as (plain, "p"));
	check (g_file_test (folder, G_FILE_TEST_IS_DIR));

	g_free (plain);
	g_free (folder);
}

int
main (int argc, char **argv)
{
	char *dir;
	guint supported;

	g_test_init (&argc, &argv, NULL);

	dir = test_scratch_dir ("nemo-link-edit-XXXXXX", NULL);
	g_assert (dir != NULL);

	check_shortcut (dir);
	check_guard_takes_only_links (dir);

	supported = nemo_link_kinds_supported (dir);
	if (supported & NEMO_LINK_FILE_SYMLINK) {
		check_file_links (dir);
	} else {
		g_printerr ("note: symlinks are not permitted here, so those checks are skipped\n");
	}
	if (supported & NEMO_LINK_DIR_SYMLINK) {
		check_folder_link (dir, NEMO_LINK_DIR_SYMLINK);
	}
	if (supported & NEMO_LINK_JUNCTION) {
		check_folder_link (dir, NEMO_LINK_JUNCTION);
	}

	g_free (dir);

	g_printerr ("%d failure(s)\n", failures);
	return failures == 0 ? 0 : 1;
}
