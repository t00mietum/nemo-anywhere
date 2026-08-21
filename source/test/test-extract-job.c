/* Unpacking for real: a zip is written with libarchive, handed to the job, and
 * the folder it lands in is inspected. Covers the two layouts, the paths an
 * archive can name, and the guard that keeps a hostile one from writing outside
 * the folder that was picked. Collisions are not covered here - every answer to
 * one comes from a dialog, and there is nobody to click it. */

#include "test.h"

#include <libnemo-private/nemo-extract.h>

#include <archive.h>
#include <archive_entry.h>
#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>

#define EXTRACT_TIMEOUT_SECONDS 30

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
extract_done (GFile    *destination_dir,
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
	g_printerr ("FAIL: unpacking did not finish within %d seconds\n",
		    EXTRACT_TIMEOUT_SECONDS);
	failures++;
	gtk_main_quit ();

	return G_SOURCE_REMOVE;
}

static void
add_file_entry (struct archive *a,
		const char     *path,
		const char     *contents)
{
	struct archive_entry *entry = archive_entry_new ();

	archive_entry_set_pathname (entry, path);
	archive_entry_set_size (entry, (la_int64_t) strlen (contents));
	archive_entry_set_filetype (entry, AE_IFREG);
	archive_entry_set_perm (entry, 0644);
	archive_entry_set_mtime (entry, 1000000000, 0);

	check (archive_write_header (a, entry) == ARCHIVE_OK);
	check (archive_write_data (a, contents, strlen (contents)) == (la_ssize_t) strlen (contents));

	archive_entry_free (entry);
}

static void
add_dir_entry (struct archive *a,
	       const char     *path)
{
	struct archive_entry *entry = archive_entry_new ();

	archive_entry_set_pathname (entry, path);
	archive_entry_set_size (entry, 0);
	archive_entry_set_filetype (entry, AE_IFDIR);
	archive_entry_set_perm (entry, 0755);

	check (archive_write_header (a, entry) == ARCHIVE_OK);

	archive_entry_free (entry);
}

/* A folder with a file at its root and one a level down, plus an entry whose
   stored path climbs out of wherever it is unpacked. */
static void
write_test_zip (const char *path)
{
	struct archive *a = archive_write_new ();

	check (archive_write_set_format_zip (a) == ARCHIVE_OK);
	check (archive_write_open_filename (a, path) == ARCHIVE_OK);

	add_dir_entry (a, "photos/");
	add_file_entry (a, "photos/one.txt", "first");
	add_dir_entry (a, "photos/sub/");
	add_file_entry (a, "photos/sub/two.txt", "second");
	add_file_entry (a, "../escape.txt", "should not escape");

	check (archive_write_close (a) == ARCHIVE_OK);
	archive_write_free (a);
}

static void
run_job (const char        *archive_path,
	 const char        *destination,
	 NemoExtractLayout  layout,
	 GtkWidget         *window)
{
	GList *archives = NULL;
	GFile *dest;
	guint timeout_id;

	job_finished = FALSE;
	job_succeeded = FALSE;

	archives = g_list_prepend (archives, g_file_new_for_path (archive_path));
	dest = g_file_new_for_path (destination);

	nemo_extract_files (archives, dest, layout, GTK_WINDOW (window), extract_done, NULL);

	/* The job runs on a worker; without a deadline a hang would sit here
	   until meson's own timeout killed the run. */
	timeout_id = g_timeout_add_seconds (EXTRACT_TIMEOUT_SECONDS, give_up, NULL);
	gtk_main ();
	g_source_remove (timeout_id);

	check (job_finished);
	check (job_succeeded);

	g_list_free_full (archives, g_object_unref);
	g_object_unref (dest);
}

static void
check_contents (const char *path,
		const char *expected)
{
	char *contents = NULL;

	if (g_file_get_contents (path, &contents, NULL, NULL)) {
		check (g_strcmp0 (contents, expected) == 0);
		g_free (contents);
	} else {
		g_printerr ("FAIL: %s could not be read\n", path);
		failures++;
	}
}

static void
check_here_layout (const char *tmp,
		   const char *archive_path,
		   GtkWidget  *window)
{
	char *dest = g_build_filename (tmp, "here", NULL);
	char *one, *two, *escaped, *outside;

	g_mkdir_with_parents (dest, 0700);
	run_job (archive_path, dest, NEMO_EXTRACT_HERE, window);

	/* The archive holds a folder, so unpacking here produces that folder
	   rather than its contents loose. */
	one = g_build_filename (dest, "photos", "one.txt", NULL);
	two = g_build_filename (dest, "photos", "sub", "two.txt", NULL);
	check (g_file_test (one, G_FILE_TEST_IS_REGULAR));
	check (g_file_test (two, G_FILE_TEST_IS_REGULAR));
	check_contents (one, "first");
	check_contents (two, "second");

	/* The climbing entry landed inside the folder that was picked, and
	   nothing appeared beside it. */
	escaped = g_build_filename (dest, "escape.txt", NULL);
	outside = g_build_filename (tmp, "escape.txt", NULL);
	check (g_file_test (escaped, G_FILE_TEST_IS_REGULAR));
	check (!g_file_test (outside, G_FILE_TEST_EXISTS));

	g_free (one);
	g_free (two);
	g_free (escaped);
	g_free (outside);
	g_free (dest);
}

static void
check_subfolder_layout (const char *tmp,
			const char *archive_path,
			GtkWidget  *window)
{
	char *dest = g_build_filename (tmp, "each", NULL);
	char *one;

	g_mkdir_with_parents (dest, 0700);
	run_job (archive_path, dest, NEMO_EXTRACT_TO_SUBFOLDER, window);

	/* A folder named after the archive, holding what the archive holds -
	   here that is the archive's own "photos" folder, one level further in.
	   The layout is literal on purpose: what it is for is an archive that
	   would otherwise scatter, and guessing would make it unpredictable. */
	one = g_build_filename (dest, "photos", "photos", "one.txt", NULL);
	check (g_file_test (one, G_FILE_TEST_IS_REGULAR));
	check_contents (one, "first");

	g_free (one);
	g_free (dest);
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
	GtkWidget *window;
	char *tmp;
	char *archive_path;

	test_init (&argc, &argv);

	tmp = g_dir_make_tmp ("nemo-extract-test-XXXXXX", NULL);
	archive_path = g_build_filename (tmp, "photos.zip", NULL);

	write_test_zip (archive_path);
	check (g_file_test (archive_path, G_FILE_TEST_IS_REGULAR));

	window = test_window_new ("extract test", 5);
	gtk_widget_show (window);

	check_here_layout (tmp, archive_path, window);
	check_subfolder_layout (tmp, archive_path, window);

	remove_tree (tmp);

	g_free (archive_path);
	g_free (tmp);

	if (failures == 0) {
		g_print ("extract: all checks passed\n");
	}

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
