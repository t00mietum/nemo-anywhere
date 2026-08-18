/* Exercises the Recycle Bin trash:/// backend end to end: enumeration with trash
 * attributes, per-item delete, and restore via move, including metadata-sibling
 * cleanup in the XDG trash wine maps the bin onto. Windows-only; the Linux build
 * compiles it out.
 *
 * The seeded cases need wine, since they write straight into that XDG trash. The
 * rest recycle files of their own through the shell, so they run on either, and a
 * native run is where the recycle-bin behaviour is actually observable: a listed
 * name that keeps its extension, an original location worth restoring to, a 64-bit
 * deletion timestamp, and a uri from outside the bin being refused. */

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

/* Cleared only when the shell bin itself could not be worked, so a run that
 * proved nothing is reported as a skip rather than a pass. */
static gboolean real_bin_ran = FALSE;

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

/* An item's name is its escaped backing path, so a uri can be built for any path
 * at all - which is the point of the containment case below. */
#define TRASH_ROOT_URI "trash:///"

static char *
uri_for_real_path (const char *real_path)
{
	char *escaped, *uri;

	escaped = g_uri_escape_string (real_path, NULL, TRUE);
	uri = g_strconcat (TRASH_ROOT_URI, escaped, NULL);
	g_free (escaped);

	return uri;
}

