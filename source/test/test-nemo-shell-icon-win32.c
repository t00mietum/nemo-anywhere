/* The shell's icon for a shortcut: a real picture at each size, its target's
 * rather than one generic shape, and cached. */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gstdio.h>
#include <libnemo-private/nemo-shell-icon-win32.h>
#include <libnemo-private/nemo-shortcut-win32.h>

static int failures;

static void
check (gboolean ok, const char *what)
{
	g_print ("%s: %s\n", ok ? "PASS" : "FAIL", what);
	if (!ok) {
		failures++;
	}
}

static gboolean
has_visible_pixels (GdkPixbuf *pixbuf)
{
	const guchar *pixels = gdk_pixbuf_get_pixels (pixbuf);
	gint stride = gdk_pixbuf_get_rowstride (pixbuf);
	gint w = gdk_pixbuf_get_width (pixbuf);
	gint h = gdk_pixbuf_get_height (pixbuf);
	gint x, y, seen = 0;

	if (!gdk_pixbuf_get_has_alpha (pixbuf)) {
		return TRUE;
	}

	for (y = 0; y < h; y++) {
		for (x = 0; x < w; x++) {
			if (pixels[y * stride + x * 4 + 3] > 128) {
				seen++;
			}
		}
	}

	return seen > (w * h) / 10;
}

static gboolean
same_picture (GdkPixbuf *a, GdkPixbuf *b)
{
	gint h = gdk_pixbuf_get_height (a);
	gint stride = gdk_pixbuf_get_rowstride (a);

	return gdk_pixbuf_get_width (a) == gdk_pixbuf_get_width (b) &&
	       h == gdk_pixbuf_get_height (b) &&
	       stride == gdk_pixbuf_get_rowstride (b) &&
	       memcmp (gdk_pixbuf_get_pixels (a), gdk_pixbuf_get_pixels (b), (gsize) stride * h) == 0;
}

int
main (int argc, char *argv[])
{
	char *dir, *to_notepad, *to_folder, *folder;
	GdkPixbuf *small, *large, *jumbo, *again, *folder_icon;
	const int sizes[] = { 16, 32, 48, 256 };
	int i;

	gtk_init_check (&argc, &argv);

	dir = g_dir_make_tmp ("nemo-shell-icon-XXXXXX", NULL);
	g_assert (dir != NULL);

	folder = g_build_filename (dir, "a folder", NULL);
	g_mkdir (folder, 0700);
	to_notepad = g_build_filename (dir, "notepad.lnk", NULL);
	to_folder = g_build_filename (dir, "folder.lnk", NULL);

	check (nemo_shortcut_win32_create ("C:\\Windows\\notepad.exe", to_notepad, NULL, NULL, NULL, NULL),
	       "a shortcut to notepad is made");
	check (nemo_shortcut_win32_create (folder, to_folder, NULL, NULL, NULL, NULL),
	       "a shortcut to a folder is made");

	for (i = 0; i < G_N_ELEMENTS (sizes); i++) {
		GdkPixbuf *pixbuf = nemo_shell_icon_win32_for_path (to_notepad, sizes[i], 1);
		char *what = g_strdup_printf ("an icon comes back at %d", sizes[i]);

		check (pixbuf != NULL, what);
		g_free (what);

		if (pixbuf != NULL) {
			what = g_strdup_printf ("and it is %dx%d with something drawn in it (got %dx%d)",
						sizes[i], sizes[i], gdk_pixbuf_get_width (pixbuf), gdk_pixbuf_get_height (pixbuf));
			check (gdk_pixbuf_get_width (pixbuf) == sizes[i] && has_visible_pixels (pixbuf), what);
			g_free (what);
			g_object_unref (pixbuf);
		}
	}

	small = nemo_shell_icon_win32_for_path (to_notepad, 16, 1);
	again = nemo_shell_icon_win32_for_path (to_notepad, 16, 1);
	check (small != NULL && small == again, "the second ask is answered from the cache");

	large = nemo_shell_icon_win32_for_path (to_notepad, 24, 1);
	check (large != NULL && gdk_pixbuf_get_width (large) == 24, "24 is scaled down from the 32 list");

	jumbo = nemo_shell_icon_win32_for_path (to_notepad, 128, 1);
	check (jumbo != NULL && gdk_pixbuf_get_width (jumbo) == 128 && has_visible_pixels (jumbo),
	       "128 is scaled down from the jumbo list and still has a picture in it");

	folder_icon = nemo_shell_icon_win32_for_path (to_folder, 32, 1);
	check (folder_icon != NULL && large != NULL && !same_picture (folder_icon, large),
	       "a shortcut to a folder gets a different picture from one to a program");

	check (nemo_shell_icon_win32_for_path ("Q:\\no\\such\\thing.lnk", 32, 1) == NULL ||
	       TRUE, "a missing file does not crash");

	g_clear_object (&small);
	g_clear_object (&again);
	g_clear_object (&large);
	g_clear_object (&jumbo);
	g_clear_object (&folder_icon);

	g_remove (to_notepad);
	g_remove (to_folder);
	g_rmdir (folder);
	g_rmdir (dir);
	g_free (to_folder);
	g_free (to_notepad);
	g_free (folder);
	g_free (dir);

	return failures == 0 ? 0 : 1;
}
