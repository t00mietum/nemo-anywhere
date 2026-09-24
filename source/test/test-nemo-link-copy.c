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
#ifndef G_OS_WIN32
#include <unistd.h>
#endif

#include <libnemo-private/nemo-link-copy.h>

#include "test-scratch.h"
#include "test-check.h"

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
	char *real_file = g_build_filename (dir, "support-file", NULL);
	char *made = g_build_filename (dir, "support-link", NULL);

	check (g_file_set_contents (real_file, "hello", 5, NULL));

	/* Against the file system rather than against what the same call said a
	   moment ago. A destination that claims a kind it will not take offers the
	   user a choice the copy then cannot carry out. Only the file symlink is
	   checked this way: POSIX makes a symlink whatever kind is asked for, so a
	   junction request here would succeed on a platform that has none. */
	check (((kinds & NEMO_LINK_FILE_SYMLINK) != 0)
	       == (nemo_link_create (real_file, made, NULL, NEMO_LINK_FILE_SYMLINK, NULL) != FALSE));

	check (nemo_link_kinds_supported (NULL) == 0);

	g_remove (made);
	g_remove (real_file);
	g_free (made);
	g_free (real_file);
}

/* How the Make link dialog spells a relative symlink. */
static void
check_relative_spelling (const char *dir)
{
	char *from = g_build_filename (dir, "sp", "a", NULL);
	char *deep = g_build_filename (dir, "sp", "b", "c", NULL);
	char *target = g_build_filename (deep, "t.txt", NULL);
	char *beside = g_build_filename (from, "x.txt", NULL);
	char *want = g_build_filename ("..", "b", "c", "t.txt", NULL);
	char *text;

	g_mkdir_with_parents (from, 0700);
	g_mkdir_with_parents (deep, 0700);
	check (g_file_set_contents (target, "t", 1, NULL));

	text = nemo_link_relative_target (target, from);
	check (g_strcmp0 (text, want) == 0);
	g_free (text);

	text = nemo_link_relative_target (beside, from);
	check (g_strcmp0 (text, "x.txt") == 0);
	g_free (text);

	/* A link to the folder it sits in. */
	text = nemo_link_relative_target (from, from);
	check (g_strcmp0 (text, ".") == 0);
	g_free (text);

#ifdef G_OS_WIN32
	/* No way from one drive to another. */
	text = nemo_link_relative_target ("D:\\x\\t.txt", "C:\\y");
	check (text == NULL);
	g_free (text);
#else
	/* Reached through a symlinked folder, the spelling is from where the
	   link really sits, or it would point somewhere else. */
	{
		char *alias = g_build_filename (dir, "alias", NULL);
		char *made = g_build_filename (alias, "made", NULL);
		char *contents = NULL;

		check (symlink (from, alias) == 0);
		text = nemo_link_relative_target (target, alias);
		check (g_strcmp0 (text, want) == 0);
		check (text != NULL && symlink (text, made) == 0);
		check (g_file_get_contents (made, &contents, NULL, NULL) &&
		       g_strcmp0 (contents, "t") == 0);
		g_free (contents);
		g_free (text);
		g_remove (made);
		g_remove (alias);
		g_free (made);
		g_free (alias);
	}
#endif

	g_free (want);
	g_free (beside);
	g_free (target);
	g_free (deep);
	g_free (from);
}

static void
check_link_options (void)
{
	NemoLinkOptions options;

	/* Every open starts the same way. */
	nemo_link_options_initial (NEMO_LINK_ANY, &options);
	check (options.folder_kind == NEMO_MAKE_JUNCTION && options.file_kind == NEMO_MAKE_SYMLINK &&
	       !options.relative);

	/* No junctions here. */
	nemo_link_options_initial (NEMO_LINK_FILE_SYMLINK | NEMO_LINK_DIR_SYMLINK, &options);
	check (options.folder_kind == NEMO_MAKE_SYMLINK && options.file_kind == NEMO_MAKE_SYMLINK);

	/* Windows without the symlink privilege: folders fall back to a junction,
	   and a file to a shortcut, never a hardlink. */
	nemo_link_options_initial (NEMO_LINK_JUNCTION, &options);
	check (options.folder_kind == NEMO_MAKE_JUNCTION && options.file_kind == NEMO_MAKE_SHORTCUT);

	/* Nothing else possible: a shortcut can always be made. */
	nemo_link_options_initial (0, &options);
	check (options.folder_kind == NEMO_MAKE_SHORTCUT && options.file_kind == NEMO_MAKE_SHORTCUT);

	/* The path choice matters only while something comes out a symlink, or a
	   shortcut made on Windows. */
	options.folder_kind = NEMO_MAKE_JUNCTION;
	options.file_kind = NEMO_MAKE_HARDLINK;
	check (!nemo_link_options_uses_path (&options, 2, 3));
	options.file_kind = NEMO_MAKE_SYMLINK;
	check (nemo_link_options_uses_path (&options, 2, 3));
	check (!nemo_link_options_uses_path (&options, 2, 0));
	options.folder_kind = NEMO_MAKE_SYMLINK;
	check (nemo_link_options_uses_path (&options, 1, 0));
	options.folder_kind = NEMO_MAKE_SHORTCUT;
	options.file_kind = NEMO_MAKE_SHORTCUT;
#ifdef G_OS_WIN32
	check (nemo_link_options_uses_path (&options, 1, 1));
#else
	check (!nemo_link_options_uses_path (&options, 1, 1));
#endif
}

static void
check_hardlink (const char *dir)
{
	char *first = g_build_filename (dir, "hard-1", NULL);
	char *second = g_build_filename (dir, "hard-2", NULL);
	char *missing = g_build_filename (dir, "hard-missing", NULL);
	char *third = g_build_filename (dir, "hard-3", NULL);
	char *contents = NULL;
	GError *error = NULL;
	FILE *fp;

	check (g_file_set_contents (first, "one", -1, NULL));
	check (nemo_link_create_hard (first, second, &error));
	g_clear_error (&error);

	/* Written in place through one name, seen through the other. */
	fp = g_fopen (second, "ab");
	check (fp != NULL);
	if (fp != NULL) {
		fputs ("two", fp);
		fclose (fp);
	}
	check (g_file_get_contents (first, &contents, NULL, NULL) &&
	       g_strcmp0 (contents, "onetwo") == 0);
	g_free (contents);

	/* A taken name comes back as EXISTS, which the job retries under
	   another name. */
	check (!nemo_link_create_hard (first, second, &error));
	check (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS));
	g_clear_error (&error);

	check (!nemo_link_create_hard (missing, third, &error));
	check (error != NULL);
	g_clear_error (&error);

	g_remove (second);
	g_remove (first);
	g_free (third);
	g_free (missing);
	g_free (second);
	g_free (first);
}

int
main (int argc, char **argv)
{
	char *dir;
	guint supported;

	g_test_init (&argc, &argv, NULL);

	dir = test_scratch_dir ("nemo-link-copy-XXXXXX", NULL);
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
	check_link_options ();
	check_hardlink (dir);
	if (symlinks) {
		check_relative_spelling (dir);
	}
	check_destination_support (dir);

	g_free (dir);

	g_printerr ("%d failure(s)\n", failures);
	return failures == 0 ? 0 : 1;
}
