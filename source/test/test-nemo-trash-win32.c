/* Exercises the Recycle Bin trash:/// backend end to end: enumeration with trash
 * attributes, per-item delete, and restore via move, including metadata-sibling
 * cleanup in the XDG trash wine maps the bin onto. Windows-only; the Linux build
 * compiles it out.
 *
 * The seeded cases need wine, since they write straight into that XDG trash. The
 * trashed-folder case recycles a folder of its own for real, so it runs on either. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32

#include <windows.h>
#include <shellapi.h>

#include <libnemo-private/nemo-trash-win32.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

static char *trash_files_dir;
static char *trash_info_dir;

/* A fixture that quietly failed to be written would make every check after it
 * meaningless, so say so instead. */
static void
write_fixture (const char *path, const char *contents)
{
	if (!g_file_set_contents (path, contents, -1, NULL)) {
		g_printerr ("FAIL could not write %s\n", path);
		failures++;
	}
}

static void
seed_item (const char *name, const char *content)
{
	char *path, *info_path, *info_content;

	path = g_build_filename (trash_files_dir, name, NULL);
	write_fixture (path, content);
	g_free (path);

	info_path = g_strdup_printf ("%s%c%s.trashinfo",
				     trash_info_dir, G_DIR_SEPARATOR, name);
	info_content = g_strdup_printf ("[Trash Info]\nPath=/root/%s\nDeletionDate=2026-07-20T10:00:00\n",
					name);
	write_fixture (info_path, info_content);
	g_free (info_content);
	g_free (info_path);
}

static gboolean
item_seeded_exists (const char *name)
{
	char *path;
	gboolean exists;

	path = g_build_filename (trash_files_dir, name, NULL);
	exists = g_file_test (path, G_FILE_TEST_EXISTS);
	g_free (path);
	return exists;
}

static gboolean
trashinfo_exists (const char *name)
{
	char *path;
	gboolean exists;

	path = g_strdup_printf ("%s%c%s.trashinfo",
				trash_info_dir, G_DIR_SEPARATOR, name);
	exists = g_file_test (path, G_FILE_TEST_EXISTS);
	g_free (path);
	return exists;
}

/* find an item by display name; returns its trash uri or NULL */
static char *
find_item_uri (const char *display_name)
{
	GFile *root;
	GFileEnumerator *enumerator;
	GFileInfo *info;
	char *uri = NULL;

	root = g_file_new_for_uri ("trash:///");
	enumerator = g_file_enumerate_children (root, "standard::*,trash::*",
						0, NULL, NULL);
	if (enumerator != NULL) {
		while (uri == NULL &&
		       (info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
			if (g_strcmp0 (g_file_info_get_display_name (info), display_name) == 0) {
				GFile *child;

				child = g_file_get_child (root, g_file_info_get_name (info));
				uri = g_file_get_uri (child);
				g_object_unref (child);
			}
			g_object_unref (info);
		}
		g_object_unref (enumerator);
	}
	g_object_unref (root);

	return uri;
}

/* Puts a path in the Recycle Bin without any of the shell's dialogs. GLib's
 * g_file_trash leaves the confirmations on, which no unattended run can answer. */
static gboolean
recycle_quietly (const char *path)
{
	SHFILEOPSTRUCTW op;
	wchar_t *wide;
	glong len;
	gboolean ok;

	/* glong, and no cast: g_utf8_to_utf16 writes a 32-bit long here, so through
	   a gsize pointer the top half stayed stack garbage - and that value then
	   sized and indexed the buffer below. */
	wide = (wchar_t *) g_utf8_to_utf16 (path, -1, NULL, &len, NULL);
	if (wide == NULL) {
		return FALSE;
	}

	/* pFrom is a double-NUL terminated list, so it needs one NUL past the string. */
	wide = g_renew (wchar_t, wide, len + 2);
	wide[len] = L'\0';
	wide[len + 1] = L'\0';

	memset (&op, 0, sizeof (op));
	op.wFunc = FO_DELETE;
	op.pFrom = wide;
	op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

	ok = (SHFileOperationW (&op) == 0) && !op.fAnyOperationsAborted;
	g_free (wide);

	return ok;
}

/* Walks a trash location the way the pre-delete count does, returning how many
 * entries it saw. Also checks that each child points back at its parent. */
static int
walk_trash_tree (GFile *dir)
{
	GFileEnumerator *enumerator;
	GFileInfo *info;
	GError *error = NULL;
	int seen = 0;

	enumerator = g_file_enumerate_children (dir,
						G_FILE_ATTRIBUTE_STANDARD_NAME ","
						G_FILE_ATTRIBUTE_STANDARD_TYPE,
						G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
						NULL, &error);
	if (enumerator == NULL) {
		g_printerr ("  listing failed: %s\n", error != NULL ? error->message : "?");
		g_clear_error (&error);
		return -1;
	}

	while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
		GFile *child, *back;

		child = g_file_get_child (dir, g_file_info_get_name (info));
		seen++;

		check (g_file_has_prefix (child, dir));
		back = g_file_get_parent (child);
		check (back != NULL && g_file_equal (back, dir));
		g_clear_object (&back);

		if (g_file_info_get_file_type (info) == G_FILE_TYPE_DIRECTORY) {
			int below = walk_trash_tree (child);

			if (below > 0) {
				seen += below;
			}
		}

		g_object_unref (child);
		g_object_unref (info);
	}

	g_file_enumerator_close (enumerator, NULL, NULL);
	g_object_unref (enumerator);

	return seen;
}

/* --- a trashed folder must list its contents and delete as a unit --- */

