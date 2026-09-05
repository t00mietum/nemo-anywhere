/* The command line one copy hands another to show a location, and the
 * fallback when a selection cannot be said on one. */

#include <config.h>

#include <gio/gio.h>

#include "nemo-new-process.h"
#include <libnemo-private/nemo-file-utilities.h>

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
	GFile *folder = g_file_new_for_path ("/some/folder");
	GFile *item = g_file_new_for_path ("/some/folder/item.txt");
	GFile *elsewhere = g_file_new_for_path ("/other/place");
	char *exe = nemo_get_exe_path ();
	char **args;

	/* The first word is this very program, by full path. */
	check (exe != NULL);
	check (exe != NULL && g_path_is_absolute (exe));
	check (exe != NULL && g_file_test (exe, G_FILE_TEST_IS_EXECUTABLE));

	args = nemo_new_process_argv (folder, NULL);
	check (g_strv_length (args) == 2);
	check (g_strcmp0 (args[0], exe) == 0);
	check (g_strcmp0 (args[1], "file:///some/folder") == 0);
	g_strfreev (args);

	/* An item in the folder asked for: the folder is implied by --select. */
	args = nemo_new_process_argv (folder, item);
	check (g_strv_length (args) == 3);
	check (g_strcmp0 (args[1], "--select") == 0);
	check (g_strcmp0 (args[2], "file:///some/folder/item.txt") == 0);
	g_strfreev (args);

	/* A selection with no folder given still works. */
	args = nemo_new_process_argv (NULL, item);
	check (g_strv_length (args) == 3);
	check (g_strcmp0 (args[1], "--select") == 0);
	g_strfreev (args);

	/* An item outside the folder cannot be said: the folder wins. */
	args = nemo_new_process_argv (elsewhere, item);
	check (g_strv_length (args) == 2);
	check (g_strcmp0 (args[1], "file:///other/place") == 0);
	g_strfreev (args);

	/* Nothing at all opens the default place. */
	args = nemo_new_process_argv (NULL, NULL);
	check (g_strv_length (args) == 1);
	g_strfreev (args);

	g_free (exe);
	g_object_unref (folder);
	g_object_unref (item);
	g_object_unref (elsewhere);

	if (failures == 0) {
		g_print ("OK\n");
	}

	return failures == 0 ? 0 : 1;
}
