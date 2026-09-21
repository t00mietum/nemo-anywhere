/* A folder that is mostly images opens at its own default size. What counts as
 * mostly images is two rules at once - enough pictures to be a gallery, and
 * more pictures than anything else - and folders are left out of the count. */

#include <config.h>

#include <stdlib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <libnemo-private/nemo-directory.h>
#include <libnemo-private/nemo-file.h>

#include "test-scratch.h"
#include "test-check.h"

/* The directory only keeps its file list while something is watching it, which
 * in the program is always the view. A plain call_when_ready hands the files to
 * the callback and lets go of them again. */
static gboolean
wait_for_directory (NemoDirectory *directory)
{
	int spins;

	for (spins = 0; spins < 5000; spins++) {
		if (nemo_directory_are_all_files_seen (directory)) {
			return TRUE;
		}
		g_main_context_iteration (NULL, FALSE);
		g_usleep (2000);
	}

	return FALSE;
}

static void
write_image (const char *dir, int n)
{
	GdkPixbuf *pixbuf;
	char *path;

	path = g_strdup_printf ("%s/shot-%d.png", dir, n);
	pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 4, 4);
	gdk_pixbuf_fill (pixbuf, 0x336699ff);
	check (gdk_pixbuf_save (pixbuf, path, "png", NULL, NULL));

	g_object_unref (pixbuf);
	g_free (path);
}

static void
write_text (const char *dir, int n)
{
	char *path = g_strdup_printf ("%s/notes-%d.txt", dir, n);

	check (g_file_set_contents (path, "x\n", 2, NULL));
	g_free (path);
}

static void
write_folder (const char *dir, int n)
{
	char *path = g_strdup_printf ("%s/sub-%d", dir, n);

	check (g_mkdir (path, 0755) == 0);
	g_free (path);
}

/* Builds a folder with the given contents and asks. */
static gboolean
mostly_images (const char *base, const char *name,
	       int images, int texts, int folders)
{
	NemoDirectory *directory;
	char *dir, *uri;
	gboolean answer;
	int client;
	int i;

	dir = g_build_filename (base, name, NULL);
	check (g_mkdir (dir, 0755) == 0);

	for (i = 0; i < images; i++) {
		write_image (dir, i);
	}
	for (i = 0; i < texts; i++) {
		write_text (dir, i);
	}
	for (i = 0; i < folders; i++) {
		write_folder (dir, i);
	}

	uri = g_filename_to_uri (dir, NULL, NULL);
	directory = nemo_directory_get_by_uri (uri);

	nemo_directory_file_monitor_add (directory, &client, TRUE,
					 NEMO_FILE_ATTRIBUTE_INFO, NULL, NULL);
	check (wait_for_directory (directory));

	answer = nemo_directory_is_mostly_images (directory);

	nemo_directory_file_monitor_remove (directory, &client);
	nemo_directory_unref (directory);
	g_free (uri);
	g_free (dir);

	return answer;
}

int
main (int argc, char **argv)
{
	char *tmp;

	gtk_init_check (&argc, &argv);

	tmp = test_scratch_dir ("nemo-image-folder-XXXXXX", NULL);
	if (tmp == NULL) {
		g_printerr ("FAIL could not make a temp dir\n");
		return EXIT_FAILURE;
	}

	check (!mostly_images (tmp, "empty", 0, 0, 0));

	/* One picture in a folder is not a gallery, whatever share it is of the
	   files there. */
	check (!mostly_images (tmp, "one-image", 1, 0, 0));
	check (!mostly_images (tmp, "one-of-two", 1, 1, 0));

	check (mostly_images (tmp, "two-images", 2, 0, 0));
	check (mostly_images (tmp, "half-images", 2, 2, 0));
	check (!mostly_images (tmp, "outnumbered", 2, 3, 0));
	check (mostly_images (tmp, "gallery", 6, 2, 0));

	/* Folders are not counted, so a set of pictures filed into sub-folders
	   still reads as a folder of pictures. */
	check (mostly_images (tmp, "with-folders", 2, 0, 5));
	check (!mostly_images (tmp, "folders-and-text", 2, 3, 5));

	g_free (tmp);

	if (failures == 0)
		g_print ("nemo-image-folder: all checks passed\n");

	return failures == 0 ? 0 : 1;
}
