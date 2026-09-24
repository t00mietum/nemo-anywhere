/* Make link, through the real job, with what the dialog would have answered
 * handed in up front. One job per run, for the reason the link copy job test
 * gives: the queue starts a job only when the one before it says it is done.
 *
 * Argument: "relative" (default), "absolute", "hardlink" or "junction".
 */

#include "test.h"

#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-link-copy.h>

#include <glib/gstdio.h>
#include <stdio.h>
#include <stdlib.h>

#include "test-scratch.h"
#include "test-check.h"

#define JOB_TIMEOUT_SECONDS 20

static gboolean job_finished;
static gboolean job_succeeded;

static void
job_done (GHashTable *debuting_uris,
          gboolean success,
          gpointer data)
{
	job_succeeded = success;
	job_finished = TRUE;
	gtk_main_quit ();
}

static gboolean
give_up (gpointer data)
{
	g_printerr ("FAIL: link job did not finish within %d seconds\n", JOB_TIMEOUT_SECONDS);
	failures++;
	gtk_main_quit ();
	return G_SOURCE_REMOVE;
}

static NemoLinkKind
kind_of (const char *path)
{
	GFile *file = g_file_new_for_path (path);
	NemoLinkKind kind = nemo_link_kind (file, NULL);

	g_object_unref (file);
	return kind;
}

static char *
target_of (const char *path)
{
	GFile *file = g_file_new_for_path (path);
	char *target = NULL;

	nemo_link_read_target (file, &target, NULL);
	g_object_unref (file);
	return target;
}

static char *
uri_of (const char *path)
{
	return g_filename_to_uri (path, NULL, NULL);
}

int
main (int argc, char *argv[])
{
	NemoLinkOptions options = { FALSE, FALSE, TRUE };
	GtkWidget *window;
	GList *uris = NULL;
	const char *how;
	char *tmp, *src_dir, *dst_dir, *dst_uri;
	char *payload, *folder, *made_file, *made_folder;
	char *text, *want, *contents = NULL;
	guint supported, timeout_id;
	gboolean with_file, with_folder;
	FILE *fp;

	g_free (test_scratch_config_home ("nemo-make-link-home-XXXXXX"));
	nemo_global_preferences_init ();
	test_init (&argc, &argv);
	how = (argc > 1) ? argv[1] : "relative";

	tmp = test_scratch_dir ("nemo-make-link-XXXXXX", NULL);
	src_dir = g_build_filename (tmp, "from", NULL);
	dst_dir = g_build_filename (tmp, "to", NULL);
	g_mkdir_with_parents (src_dir, 0700);
	g_mkdir_with_parents (dst_dir, 0700);

	payload = g_build_filename (src_dir, "payload.txt", NULL);
	folder = g_build_filename (src_dir, "folder", NULL);
	made_file = g_build_filename (dst_dir, "payload.txt", NULL);
	made_folder = g_build_filename (dst_dir, "folder", NULL);
	check (g_file_set_contents (payload, "payload", -1, NULL));
	g_mkdir_with_parents (folder, 0700);

	supported = nemo_link_kinds_supported (dst_dir);
	with_file = g_strcmp0 (how, "junction") != 0;
	with_folder = g_strcmp0 (how, "hardlink") != 0;

	if (g_strcmp0 (how, "absolute") == 0) {
		options.relative = FALSE;
	} else if (g_strcmp0 (how, "hardlink") == 0) {
		options.file_hardlink = TRUE;
	} else if (g_strcmp0 (how, "junction") == 0) {
		options.folder_junction = TRUE;
	}

	if (g_strcmp0 (how, "junction") == 0
	    ? !(supported & NEMO_LINK_JUNCTION)
	    : (g_strcmp0 (how, "hardlink") != 0 && !(supported & NEMO_LINK_FILE_SYMLINK))) {
		g_printerr ("note: that kind of link cannot be made here, nothing to check\n");
		return 77;
	}

	if (with_file) {
		uris = g_list_append (uris, uri_of (payload));
	}
	if (with_folder) {
		uris = g_list_append (uris, uri_of (folder));
	}
	dst_uri = uri_of (dst_dir);

	window = test_window_new ("make link test", 5);
	gtk_widget_show (window);

	nemo_file_operations_symlink (uris, NULL, dst_uri, &options, window, job_done, NULL);

	timeout_id = g_timeout_add_seconds (JOB_TIMEOUT_SECONDS, give_up, NULL);
	gtk_main ();
	g_source_remove (timeout_id);

	check (job_finished);
	check (job_succeeded);

	if (g_strcmp0 (how, "relative") == 0) {
		want = g_build_filename ("..", "from", "payload.txt", NULL);
		text = target_of (made_file);
		check (g_strcmp0 (text, want) == 0);
		g_free (text);
		g_free (want);

		want = g_build_filename ("..", "from", "folder", NULL);
		text = target_of (made_folder);
		check (g_strcmp0 (text, want) == 0);
		check (kind_of (made_folder) == NEMO_LINK_DIR_SYMLINK);
		g_free (text);
		g_free (want);

		/* And it leads where it should. */
		check (g_file_get_contents (made_file, &contents, NULL, NULL) &&
		       g_strcmp0 (contents, "payload") == 0);
		g_clear_pointer (&contents, g_free);
	} else if (g_strcmp0 (how, "absolute") == 0) {
		text = target_of (made_file);
		check (text != NULL && g_path_is_absolute (text));
		check (g_strcmp0 (text, payload) == 0);
		g_free (text);
		check (kind_of (made_folder) == NEMO_LINK_DIR_SYMLINK);
	} else if (g_strcmp0 (how, "hardlink") == 0) {
		/* Not a link at all to look at: a second name for the same file. */
		check (kind_of (made_file) == NEMO_LINK_NONE);
		fp = g_fopen (made_file, "ab");
		check (fp != NULL);
		if (fp != NULL) {
			fputs (" more", fp);
			fclose (fp);
		}
		check (g_file_get_contents (payload, &contents, NULL, NULL) &&
		       g_strcmp0 (contents, "payload more") == 0);
		g_clear_pointer (&contents, g_free);
	} else {
		check (kind_of (made_folder) == NEMO_LINK_JUNCTION);
	}

	/* The originals are where they were. */
	check (kind_of (payload) == NEMO_LINK_NONE);
	check (g_file_test (folder, G_FILE_TEST_IS_DIR));

	g_list_free_full (uris, g_free);
	g_free (dst_uri);
	g_free (made_folder);
	g_free (made_file);
	g_free (folder);
	g_free (payload);
	g_free (dst_dir);
	g_free (src_dir);
	g_free (tmp);

	if (failures > 0) {
		return EXIT_FAILURE;
	}

	g_print ("make link job (%s): all checks passed\n", how);
	return EXIT_SUCCESS;
}
