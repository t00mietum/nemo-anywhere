/* Copying a link should be able to leave a link rather than a copy of what it
 * points at. Windows makes that harder than POSIX: there are three kinds to
 * tell apart, the toolkit says only "this is a link", and the kind has to come
 * from the reparse tag. These checks cover reading a link, making one back
 * again, and the rule that decides what a link becomes when the destination
 * cannot hold its own kind.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <libnemo-private/nemo-link-copy.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

/* Windows wants Developer Mode or an elevated run before it will make a
   symlink; where neither is on, those checks are skipped rather than failed. */
static gboolean symlinks;
static gboolean junctions;

static NemoLinkKind
kind_of (const char *path)
{
	GFile *file = g_file_new_for_path (path);
	NemoLinkKind kind = nemo_link_kind (file, NULL);

	g_object_unref (file);
	return kind;
}

static gboolean
target_of (const char *path, char **target)
{
	GFile *file = g_file_new_for_path (path);
	gboolean ok = nemo_link_read_target (file, target, NULL);

	g_object_unref (file);
	return ok;
}

/* Removes a link rather than following it - deleting the contents of what a
   link points at is not a mistake a test gets to make twice. */
static void
remove_tree (const char *path)
{
	GDir *dir;
	const char *name;

	if (kind_of (path) != NEMO_LINK_NONE) {
		if (g_rmdir (path) != 0) {
			g_remove (path);
		}
		return;
	}

	dir = g_dir_open (path, 0, NULL);
	if (dir != NULL) {
		while ((name = g_dir_read_name (dir)) != NULL) {
			char *child = g_build_filename (path, name, NULL);

			remove_tree (child);
			g_free (child);
		}
		g_dir_close (dir);
		g_rmdir (path);
		return;
	}

	g_remove (path);
}

static void
check_kinds (const char *dir)
{
	char *real_dir = g_build_filename (dir, "real", NULL);
	char *real_file = g_build_filename (real_dir, "f.txt", NULL);
	char *missing = g_build_filename (dir, "not-here", NULL);
	char *junction = g_build_filename (dir, "junc", NULL);
	char *dir_sym = g_build_filename (dir, "dsym", NULL);
	char *file_sym = g_build_filename (dir, "fsym", NULL);
	char *target = NULL;

	g_mkdir_with_parents (real_dir, 0700);
	check (g_file_set_contents (real_file, "hello", 5, NULL));

	check (kind_of (real_dir) == NEMO_LINK_NONE);
	check (kind_of (real_file) == NEMO_LINK_NONE);
	check (kind_of (missing) == NEMO_LINK_NONE);

	if (junctions) {
		check (nemo_link_create (real_dir, junction, NULL, NEMO_LINK_JUNCTION, NULL));
		check (kind_of (junction) == NEMO_LINK_JUNCTION);
		check (target_of (junction, &target));
		/* A junction always records an absolute path, whatever it was given. */
		check (target != NULL && g_path_is_absolute (target));
		g_free (target);
		target = NULL;
	}

	if (symlinks) {
		check (nemo_link_create (real_dir, dir_sym, NULL, NEMO_LINK_DIR_SYMLINK, NULL));
		check (kind_of (dir_sym) == NEMO_LINK_DIR_SYMLINK);

		check (nemo_link_create (real_file, file_sym, NULL, NEMO_LINK_FILE_SYMLINK, NULL));
		check (kind_of (file_sym) == NEMO_LINK_FILE_SYMLINK);
		check (target_of (file_sym, &target));
		check (g_strcmp0 (target, real_file) == 0);
		g_free (target);
	}

	g_free (real_dir);
	g_free (real_file);
	g_free (missing);
	g_free (junction);
	g_free (dir_sym);
	g_free (file_sym);
}

/* A relative link keeps its relative text, so a copy of it points where the
   original pointed rather than back at the original's folder. */
