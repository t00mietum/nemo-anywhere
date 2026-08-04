/* Exercises typed-location parsing: backslash-separated input resolves
 * by fallback, while literal names - including real backslash-named
 * files on POSIX - always win. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#include <eel/eel-vfs-extensions.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

int
main (int argc, char *argv[])
{
	char *tmpdir, *real_file, *typed, *resolved;
	GFile *location;

	tmpdir = g_dir_make_tmp ("eel-input-test-XXXXXX", NULL);
	g_assert (tmpdir != NULL);

	real_file = g_build_filename (tmpdir, "plain.txt", NULL);
	g_file_set_contents (real_file, "x", -1, NULL);

	/* backslash-separated form of an existing path resolves to it */
	typed = g_strdelimit (g_strdup (real_file), "/", '\\');
	location = eel_g_file_new_for_user_input (typed);
	resolved = g_file_get_path (location);
	check (resolved != NULL && g_file_test (resolved, G_FILE_TEST_EXISTS));
	g_free (resolved);
	g_object_unref (location);
	g_free (typed);

	/* nonexistent input stays literal - no surprise rewriting */
	typed = g_build_filename (tmpdir, "no-such\\thing", NULL);
	location = eel_g_file_new_for_user_input (typed);
	resolved = g_file_get_path (location);
	check (resolved != NULL && strstr (resolved, "no-such") != NULL);
	g_free (resolved);
	g_object_unref (location);
	g_free (typed);

#ifndef G_OS_WIN32
	/* a file literally named with a backslash keeps working on POSIX */
	{
		char *bs_file = g_build_filename (tmpdir, "a\\b.txt", NULL);
		g_file_set_contents (bs_file, "y", -1, NULL);

		location = eel_g_file_new_for_user_input (bs_file);
		resolved = g_file_get_path (location);
		check (resolved != NULL && strcmp (resolved, bs_file) == 0);
		g_free (resolved);
		g_object_unref (location);
		g_unlink (bs_file);
		g_free (bs_file);
	}
#endif

	g_unlink (real_file);
	g_free (real_file);
	g_rmdir (tmpdir);
	g_free (tmpdir);

	if (failures == 0) {
		g_print ("eel-user-input: all checks passed\n");
	}

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
