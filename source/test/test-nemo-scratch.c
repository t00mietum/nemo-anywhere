/* The tests' scratch cleanup removes what it made and nothing else. A link out
 * of a scratch tree goes as a link, and a scratch path that has since been
 * swapped for a link is left alone. */

#include <config.h>

#include <stdlib.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "test-scratch.h"
#include "test-check.h"

static char *
make_file (const char *dir, const char *name)
{
	char *path = g_build_filename (dir, name, NULL);

	check (g_file_set_contents (path, "x", -1, NULL));
	return path;
}

static void
make_link (const char *target, const char *dir, const char *name)
{
	char *path = g_build_filename (dir, name, NULL);

	check (symlink (target, path) == 0);
	g_free (path);
}

int
main (void)
{
	char *outside, *kept, *scratch, *deep, *moved;

	/* Made the ordinary way, so the cleanup has no claim on it. */
	outside = g_dir_make_tmp ("nemo-scratch-outside-XXXXXX", NULL);
	check (outside != NULL);
	if (outside == NULL) {
		return EXIT_FAILURE;
	}
	kept = make_file (outside, "kept.txt");

	scratch = test_scratch_dir ("nemo-scratch-XXXXXX", NULL);
	deep = g_build_filename (scratch, "a", "b", NULL);
	check (g_mkdir_with_parents (deep, 0700) == 0);
	g_free (make_file (deep, "file.txt"));
	g_free (make_file (scratch, ".hidden"));
	make_link (outside, deep, "to-dir");
	make_link (kept, scratch, "to-file");

	test_scratch_cleanup ();

	check (!g_file_test (scratch, G_FILE_TEST_EXISTS));
	check (g_file_test (kept, G_FILE_TEST_IS_REGULAR));

	/* Swapped for a link after it was made: not the directory that was made. */
	g_free (scratch);
	scratch = test_scratch_dir ("nemo-scratch-XXXXXX", NULL);
	moved = g_strconcat (scratch, "-moved", NULL);
	check (g_rename (scratch, moved) == 0);
	check (symlink (outside, scratch) == 0);

	test_scratch_cleanup ();

	check (g_file_test (kept, G_FILE_TEST_IS_REGULAR));
	check (g_file_test (scratch, G_FILE_TEST_IS_SYMLINK));

	g_unlink (scratch);
	g_rmdir (moved);
	g_unlink (kept);
	g_rmdir (outside);

	g_free (moved);
	g_free (deep);
	g_free (scratch);
	g_free (kept);
	g_free (outside);

	if (failures == 0) {
		g_print ("nemo-scratch: all checks passed\n");
	}

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