static void
check_relative_target (const char *dir)
{
	char *real_dir = g_build_filename (dir, "rel", NULL);
	char *real_file = g_build_filename (real_dir, "f.txt", NULL);
	char *link = g_build_filename (real_dir, "link", NULL);
	char *junction = g_build_filename (dir, "rel-as-junction", NULL);
	char *target = NULL;

	if (!symlinks) {
		goto out;
	}

	g_mkdir_with_parents (real_dir, 0700);
	check (g_file_set_contents (real_file, "hello", 5, NULL));

	check (nemo_link_create ("f.txt", link, NULL, NEMO_LINK_FILE_SYMLINK, NULL));
	check (target_of (link, &target));
	check (g_strcmp0 (target, "f.txt") == 0);
	g_free (target);
	target = NULL;

	/* Turning a relative link into a junction has to resolve it first - a
	   junction can only name a full path. */
	if (junctions) {
		check (nemo_link_create ("rel", junction, dir, NEMO_LINK_JUNCTION, NULL));
		check (kind_of (junction) == NEMO_LINK_JUNCTION);
		check (target_of (junction, &target));
		check (target != NULL && g_str_has_suffix (target, "rel"));
		g_free (target);
	}

 out:
	g_free (real_dir);
	g_free (real_file);
	g_free (link);
	g_free (junction);
}

static void
check_choice_defaults (void)
{
	NemoLinkChoice choice;

	/* Everything allowed: every kind stays what it was. */
	nemo_link_choice_init (&choice, NEMO_LINK_ANY);
	check (choice.file_symlink_as == NEMO_LINK_FILE_SYMLINK);
	check (choice.dir_symlink_as == NEMO_LINK_DIR_SYMLINK);
	check (choice.junction_as == NEMO_LINK_JUNCTION);
	check (nemo_link_choice_makes_links (&choice));

	/* Junctions only: a folder symlink falls back to one, which still points at
	   the same place. A file symlink has nowhere to go but a copy. */
	nemo_link_choice_init (&choice, NEMO_LINK_JUNCTION);
	check (choice.file_symlink_as == NEMO_LINK_NONE);
	check (choice.dir_symlink_as == NEMO_LINK_JUNCTION);
	check (choice.junction_as == NEMO_LINK_JUNCTION);

	/* Symlinks only, which is every platform but Windows. */
	nemo_link_choice_init (&choice, NEMO_LINK_FILE_SYMLINK | NEMO_LINK_DIR_SYMLINK);
	check (choice.file_symlink_as == NEMO_LINK_FILE_SYMLINK);
	check (choice.dir_symlink_as == NEMO_LINK_DIR_SYMLINK);
	check (choice.junction_as == NEMO_LINK_DIR_SYMLINK);

	/* A destination that keeps no links at all asks nothing and copies. */
	nemo_link_choice_init (&choice, 0);
	check (!nemo_link_choice_makes_links (&choice));
	check (nemo_link_choice_for (&choice, NEMO_LINK_JUNCTION) == NEMO_LINK_NONE);
	check (nemo_link_choice_for (&choice, NEMO_LINK_NONE) == NEMO_LINK_NONE);
}

static void
check_destination_support (const char *dir)
{
	guint kinds = nemo_link_kinds_supported (dir);

	check (((kinds & NEMO_LINK_FILE_SYMLINK) != 0) == symlinks);
	check (((kinds & NEMO_LINK_JUNCTION) != 0) == junctions);
	check (nemo_link_kinds_supported (NULL) == 0);
}

int
main (int argc, char **argv)
{
	char *dir;
	guint supported;

	g_test_init (&argc, &argv, NULL);

	dir = g_dir_make_tmp ("nemo-link-copy-XXXXXX", NULL);
	g_assert (dir != NULL);

	supported = nemo_link_kinds_supported (dir);
	symlinks = (supported & NEMO_LINK_FILE_SYMLINK) != 0;
	junctions = (supported & NEMO_LINK_JUNCTION) != 0;
	if (!symlinks) {
		g_printerr ("note: symlinks are not permitted here, so those checks are skipped\n");
	}

	check_kinds (dir);
	check_relative_target (dir);
	check_choice_defaults ();
	check_destination_support (dir);

	remove_tree (dir);
	g_free (dir);

	g_printerr ("%d failure(s)\n", failures);
	return failures == 0 ? 0 : 1;
}
