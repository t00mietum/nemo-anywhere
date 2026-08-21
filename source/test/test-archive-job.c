/* Compressing for real: a folder of items is handed to the job both ways -
 * everything into one archive, and one archive per item - and what lands on
 * disk is read back with libarchive. What goes in each archive is the point,
 * so the entry names are checked rather than just the file being there. */

#include "test.h"

#include <libnemo-private/nemo-archive.h>

#include <archive.h>
#include <archive_entry.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>

#define ARCHIVE_TIMEOUT_SECONDS 30

static int failures;
static gboolean job_finished;
static gboolean job_succeeded;

#define check(expr)							\
	G_STMT_START {							\
		if (!(expr)) {						\
			g_printerr ("FAIL %s:%d: %s\n",			\
				    __FILE__, __LINE__, #expr);		\
			failures++;					\
		}							\
	} G_STMT_END

static void
archive_done (GFile    *result,
	      gboolean  success,
	      gpointer  data)
{
	job_succeeded = success;
	job_finished = TRUE;
	gtk_main_quit ();
}

static gboolean
give_up (gpointer data)
{
	g_printerr ("FAIL: compressing did not finish within %d seconds\n",
		    ARCHIVE_TIMEOUT_SECONDS);
	failures++;
	gtk_main_quit ();

	return G_SOURCE_REMOVE;
}

static void
wait_for_job (void)
{
	guint timeout_id = g_timeout_add_seconds (ARCHIVE_TIMEOUT_SECONDS, give_up, NULL);

	gtk_main ();
	g_source_remove (timeout_id);

	check (job_finished);
	check (job_succeeded);
}

static void
write_file (const char *dir,
	    const char *name,
	    const char *contents)
{
	char *path = g_build_filename (dir, name, NULL);

	check (g_file_set_contents (path, contents, -1, NULL));

	g_free (path);
}

/* Every entry in the archive, '/'-separated, in the order libarchive hands
   them over. NULL when the file could not be opened as an archive at all. */
static char **
archive_entries (const char *path)
{
	struct archive *a = archive_read_new ();
	struct archive_entry *entry;
	GPtrArray *names;

	archive_read_support_format_all (a);
	archive_read_support_filter_all (a);

	if (archive_read_open_filename (a, path, 16384) != ARCHIVE_OK) {
		archive_read_free (a);
		return NULL;
	}

	names = g_ptr_array_new ();

	while (archive_read_next_header (a, &entry) == ARCHIVE_OK) {
		g_ptr_array_add (names, g_strdup (archive_entry_pathname (entry)));
	}

	g_ptr_array_add (names, NULL);
	archive_read_free (a);

	return (char **) g_ptr_array_free (names, FALSE);
}

static gboolean
holds_entry (char       **entries,
	     const char  *wanted)
{
	int i;

	for (i = 0; entries != NULL && entries[i] != NULL; i++) {
		if (g_strcmp0 (entries[i], wanted) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static GList *
sources_in (const char *dir,
	    const char * const *names)
{
	GList *sources = NULL;
	int i;

	for (i = 0; names[i] != NULL; i++) {
		char *path = g_build_filename (dir, names[i], NULL);

		sources = g_list_append (sources, g_file_new_for_path (path));
		g_free (path);
	}

	return sources;
}

/* One archive holding the whole selection, which is what the dialog does with
   the box unticked. */
static void
check_one_archive (const char         *source_dir,
		   const char         *out_dir,
		   const char * const *names,
		   GtkWidget          *window)
{
	GList *sources = sources_in (source_dir, names);
	char *path = g_build_filename (out_dir, "everything.zip", NULL);
	GFile *destination = g_file_new_for_path (path);
	NemoArchiveOptions options;
	char **entries;

	nemo_archive_options_init (&options);
	options.format = NEMO_ARCHIVE_FORMAT_ZIP;

	job_finished = FALSE;
	job_succeeded = FALSE;
	nemo_archive_create (sources, destination, &options, GTK_WINDOW (window),
			     archive_done, NULL);
	wait_for_job ();

	entries = archive_entries (path);
	check (entries != NULL);
	check (holds_entry (entries, "one.txt"));
	check (holds_entry (entries, "two.txt"));
	check (holds_entry (entries, "sub/inner.txt"));
	g_strfreev (entries);

	nemo_archive_options_clear (&options);
	g_list_free_full (sources, g_object_unref);
	g_object_unref (destination);
	g_free (path);
}

/* The same selection with the box ticked: an archive apiece, each named after
   its item and each holding only that item. */
static void
check_each_archive (const char         *source_dir,
		    const char         *out_dir,
		    const char * const *names,
		    GtkWidget          *window)
{
	GList *sources = sources_in (source_dir, names);
	GFile *out = g_file_new_for_path (out_dir);
	NemoArchiveOptions options;
	char *path;
	char **entries;

	nemo_archive_options_init (&options);
	options.format = NEMO_ARCHIVE_FORMAT_ZIP;

	job_finished = FALSE;
	job_succeeded = FALSE;
	nemo_archive_create_each (sources, out, &options, GTK_WINDOW (window),
				  archive_done, NULL);
	wait_for_job ();

	/* The item's whole name is kept, so a file keeps its extension. */
	path = g_build_filename (out_dir, "one.txt.zip", NULL);
	entries = archive_entries (path);
	check (entries != NULL);
	check (holds_entry (entries, "one.txt"));
	check (!holds_entry (entries, "two.txt"));
	g_strfreev (entries);
	g_free (path);

	path = g_build_filename (out_dir, "two.txt.zip", NULL);
	entries = archive_entries (path);
	check (entries != NULL);
	check (holds_entry (entries, "two.txt"));
	check (!holds_entry (entries, "one.txt"));
	g_strfreev (entries);
	g_free (path);

	/* A folder brings what is inside it, and nothing from beside it. */
	path = g_build_filename (out_dir, "sub.zip", NULL);
	entries = archive_entries (path);
	check (entries != NULL);
	check (holds_entry (entries, "sub/inner.txt"));
	check (!holds_entry (entries, "one.txt"));
	g_strfreev (entries);
	g_free (path);

	/* Nothing was written under the name the whole selection would have
	   used, so the two ways cannot be confused for each other. */
	path = g_build_filename (out_dir, "everything.zip", NULL);
	check (!g_file_test (path, G_FILE_TEST_EXISTS));
	g_free (path);

	nemo_archive_options_clear (&options);
	g_list_free_full (sources, g_object_unref);
	g_object_unref (out);
}

/* An archive in the selection keeps its suffix rather than swapping it, or the
   archive being written would land on the file being read. */
static void
check_archive_of_archive (const char *out_dir,
			  GtkWidget  *window)
{
	static const char * const names[] = { "one.txt.zip", NULL };
	GList *sources = sources_in (out_dir, names);
	GFile *out = g_file_new_for_path (out_dir);
	NemoArchiveOptions options;
	char *path;
	char **entries;

	nemo_archive_options_init (&options);
	options.format = NEMO_ARCHIVE_FORMAT_ZIP;

	job_finished = FALSE;
	job_succeeded = FALSE;
	nemo_archive_create_each (sources, out, &options, GTK_WINDOW (window),
				  archive_done, NULL);
	wait_for_job ();

	path = g_build_filename (out_dir, "one.txt.zip.zip", NULL);
	entries = archive_entries (path);
	check (entries != NULL);
	check (holds_entry (entries, "one.txt.zip"));
	g_strfreev (entries);
	g_free (path);

	/* The file it was made from is still what it was. */
	path = g_build_filename (out_dir, "one.txt.zip", NULL);
	entries = archive_entries (path);
	check (entries != NULL);
	check (holds_entry (entries, "one.txt"));
	g_strfreev (entries);
	g_free (path);

	nemo_archive_options_clear (&options);
	g_list_free_full (sources, g_object_unref);
	g_object_unref (out);
}

static void
remove_tree (const char *path)
{
	GDir *dir = g_dir_open (path, 0, NULL);

	if (dir != NULL) {
		const char *name;

		while ((name = g_dir_read_name (dir)) != NULL) {
			char *child = g_build_filename (path, name, NULL);

			remove_tree (child);
			g_free (child);
		}

		g_dir_close (dir);
		g_rmdir (path);
	} else {
		g_remove (path);
	}
}

int
main (int argc, char *argv[])
{
	static const char * const names[] = { "one.txt", "two.txt", "sub", NULL };
	GtkWidget *window;
	char *tmp;
	char *source_dir;
	char *sub_dir;
	char *one_dir;
	char *each_dir;

	test_init (&argc, &argv);

	tmp = g_dir_make_tmp ("nemo-archive-test-XXXXXX", NULL);
	source_dir = g_build_filename (tmp, "items", NULL);
	sub_dir = g_build_filename (source_dir, "sub", NULL);
	one_dir = g_build_filename (tmp, "one", NULL);
	each_dir = g_build_filename (tmp, "each", NULL);

	g_mkdir_with_parents (sub_dir, 0700);
	g_mkdir_with_parents (one_dir, 0700);
	g_mkdir_with_parents (each_dir, 0700);

	write_file (source_dir, "one.txt", "first");
	write_file (source_dir, "two.txt", "second");
	write_file (sub_dir, "inner.txt", "inner");

	window = test_window_new ("archive test", 5);
	gtk_widget_show (window);

	check_one_archive (source_dir, one_dir, names, window);
	check_each_archive (source_dir, each_dir, names, window);
	check_archive_of_archive (each_dir, window);

	remove_tree (tmp);

	g_free (each_dir);
	g_free (one_dir);
	g_free (sub_dir);
	g_free (source_dir);
	g_free (tmp);

	if (failures == 0) {
		g_print ("archive: all checks passed\n");
	}

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
