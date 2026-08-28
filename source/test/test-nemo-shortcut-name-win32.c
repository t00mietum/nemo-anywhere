/* How a shortcut is named on screen, and what a rename does with the extension
 * that is not on screen. The second half is the one that matters: the rename box
 * starts from the shown name, so without putting the .lnk back a rename turns a
 * shortcut into an ordinary file and the target is gone. Windows-only. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#ifdef G_OS_WIN32

#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-global-preferences.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

typedef struct {
	GMainLoop *loop;
	GError    *error;
} RenameWait;

static void
rename_done (NemoFile *file, GFile *result, GError *error, gpointer data)
{
	RenameWait *wait = data;

	if (error != NULL) {
		wait->error = g_error_copy (error);
	}
	g_main_loop_quit (wait->loop);
}

/* Rename @file to @new_name and wait for the operation to finish. */
static gboolean
rename_and_wait (NemoFile *file, const char *new_name)
{
	RenameWait wait = { g_main_loop_new (NULL, FALSE), NULL };
	gboolean ok;

	nemo_file_rename (file, new_name, rename_done, &wait);
	g_main_loop_run (wait.loop);

	ok = wait.error == NULL;
	if (!ok) {
		g_printerr ("  rename to %s failed: %s\n", new_name, wait.error->message);
		g_error_free (wait.error);
	}
	g_main_loop_unref (wait.loop);

	return ok;
}

static NemoFile *
file_at (const char *path)
{
	GFile *location = g_file_new_for_path (path);
	NemoFile *file = nemo_file_get (location);

	g_object_unref (location);
	return file;
}

/* The preference is cached behind a changed:: handler, so let it be delivered. */
static void
set_show_extension (gboolean show)
{
	nemo_config_set_boolean (nemo_preferences,
				 NEMO_PREFERENCES_SHOW_SHORTCUT_EXTENSION, show);

	while (g_main_context_iteration (NULL, FALSE)) {
		/* drain */
	}
}

int
main (int argc, char *argv[])
{
	char *dir;
	char *scratch_config;
	char *link_path;
	char *renamed_path;
	NemoFile *file;
	char *shown;

	dir = g_dir_make_tmp ("nemo-lnkname-XXXXXX", NULL);
	g_assert (dir != NULL);

	/* A test that reads the real config fails on somebody else's machine. */
	scratch_config = g_build_filename (dir, "config", NULL);
	g_mkdir_with_parents (scratch_config, 0700);
	g_setenv ("APPDATA", scratch_config, TRUE);
	g_setenv ("HOME", scratch_config, TRUE);
	g_setenv ("XDG_CONFIG_HOME", scratch_config, TRUE);

	/* Only so the icon theme nemo-file hooks on startup has a screen to hang
	   off; nothing here needs a window. */
	gtk_init_check (&argc, &argv);
	nemo_global_preferences_init ();

	link_path = g_build_filename (dir, "target.lnk", NULL);
	if (!g_file_set_contents (link_path, "not a real shortcut, only a name", -1, NULL)) {
		g_printerr ("FAIL cannot write %s\n", link_path);
		failures++;
	}

	file = file_at (link_path);
	g_assert (file != NULL);

	set_show_extension (FALSE);
	shown = nemo_file_get_display_name (file);
	check (g_strcmp0 (shown, "target") == 0);
	g_free (shown);

	set_show_extension (TRUE);
	shown = nemo_file_get_display_name (file);
	check (g_strcmp0 (shown, "target.lnk") == 0);
	g_free (shown);

	/* With the extension hidden, a rename typed as the shown name keeps it. */
	set_show_extension (FALSE);
	if (rename_and_wait (file, "renamed")) {
		renamed_path = g_build_filename (dir, "renamed.lnk", NULL);
		check (g_file_test (renamed_path, G_FILE_TEST_EXISTS));
		g_free (renamed_path);

		renamed_path = g_build_filename (dir, "renamed", NULL);
		check (!g_file_test (renamed_path, G_FILE_TEST_EXISTS));
		g_free (renamed_path);
	} else {
		failures++;
	}

	nemo_file_unref (file);
	g_free (link_path);
	g_free (scratch_config);
	g_free (dir);

	if (failures > 0) {
		g_printerr ("%d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("shortcut-name-win32: all checks passed\n");
	return EXIT_SUCCESS;
}

#else /* !G_OS_WIN32 */

int
main (void)
{
	g_print ("shortcut-name-win32: skipped (Windows only)\n");
	return EXIT_SUCCESS;
}

#endif
