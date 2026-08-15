/* Exercises the thumbnail factory's failure paths: a helper that never exits, an
 * image whose short side rounds away to nothing, and a .thumbnailer that goes bad
 * while it is being watched. Runs against a throwaway XDG_DATA_HOME so it sees
 * only its own thumbnailers, never the ones installed on the box.
 *
 * Re-runs itself as the hanging helper when handed --hang. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <libnemo-private/nemo-config.h>
#include <libnemo-private/nemo-desktop-thumbnail.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

#define HANG_MIME   "application/x-nemo-hang-test"
#define RELOAD_MIME "application/x-nemo-reload-test"

static char *thumbnailers_dir;
static char *work_dir;

static void
write_thumbnailer (const char *name, const char *contents)
{
	char *path = g_build_filename (thumbnailers_dir, name, NULL);

	if (!g_file_set_contents (path, contents, -1, NULL)) {
		g_printerr ("FAIL could not write %s\n", path);
		failures++;
	}

	g_free (path);
}

/* Exec lines are shell-parsed, so the path to the helper needs forward slashes
 * even on Windows - a backslash would be read as an escape. */
static char *
self_command (const char *self)
{
	char *absolute = g_canonicalize_filename (self, NULL);
	char *command;
	gsize i;

	for (i = 0; absolute[i] != '\0'; i++) {
		if (absolute[i] == '\\')
			absolute[i] = '/';
	}

	command = g_strdup_printf ("\"%s\" --hang %%i %%o", absolute);
	g_free (absolute);

	return command;
}

/* Writes a png of the given size and hands back its uri. */
static char *
write_image (const char *name, int width, int height)
{
	GdkPixbuf *pixbuf = gdk_pixbuf_new (GDK_COLORSPACE_RGB, FALSE, 8, width, height);
	char *path = g_build_filename (work_dir, name, NULL);
	char *uri;

	gdk_pixbuf_fill (pixbuf, 0x336699ff);
	gdk_pixbuf_save (pixbuf, path, "png", NULL, NULL);
	g_object_unref (pixbuf);

	uri = g_filename_to_uri (path, NULL, NULL);
	g_free (path);

	return uri;
}

/* --- a helper that never exits -------------------------------------------- */

static void
test_hung_thumbnailer (const char *self)
{
	NemoDesktopThumbnailFactory *factory;
	char *command = self_command (self);
	char *entry;
	char *uri;
	gint64 started;
	int elapsed;
	GdkPixbuf *pixbuf;

	entry = g_strdup_printf ("[Thumbnailer Entry]\nExec=%s\nMimeType=%s;\n",
				 command, HANG_MIME);
	write_thumbnailer ("hang.thumbnailer", entry);
	g_free (entry);
	g_free (command);

	factory = nemo_desktop_thumbnail_factory_new (NEMO_DESKTOP_THUMBNAIL_SIZE_NORMAL);
	uri = write_image ("hang-source.png", 64, 64);

	/* Without a timeout this never comes back, and the worker thread it is
	 * running on is gone for the rest of the session. */
	started = g_get_monotonic_time ();
	pixbuf = nemo_desktop_thumbnail_factory_generate_thumbnail (factory, uri, HANG_MIME);
	elapsed = (int) ((g_get_monotonic_time () - started) / G_USEC_PER_SEC);

	check (pixbuf == NULL);
	check (elapsed < 120);
	g_print ("  hung helper gave up after %ds\n", elapsed);

	g_clear_object (&pixbuf);
	g_free (uri);
	g_object_unref (factory);

	/* Out of the way of any factory built after this one. */
	entry = g_build_filename (thumbnailers_dir, "hang.thumbnailer", NULL);
	g_remove (entry);
	g_free (entry);
}

/* --- an image whose short side rounds to zero ----------------------------- */