static char *
real_path_for_uri (const char *uri)
{
	return g_uri_unescape_string (uri + strlen (TRASH_ROOT_URI), NULL);
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

/* --- a real bin item: listed name, original location and date survive a restore ---
 *
 * The seeded cases further down plant files into wine's XDG trash, so on real
 * Windows this is the only coverage listing, trash attributes and restore get.
 * The fixture name carries a dot before its extension deliberately: that is the
 * shape that breaks when a shortened name is repaired by looking for a dot. */
static void
test_real_bin_roundtrip (void)
{
	char *dir, *orig, *uri, *listed_path, *contents;
	const char *orig_path, *date;
	GFile *item, *dest;
	GFileInfo *info;
	GError *error = NULL;

	/* Under the home, not the temp dir: a volume the bin does not cover would
	 * recycle nothing and the case would prove nothing. */
	dir = g_build_filename (g_get_home_dir (), "nemoverify-trash", NULL);
	g_mkdir_with_parents (dir, 0700);
	orig = g_build_filename (dir, "report.2026.txt", NULL);
	write_fixture (orig, "round trip");

	/* An earlier run that died mid-case would otherwise be found instead. */
	while ((uri = find_item_uri ("report.2026.txt")) != NULL) {
		item = g_file_new_for_uri (uri);
		g_file_delete (item, NULL, NULL);
		g_object_unref (item);
		g_free (uri);
	}

	if (!recycle_quietly (orig)) {
		g_printerr ("SKIP real-bin round trip: could not recycle %s\n", orig);
		g_free (orig);
		g_free (dir);
		return;
	}
	real_bin_ran = TRUE;

	/* Found by its full name at all is the first half of the claim: the shell's
	 * in-folder name is what the listing shows and what a restore writes. */
	uri = find_item_uri ("report.2026.txt");
	check (uri != NULL);
	if (uri == NULL) {
		g_free (orig);
		g_free (dir);
		return;
	}

	item = g_file_new_for_uri (uri);
	info = g_file_query_info (item, "standard::*,trash::*", 0, NULL, &error);
	check (info != NULL);
	if (info != NULL) {
		check (g_strcmp0 (g_file_info_get_display_name (info), "report.2026.txt") == 0);

		/* The restore target. A wrong one here is how a file comes back under
		 * the wrong name or into the wrong folder. */
		orig_path = g_file_info_get_attribute_byte_string (info, "trash::orig-path");
		check (orig_path != NULL);
		if (orig_path != NULL) {
			check (g_ascii_strcasecmp (orig_path, orig) == 0);
			if (g_ascii_strcasecmp (orig_path, orig) != 0) {
				g_printerr ("  orig-path: %s\n  expected:  %s\n", orig_path, orig);
			}
		}

		/* Formatted from a 64-bit shell timestamp; a truncated one lands
		 * outside this century rather than merely being wrong. */
		date = g_file_info_get_attribute_string (info, "trash::deletion-date");
		check (date != NULL);
		if (date != NULL) {
			check (strlen (date) == strlen ("2026-01-01T00:00:00"));
			check (g_str_has_prefix (date, "20"));
			if (!g_str_has_prefix (date, "20")) {
				g_printerr ("  deletion-date: %s\n", date);
			}
		}

		g_object_unref (info);
	}
	g_clear_error (&error);

	/* The backing file keeps the real extension whatever the listing says, so a
	 * name that lost it would show up as a mismatch rather than a failed move. */
	listed_path = real_path_for_uri (uri);
	check (listed_path != NULL && g_str_has_suffix (listed_path, ".txt"));
	g_free (listed_path);

	/* --- restore, the way the view does it --- */
	dest = g_file_new_for_path (orig);
	check (g_file_move (item, dest, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error));
	if (error != NULL) {
		g_printerr ("  restore said: %s\n", error->message);
		g_clear_error (&error);
	}

	contents = NULL;
	check (g_file_get_contents (orig, &contents, NULL, NULL));
	check (g_strcmp0 (contents, "round trip") == 0);
	g_free (contents);

	g_object_unref (dest);
	g_object_unref (item);
	g_free (uri);

	g_unlink (orig);
	g_rmdir (dir);
	g_free (orig);
	g_free (dir);
}

/* --- a uri that names a file outside the bin is refused by every operation ---
 *
 * The path comes out of a uri any local process can hand over, so delete, move and
 * read each have to prove bin membership first. A "\.." inside a real bin item is
 * the second half: the shell resolves it, so the check has to resolve it too. */
static void
test_outside_bin_refused (void)
{
	char *outside, *uri, *bin_item, *escape_path, *dir, *bait, *dest_path;
	GFile *item, *dest;
	GFileInputStream *stream;
	GError *error = NULL;

	outside = g_build_filename (g_get_home_dir (), "nemoverify-outside.txt", NULL);
	write_fixture (outside, "keep me");

	uri = uri_for_real_path (outside);
	item = g_file_new_for_uri (uri);

	/* Each arm re-seeds: the first one succeeding would otherwise take the file
	 * away and leave the next two passing because there was nothing left to act
	 * on. Three separate entry points, three separate claims. */
	check (!g_file_delete (item, NULL, &error));
	check (error != NULL);
	g_clear_error (&error);
	check (g_file_test (outside, G_FILE_TEST_EXISTS));

	write_fixture (outside, "keep me");
	dest_path = g_build_filename (g_get_home_dir (), "nemoverify-outside-moved.txt", NULL);
	g_unlink (dest_path);
	dest = g_file_new_for_path (dest_path);
	check (!g_file_move (item, dest, G_FILE_COPY_OVERWRITE, NULL, NULL, NULL, &error));
	check (error != NULL);
	g_clear_error (&error);
	check (!g_file_test (dest_path, G_FILE_TEST_EXISTS));
	g_unlink (dest_path);
	g_object_unref (dest);
	g_free (dest_path);

	write_fixture (outside, "keep me");
	stream = g_file_read (item, NULL, &error);
	check (stream == NULL);
	check (error != NULL);
	g_clear_error (&error);
	g_clear_object (&stream);

	g_object_unref (item);
	g_free (uri);

	/* --- the traversal half: a real bin item with "\..\.." walked off it --- */
	dir = g_build_filename (g_get_home_dir (), "nemoverify-bait", NULL);
	g_mkdir_with_parents (dir, 0700);
	bait = g_build_filename (dir, "bait.txt", NULL);
	write_fixture (bait, "bait");

	if (recycle_quietly (bait)) {
		uri = find_item_uri ("bait.txt");
		check (uri != NULL);

		if (uri != NULL) {
			bin_item = real_path_for_uri (uri);

			/* Three levels up is the drive root ($R file -> per-sid folder
			 * -> $Recycle.Bin -> C:\), then back down to a file the caller
			 * was never granted. The target is spelled drive-relative so
			 * the walk is what gets there. */
			escape_path = g_strconcat (bin_item, "\\..\\..\\..\\",
						   outside + (g_path_is_absolute (outside) ? 3 : 0),
						   NULL);
			g_free (bin_item);
			g_free (uri);

			uri = uri_for_real_path (escape_path);
			item = g_file_new_for_uri (uri);

			write_fixture (outside, "keep me");
			check (!g_file_delete (item, NULL, &error));
			check (error != NULL);
			g_clear_error (&error);
			check (g_file_test (outside, G_FILE_TEST_EXISTS));

			g_object_unref (item);
			g_free (uri);
			g_free (escape_path);
		}

		/* Take the bait back out of the bin, every match of it. */
		while ((uri = find_item_uri ("bait.txt")) != NULL) {
			item = g_file_new_for_uri (uri);
			g_file_delete (item, NULL, NULL);
			g_object_unref (item);
			g_free (uri);
		}
	} else {
		g_printerr ("SKIP traversal case: could not recycle %s\n", bait);
	}

	g_free (bait);
	g_rmdir (dir);
	g_free (dir);

	g_unlink (outside);
	g_free (outside);
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
	test_real_bin_roundtrip ();
	test_outside_bin_refused ();

	/* The seeded cases plant files into wine's unix-style XDG trash layout, which
	 * does not exist on real Windows. Everything above works the shell bin itself,
	 * so a native run is a real result and says so; only the seeded half is out of
	 * reach here. Reporting the whole test as a skip is what let the recycle-bin
	 * divergences through before. */
	wine_home = g_getenv ("WINEHOMEDIR");
	if (wine_home == NULL) {
		g_printerr ("skipping the seeded cases: they need wine (WINEHOMEDIR unset)\n");
		if (failures != 0) {
			return EXIT_FAILURE;
		}
		if (!real_bin_ran) {
			g_printerr ("SKIP: the shell recycle bin could not be worked either\n");
			return TEST_SKIPPED;
		}
		g_print ("trash-win32: recycle bin checks passed\n");
		return EXIT_SUCCESS;
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
