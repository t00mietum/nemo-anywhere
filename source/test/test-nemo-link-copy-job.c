/* A copy that includes a link should leave a link, and a copy told to follow
 * the link should leave the contents instead. Runs the real copy job, with the
 * dialog answered up front through NEMO_LINK_COPY - there is nobody here to
 * click it. One copy per run: the job queue only starts a job when the one
 * before it reports finished, and nothing here is listening for that.
 *
 * Argument: "keep" (default) or "copy".
 */

#include "test.h"

#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-link-copy.h>

#include <glib/gstdio.h>
#include <stdlib.h>

#define COPY_TIMEOUT_SECONDS 20

static int failures;
static gboolean copy_finished;
static gboolean copy_succeeded;

#define check(expr)							\
	G_STMT_START {							\
		if (!(expr)) {						\
			g_printerr ("FAIL %s:%d: %s\n",			\
				    __FILE__, __LINE__, #expr);		\
			failures++;					\
		}							\
	} G_STMT_END

static void
copy_done (GHashTable *debuting_uris,
           gboolean success,
           gpointer data)
{
	copy_succeeded = success;
	copy_finished = TRUE;
	gtk_main_quit ();
}

static gboolean
give_up (gpointer data)
{
	g_printerr ("FAIL: copy did not finish within %d seconds\n",
		    COPY_TIMEOUT_SECONDS);
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

int
main (int argc, char *argv[])
{
	GtkWidget *window;
	GList *sources;
	GFile *dest;
	const char *how;
	char *tmp, *src_dir, *dst_dir;
	char *real_file, *link_path, *landed, *contents = NULL;
	guint timeout_id;

	test_init (&argc, &argv);

	how = (argc > 1) ? argv[1] : "keep";

	tmp = g_dir_make_tmp ("nemo-link-job-XXXXXX", NULL);
	src_dir = g_build_filename (tmp, "from", NULL);
	dst_dir = g_build_filename (tmp, "to", NULL);
	g_mkdir_with_parents (src_dir, 0700);
	g_mkdir_with_parents (dst_dir, 0700);

	real_file = g_build_filename (src_dir, "payload.txt", NULL);
	link_path = g_build_filename (src_dir, "pointer", NULL);
	landed = g_build_filename (dst_dir, "pointer", NULL);

	if (!(nemo_link_kinds_supported (src_dir) & NEMO_LINK_FILE_SYMLINK)) {
		g_printerr ("note: file symlinks are not permitted here, nothing to check\n");
		goto out;
	}

	check (g_file_set_contents (real_file, "payload", -1, NULL));
	check (nemo_link_create (real_file, link_path, NULL, NEMO_LINK_FILE_SYMLINK, NULL));

	g_setenv ("NEMO_LINK_COPY", how, TRUE);

	window = test_window_new ("link copy test", 5);
	gtk_widget_show (window);

	sources = g_list_prepend (NULL, g_file_new_for_path (link_path));
	dest = g_file_new_for_path (dst_dir);

	nemo_file_operations_copy (sources, NULL, dest, GTK_WINDOW (window),
				   copy_done, NULL);

	timeout_id = g_timeout_add_seconds (COPY_TIMEOUT_SECONDS, give_up, NULL);
	gtk_main ();
	g_source_remove (timeout_id);

	check (copy_finished);
	check (copy_succeeded);

	if (g_strcmp0 (how, "keep") == 0) {
		check (kind_of (landed) == NEMO_LINK_FILE_SYMLINK);
	} else {
		/* An ordinary file holding the contents. This is what every copy
		   used to do on Windows whether it was wanted or not. */
		check (kind_of (landed) == NEMO_LINK_NONE);
		if (g_file_get_contents (landed, &contents, NULL, NULL)) {
			check (g_strcmp0 (contents, "payload") == 0);
			g_free (contents);
		} else {
			check (FALSE);
		}
	}

	/* The original is untouched either way. */
	check (kind_of (link_path) == NEMO_LINK_FILE_SYMLINK);

	g_list_free_full (sources, g_object_unref);
	g_object_unref (dest);

 out:
	g_remove (landed);
	g_remove (link_path);
	g_remove (real_file);
	g_rmdir (dst_dir);
	g_rmdir (src_dir);
	g_rmdir (tmp);

	g_free (landed);
	g_free (link_path);
	g_free (real_file);
	g_free (dst_dir);
	g_free (src_dir);
	g_free (tmp);

	if (failures == 0) {
		g_print ("link copy job (%s): all checks passed\n", how);
	}

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
