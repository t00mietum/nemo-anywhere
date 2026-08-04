/* Exercises the Recycle Bin trash:/// backend end to end under wine:
 * enumeration with trash attributes, per-item delete, and restore via
 * move, including metadata-sibling cleanup in the XDG trash wine maps
 * the bin onto. Windows-only; the Linux build compiles it out. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32

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

static void
seed_item (const char *name, const char *content)
{
	char *path, *info_path, *info_content;

	path = g_build_filename (trash_files_dir, name, NULL);
	g_file_set_contents (path, content, -1, NULL);
	g_free (path);

	info_path = g_strdup_printf ("%s%c%s.trashinfo",
				     trash_info_dir, G_DIR_SEPARATOR, name);
	info_content = g_strdup_printf ("[Trash Info]\nPath=/root/%s\nDeletionDate=2026-07-20T10:00:00\n",
					name);
	g_file_set_contents (info_path, info_content, -1, NULL);
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

int
main (int argc, char *argv[])
{
	const char *wine_home;
	char *home_root, *uri, *dest_path, *contents;
	GFile *item, *dest;
	GFileInfo *info;

	wine_home = g_getenv ("WINEHOMEDIR");
	if (wine_home == NULL) {
		g_printerr ("SKIP: needs wine (WINEHOMEDIR unset)\n");
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

	nemo_trash_win32_register ();

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
