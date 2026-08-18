/* An image nemo can decode itself must be recognised as one on Windows.
 *
 * The internal-thumbnail check asks whether gdk-pixbuf can load the file's mime
 * type. On win32 nemo's stored "mime type" is really the extension (".png"), so
 * the question was being asked about a string that is not a mime type at all and
 * the answer was always no. Losing that path is not a missing thumbnail - the
 * external thumbnailer still draws one - it is a thumbnail frozen at the small
 * cached size, which is why images went blurry as the zoom went up.
 *
 * Windows-only; Linux compiles it out. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#ifdef G_OS_WIN32

#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-private.h>
#include <libnemo-private/nemo-thumbnails.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

/* A NemoFile that has actually been told what it is - the answer depends on the
 * stored type, so a file whose info never arrived would prove nothing. */
static NemoFile *
seen_file (const char *path)
{
	GFile *location = g_file_new_for_path (path);
	NemoFile *file = nemo_file_get (location);
	GFileInfo *info;

	info = g_file_query_info (location, "standard::*",
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
	g_object_unref (location);

	if (info == NULL) {
		nemo_file_unref (file);
		return NULL;
	}

	nemo_file_update_info (file, info);
	g_object_unref (info);
	return file;
}

/* Write a real image of @format - a decodable file, not a renamed blob, so the
 * check is answering about something gdk-pixbuf could genuinely open. */
static gboolean
write_image (const char *path, const char *format)
{
	GdkPixbuf *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, 4, 4);
	gboolean ok;

	gdk_pixbuf_fill (pixbuf, 0x336699ff);
	ok = gdk_pixbuf_save (pixbuf, path, format, NULL, NULL);
	g_object_unref (pixbuf);

	return ok;
}

int
main (int argc, char *argv[])
{
	char *dir, *path;
	NemoFile *file;

	gtk_init_check (&argc, &argv);

	dir = g_dir_make_tmp ("nemo-thumb-XXXXXX", NULL);
	g_assert (dir != NULL);

	/* png and jpeg: the two formats gdk-pixbuf always has built in, so this
	 * cannot fail for want of a loader module on the box running it. */
	path = g_build_filename (dir, "picture.png", NULL);
	if (write_image (path, "png")) {
		file = seen_file (path);
		check (file != NULL);
		if (file != NULL) {
			g_autofree char *stored = nemo_file_get_mime_type (file);

			/* State the premise out loud: on win32 this really is
			 * the extension, which is why the conversion is needed. */
			if (g_strcmp0 (stored, "image/png") != 0) {
				g_print ("  note: stored type for a .png is [%s]\n", stored);
			}

			check (nemo_can_thumbnail_internally (file));
			nemo_file_unref (file);
		}
	} else {
		g_printerr ("  could not write a png; skipping that case\n");
	}
	g_unlink (path);
	g_free (path);

	path = g_build_filename (dir, "photo.jpeg", NULL);
	if (write_image (path, "jpeg")) {
		file = seen_file (path);
		check (file != NULL);
		if (file != NULL) {
			check (nemo_can_thumbnail_internally (file));
			nemo_file_unref (file);
		}
	} else {
		g_printerr ("  could not write a jpeg; skipping that case\n");
	}
	g_unlink (path);
	g_free (path);

	/* The other side of the answer. Without these a check that simply said
	 * yes to everything would pass, and nemo would try to decode text. */
	path = g_build_filename (dir, "notes.txt", NULL);
	check (g_file_set_contents (path, "not an image", -1, NULL));
	file = seen_file (path);
	check (file != NULL);
	if (file != NULL) {
		check (!nemo_can_thumbnail_internally (file));
		nemo_file_unref (file);
	}
	g_unlink (path);
	g_free (path);

	path = g_build_filename (dir, "mystery.zzz", NULL);
	check (g_file_set_contents (path, "unknown to everyone", -1, NULL));
	file = seen_file (path);
	check (file != NULL);
	if (file != NULL) {
		check (!nemo_can_thumbnail_internally (file));
		nemo_file_unref (file);
	}
	g_unlink (path);
	g_free (path);

	g_rmdir (dir);
	g_free (dir);

	if (failures == 0) {
		g_print ("thumbnail-win32: all checks passed\n");
	}
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#else /* !G_OS_WIN32 */

int
main (int argc, char *argv[])
{
	return EXIT_SUCCESS;
}

#endif