static void
test_trashed_folder (void)
{
	char *dir, *base, *nested, *uri;
	GFile *item;
	GError *error = NULL;

	/* Under the home rather than the temp dir - a volume or folder the bin does
	 * not cover would recycle nothing and the case would prove nothing. */
	dir = g_build_filename (g_get_home_dir (), "nemo-trashdir-test", NULL);
	nested = g_build_filename (dir, "inner", NULL);
	g_mkdir_with_parents (nested, 0700);

	base = g_build_filename (dir, "top.txt", NULL);
	write_fixture (base, "top");
	g_free (base);
	base = g_build_filename (nested, "deep.txt", NULL);
	write_fixture (base, "deep");
	g_free (base);
	g_free (nested);

	if (!recycle_quietly (dir)) {
		g_printerr ("SKIP trashed-folder: could not recycle %s\n", dir);
		g_free (dir);
		return;
	}
	g_free (dir);

	uri = find_item_uri ("nemo-trashdir-test");
	check (uri != NULL);

	/* The pre-delete count walks in here, so a trashed folder that refuses to
	 * list stops a permanent delete before it starts. top.txt, inner, deep.txt. */
	if (uri != NULL) {
		item = g_file_new_for_uri (uri);
		check (walk_trash_tree (item) == 3);
		g_object_unref (item);
	}

	/* Every match goes, not just the first - a run that failed here left its
	 * folder in the bin, and inheriting it would fail the next run too.
	 * Without the whole tree coming out this is NOT_EMPTY, which is where both
	 * "Empty Trash" and permanently deleting the folder used to stop. */
	while (uri != NULL) {
		item = g_file_new_for_uri (uri);
		check (g_file_delete (item, NULL, &error));
		if (error != NULL) {
			g_printerr ("  delete said: %s\n", error->message);
			g_clear_error (&error);
			g_object_unref (item);
			g_free (uri);
			return;
		}
		g_object_unref (item);
		g_free (uri);

		uri = find_item_uri ("nemo-trashdir-test");
	}
}

/* meson reports this exit code as SKIP rather than a pass. */
#define TEST_SKIPPED 77

int
main (int argc, char *argv[])
{
	const char *wine_home;
	char *home_root, *uri, *dest_path, *contents;
	GFile *item, *dest;
	GFileInfo *info;

	nemo_trash_win32_register ();
	test_trashed_folder ();

	/* The seeded cases plant files into wine's unix-style XDG trash layout, which
	 * does not exist on real Windows - so WINEHOMEDIR being unset is exactly the
	 * case this test cannot cover. Report that as a skip (meson's 77), not as a
	 * pass: reading green there is what let the recycle-bin divergences through. */
	wine_home = g_getenv ("WINEHOMEDIR");
	if (wine_home == NULL) {
		g_printerr ("SKIP seeded cases: need wine (WINEHOMEDIR unset)\n");
		if (failures != 0) {
			return EXIT_FAILURE;
		}
		return TEST_SKIPPED;
	}
	if (g_str_has_prefix (wine_home, "\\??\\")) {
		wine_home += 4;
	}
	home_root = g_strdup (wine_home);

	trash_files_dir = g_strdup_printf ("%s\\.local\\share\\Trash\\files", home_root);
	trash_info_dir = g_strdup_printf ("%s\\.local\\share\\Trash\\info", home_root);
	g_mkdir_with_parents (trash_files_dir, 0700);
	g_mkdir_with_parents (trash_info_dir, 0700);

	check (g_file_test (trash_files_dir, G_FILE_TEST_IS_DIR));

	/* --- enumerate: seeded item appears with trash attrs --- */
	seed_item ("trashtest-enum.txt", "enum");
	uri = find_item_uri ("trashtest-enum.txt");
	check (uri != NULL);

	if (uri != NULL) {
		item = g_file_new_for_uri (uri);
		info = g_file_query_info (item, "standard::*,trash::*", 0, NULL, NULL);
		check (info != NULL);
		if (info != NULL) {
			check (g_strcmp0 (g_file_info_get_display_name (info), "trashtest-enum.txt") == 0);
			check (g_file_info_get_attribute_string (info, "trash::deletion-date") != NULL);
			g_object_unref (info);
		}

		/* --- per-item delete removes backing file + trashinfo --- */
		check (g_file_delete (item, NULL, NULL));
		check (!item_seeded_exists ("trashtest-enum.txt"));
		check (!trashinfo_exists ("trashtest-enum.txt"));
		g_object_unref (item);
		g_free (uri);
	}

	/* --- restore: move out of the bin to a real location --- */
	seed_item ("trashtest-restore.txt", "restore me");
	uri = find_item_uri ("trashtest-restore.txt");
	check (uri != NULL);

	if (uri != NULL) {
		dest_path = g_strdup_printf ("%s\\trashtest-restored.txt", home_root);
		g_unlink (dest_path);

		item = g_file_new_for_uri (uri);
		dest = g_file_new_for_path (dest_path);
		check (g_file_move (item, dest, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, NULL));

		contents = NULL;
		check (g_file_get_contents (dest_path, &contents, NULL, NULL));
		check (g_strcmp0 (contents, "restore me") == 0);
		g_free (contents);

		check (!item_seeded_exists ("trashtest-restore.txt"));
		check (!trashinfo_exists ("trashtest-restore.txt"));

		g_unlink (dest_path);
		g_free (dest_path);
		g_object_unref (dest);
		g_object_unref (item);
		g_free (uri);
	}

	if (failures == 0) {
		g_print ("trash-win32: all checks passed\n");
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