static void
test_thin_image (void)
{
	NemoDesktopThumbnailFactory *factory;
	char *uri;
	GdkPixbuf *pixbuf;

	factory = nemo_desktop_thumbnail_factory_new (NEMO_DESKTOP_THUMBNAIL_SIZE_NORMAL);

	/* 5000x1 scaled to fit 128 asks for a height of 0.0256, which used to
	 * round down to no pixels at all. */
	uri = write_image ("thin.png", 5000, 1);

	pixbuf = nemo_desktop_thumbnail_factory_generate_thumbnail (factory, uri, "image/png");

	check (pixbuf != NULL);
	if (pixbuf != NULL) {
		check (gdk_pixbuf_get_height (pixbuf) >= 1);
		check (gdk_pixbuf_get_width (pixbuf) <= 128);
	}

	g_clear_object (&pixbuf);
	g_free (uri);
	g_object_unref (factory);
}

/* --- a .thumbnailer that goes bad under the monitor ------------------------ */

static void
test_thumbnailer_reload (void)
{
	NemoDesktopThumbnailFactory *factory;
	char *uri;
	gint64 deadline;
	gboolean dropped = FALSE;

	write_thumbnailer ("reload.thumbnailer",
			   "[Thumbnailer Entry]\nExec=true %i %o\nMimeType=" RELOAD_MIME ";\n");

	factory = nemo_desktop_thumbnail_factory_new (NEMO_DESKTOP_THUMBNAIL_SIZE_NORMAL);
	uri = write_image ("reload-source.png", 64, 64);

	check (nemo_desktop_thumbnail_factory_can_thumbnail (factory, uri, RELOAD_MIME, 0));

	/* No group header, so the reload fails and the entry has to go - the list
	 * node is freed underneath the walk that found it. */
	write_thumbnailer ("reload.thumbnailer", "not a key file at all\n");

	deadline = g_get_monotonic_time () + 10 * G_USEC_PER_SEC;
	while (g_get_monotonic_time () < deadline) {
		g_main_context_iteration (NULL, FALSE);

		if (!nemo_desktop_thumbnail_factory_can_thumbnail (factory, uri, RELOAD_MIME, 0)) {
			dropped = TRUE;
			break;
		}

		g_usleep (20000);
	}

	check (dropped);

	g_free (uri);
	g_object_unref (factory);
}

static gboolean
want (const char *name, int argc, char *argv[])
{
	int i;

	if (argc < 2)
		return TRUE;

	for (i = 1; i < argc; i++) {
		if (g_strcmp0 (argv[i], name) == 0)
			return TRUE;
	}

	return FALSE;
}

int
main (int argc, char *argv[])
{
	char *self = g_strdup (argv[0]);
	char *tmp;

	/* Re-entry as the thumbnailer that never finishes. */
	if (argc > 1 && g_strcmp0 (argv[1], "--hang") == 0) {
		for (;;)
			g_usleep (G_USEC_PER_SEC);
	}

	tmp = g_dir_make_tmp ("nemo-thumbnail-test-XXXXXX", NULL);

	/* Set before any glib call that would cache the real ones. */
	g_setenv ("XDG_CONFIG_HOME", tmp, TRUE);
	g_setenv ("XDG_DATA_HOME", tmp, TRUE);
	g_setenv ("XDG_DATA_DIRS", tmp, TRUE);
	g_setenv ("XDG_CACHE_HOME", tmp, TRUE);
	g_setenv ("HOME", tmp, TRUE);

	thumbnailers_dir = g_build_filename (tmp, "thumbnailers", NULL);
	work_dir = g_build_filename (tmp, "files", NULL);
	g_mkdir_with_parents (thumbnailers_dir, 0755);
	g_mkdir_with_parents (work_dir, 0755);

	gtk_init (&argc, &argv);
	g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL);
	nemo_config_init ();

	if (want ("hang", argc, argv))
		test_hung_thumbnailer (self);
	if (want ("thin", argc, argv))
		test_thin_image ();
	if (want ("reload", argc, argv))
		test_thumbnailer_reload ();

	nemo_config_shutdown ();
	g_free (thumbnailers_dir);
	g_free (work_dir);
	g_free (tmp);
	g_free (self);

	if (failures == 0)
		g_print ("nemo-thumbnail: all checks passed\n");

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
