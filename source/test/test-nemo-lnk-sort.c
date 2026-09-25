/* With folders first, a Windows shortcut to a folder sorts with the folders,
 * and one to a file stays with the files. Whether the target is a folder is
 * what the shortcut records, the same thing that gives it the folder icon. */

#include <config.h>

#include <stdlib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-attributes.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-lnk.h>

#include "test-scratch.h"
#include "test-check.h"

static NemoFile *
loaded (const char *dir, const char *name)
{
	g_autofree char *path = g_build_filename (dir, name, NULL);
	g_autofree char *uri = g_filename_to_uri (path, NULL, NULL);
	NemoFile *file = nemo_file_get_by_uri (uri);
	int spins;

	nemo_file_monitor_add (file, file, NEMO_FILE_ATTRIBUTE_INFO);
	for (spins = 0; spins < 5000 && !nemo_file_check_if_ready (file, NEMO_FILE_ATTRIBUTE_INFO); spins++) {
		g_main_context_iteration (NULL, FALSE);
		g_usleep (2000);
	}
	check (nemo_file_check_if_ready (file, NEMO_FILE_ATTRIBUTE_INFO));

	return file;
}

static int
by_name (NemoFile *a, NemoFile *b)
{
	return nemo_file_compare_for_sort (a, b, NEMO_FILE_SORT_BY_DISPLAY_NAME,
					   TRUE, FALSE, FALSE, NULL);
}

int
main (int argc, char **argv)
{
	g_autofree char *dir = NULL, *folder = NULL, *doc = NULL;
	g_autofree char *to_folder = NULL, *to_doc = NULL;
	NemoFile *f_folder, *f_doc, *f_to_folder, *f_to_doc;
	char *tmp;

	gtk_init_check (&argc, &argv);

	tmp = test_scratch_config_home ("nemo-lnk-sort-XXXXXX");
	nemo_global_preferences_init ();

	dir = g_build_filename (tmp, "here", NULL);
	check (g_mkdir (dir, 0755) == 0);

	/* Names chosen so that by name alone each shortcut would sort on the
	   other side of what it points at. */
	folder = g_build_filename (dir, "m folder", NULL);
	doc = g_build_filename (dir, "b doc.txt", NULL);
	to_folder = g_build_filename (dir, "z to folder.lnk", NULL);
	to_doc = g_build_filename (dir, "a to doc.lnk", NULL);
	check (g_mkdir (folder, 0755) == 0);
	check (g_file_set_contents (doc, "x", -1, NULL));
	check (nemo_lnk_write (to_folder, folder, NEMO_LNK_ABSOLUTE, NULL));
	check (nemo_lnk_write (to_doc, doc, NEMO_LNK_ABSOLUTE, NULL));

	f_folder = loaded (dir, "m folder");
	f_doc = loaded (dir, "b doc.txt");
	f_to_folder = loaded (dir, "z to folder.lnk");
	f_to_doc = loaded (dir, "a to doc.lnk");

	/* The folder shortcut comes before every file, and after the folder
	   by name. */
	check (by_name (f_to_folder, f_doc) < 0);
	check (by_name (f_to_folder, f_to_doc) < 0);
	check (by_name (f_folder, f_to_folder) < 0);

	/* The file shortcut is a file, ordered by name among the files. */
	check (by_name (f_folder, f_to_doc) < 0);
	check (by_name (f_to_doc, f_doc) < 0);

	nemo_file_monitor_remove (f_folder, f_folder);
	nemo_file_monitor_remove (f_doc, f_doc);
	nemo_file_monitor_remove (f_to_folder, f_to_folder);
	nemo_file_monitor_remove (f_to_doc, f_to_doc);
	nemo_file_unref (f_folder);
	nemo_file_unref (f_doc);
	nemo_file_unref (f_to_folder);
	nemo_file_unref (f_to_doc);
	g_free (tmp);

	if (failures == 0) {
		g_print ("nemo-lnk-sort: all checks passed\n");
	}

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
