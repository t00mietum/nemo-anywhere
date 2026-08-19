/* Exercises theme discovery and the light/dark filtering behind the Appearance
 * settings: which themes are offered for the mode in force, how a theme that
 * never declared a mode is judged, and which variant is actually applied.
 * Runs against a throwaway config root holding hand-built theme folders. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include <libnemo-private/nemo-appearance.h>
#include <libnemo-private/nemo-config.h>
#include <libnemo-private/nemo-global-preferences.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

static char *root;

static void
write_file (const char *path, const char *text)
{
	char *dir = g_path_get_dirname (path);

	g_mkdir_with_parents (dir, 0755);
	g_free (dir);

	if (!g_file_set_contents (path, text, -1, NULL)) {
		g_printerr ("could not write %s\n", path);
		failures++;
	}
}

static void
make_icon_theme (const char *name, const char *extra_keys)
{
	char *index;
	char *icon;
	char *text;

	index = g_build_filename (root, "icons", name, "index.theme", NULL);
	text = g_strdup_printf ("[Icon Theme]\n"
				"Name=%s\n"
				"%s"
				"Directories=scalable/places\n"
				"\n"
				"[scalable/places]\n"
				"Size=48\n"
				"Context=Places\n"
				"Type=Scalable\n",
				name, extra_keys != NULL ? extra_keys : "");
	write_file (index, text);
	g_free (text);
	g_free (index);

	icon = g_build_filename (root, "icons", name, "scalable", "places", "folder.svg", NULL);
	write_file (icon, "<svg xmlns=\"http://www.w3.org/2000/svg\"/>");
	g_free (icon);
}

static void
make_widget_theme (const char *name, gboolean with_dark_sheet, const char *extra_keys)
{
	char *path;

	path = g_build_filename (root, "themes", name, "gtk-3.0", "gtk.css", NULL);
	write_file (path, "/* test */\n");
	g_free (path);

	if (with_dark_sheet) {
		path = g_build_filename (root, "themes", name, "gtk-3.0", "gtk-dark.css", NULL);
		write_file (path, "/* test dark */\n");
		g_free (path);
	}

	if (extra_keys != NULL) {
		char *text = g_strdup_printf ("[Desktop Entry]\nType=X-GNOME-Metatheme\nName=%s\n%s",
					      name, extra_keys);
		path = g_build_filename (root, "themes", name, "index.theme", NULL);
		write_file (path, text);
		g_free (text);
		g_free (path);
	}
}

static NemoThemeInfo *
find (GList *themes, const char *name)
{
	GList *node;

	for (node = themes; node != NULL; node = node->next) {
		NemoThemeInfo *info = node->data;

		if (strcmp (info->name, name) == 0) {
			return info;
		}
	}

	return NULL;
}

static void
set_mode (const char *mode)
{
	nemo_config_set_string (nemo_appearance_preferences,
				NEMO_PREFERENCES_APPEARANCE_MODE, mode);
}

/* A theme that never said which background it was drawn for is judged by its
 * name. That guess is what decides whether it appears in the picker at all. */
static void
test_inferred_pairing (void)
{
	GList         *themes;
	NemoThemeInfo *info;

	set_mode ("light");

	themes = nemo_appearance_list_themes (NEMO_THEME_KIND_ICON, NEMO_THEME_FITS_LIGHT);

	info = find (themes, "ZzTestPair");
	check (info != NULL);
	if (info != NULL) {
		check (info->fits == NEMO_THEME_FITS_LIGHT);
		check (g_strcmp0 (info->counterpart, "ZzTestPair-dark") == 0);
	}

	/* The dark half of a pair has no business in the light list. */
	check (find (themes, "ZzTestPair-dark") == NULL);

	/* No suffix and no dark sibling means it serves both. */
	info = find (themes, "ZzTestLone");
	check (info != NULL);
	if (info != NULL) {
		check (info->fits == NEMO_THEME_FITS_BOTH);
		check (info->counterpart == NULL);
	}

	/* hicolor is the end of every fallback chain, never a choice. */
	check (find (themes, "hicolor") == NULL);

	g_list_free_full (themes, (GDestroyNotify) nemo_theme_info_free);

	set_mode ("dark");
	themes = nemo_appearance_list_themes (NEMO_THEME_KIND_ICON, NEMO_THEME_FITS_DARK);

	check (find (themes, "ZzTestPair") == NULL);
	info = find (themes, "ZzTestPair-dark");
	check (info != NULL);
	if (info != NULL) {
		check (info->fits == NEMO_THEME_FITS_DARK);
		check (g_strcmp0 (info->counterpart, "ZzTestPair") == 0);
	}
	check (find (themes, "ZzTestLone") != NULL);

	g_list_free_full (themes, (GDestroyNotify) nemo_theme_info_free);
}

/* X-Nemo-Modes is the theme's own answer and outranks the name. */
static void
test_declared_modes (void)
{
	GList         *themes;
	NemoThemeInfo *info;

	set_mode ("light");
	themes = nemo_appearance_list_themes (NEMO_THEME_KIND_ICON, NEMO_THEME_FITS_LIGHT);

	/* Named like a dark theme, declares itself fit for both. */
	info = find (themes, "ZzTestDeclared-dark");
	check (info != NULL);
	if (info != NULL) {
		check (info->fits == NEMO_THEME_FITS_BOTH);
		check (g_strcmp0 (info->style, "Test Style") == 0);
	}

	g_list_free_full (themes, (GDestroyNotify) nemo_theme_info_free);
}

/* A widget theme carrying its own dark sheet needs no counterpart: GTK swaps
 * between gtk.css and gtk-dark.css without the name changing. */
static void
test_widget_dark_sheet (void)
{
	GList         *themes;
	NemoThemeInfo *info;

	set_mode ("dark");
	themes = nemo_appearance_list_themes (NEMO_THEME_KIND_WIDGET, NEMO_THEME_FITS_DARK);

	info = find (themes, "ZzWidgetBoth");
	check (info != NULL);
	if (info != NULL) {
		check (info->fits == NEMO_THEME_FITS_BOTH);
		check (info->counterpart == NULL);
	}

	info = find (themes, "ZzWidgetPair-dark");
	check (info != NULL);

	g_list_free_full (themes, (GDestroyNotify) nemo_theme_info_free);
}

/* What actually gets handed to GTK: the chosen theme, or its other half when
 * the mode has moved away from it. */
static void
test_theme_for_mode (void)
{
	char *resolved;

	set_mode ("dark");

	resolved = nemo_appearance_theme_for_mode (NEMO_THEME_KIND_ICON, "ZzTestPair");
	check (g_strcmp0 (resolved, "ZzTestPair-dark") == 0);
	g_free (resolved);

	/* Already the right half - left alone. */
	resolved = nemo_appearance_theme_for_mode (NEMO_THEME_KIND_ICON, "ZzTestPair-dark");
	check (g_strcmp0 (resolved, "ZzTestPair-dark") == 0);
	g_free (resolved);

	/* Serves both, so no swap. */
	resolved = nemo_appearance_theme_for_mode (NEMO_THEME_KIND_ICON, "ZzTestLone");
	check (g_strcmp0 (resolved, "ZzTestLone") == 0);
	g_free (resolved);

	resolved = nemo_appearance_theme_for_mode (NEMO_THEME_KIND_WIDGET, "ZzWidgetPair");
	check (g_strcmp0 (resolved, "ZzWidgetPair-dark") == 0);
	g_free (resolved);

	set_mode ("light");

	resolved = nemo_appearance_theme_for_mode (NEMO_THEME_KIND_ICON, "ZzTestPair-dark");
	check (g_strcmp0 (resolved, "ZzTestPair") == 0);
	g_free (resolved);

	/* A name nobody installed comes back unchanged rather than as NULL, so a
	 * stale setting never silently becomes "no theme at all". */
	resolved = nemo_appearance_theme_for_mode (NEMO_THEME_KIND_ICON, "ZzNotInstalled");
	check (g_strcmp0 (resolved, "ZzNotInstalled") == 0);
	g_free (resolved);

	check (nemo_appearance_theme_for_mode (NEMO_THEME_KIND_ICON, NULL) == NULL);
}

/* The bundled set is compiled into the binary rather than installed as files.
 * Three things have to hold or it silently stops working: the catalog the
 * picker reads has to be there, the widget sheets have to be reachable by the
 * path the loader builds, and every icon has to sit under a directory name
 * hicolor defines - GTK reads a resource path as part of hicolor and knows
 * nothing about a symbolic/ folder, so anything left in one is invisible.
 * A build with no bundled set (Linux) has nothing to check. */
#define BUNDLE_ICONS	"/org/nemo/themes/icontheme"
#define BUNDLE_WIDGETS	"/org/nemo/themes/widgettheme"
#define BUNDLE_CATALOG	"/org/nemo/themes/catalog"

static gboolean
resource_exists (const char *path)
{
	return g_resources_get_info (path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL, NULL, NULL);
}

/* Every file under @path, recursively, as full resource paths. */
static void
collect_resources (const char *path, GPtrArray *out)
{
	char **children = g_resources_enumerate_children (path, G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	int i;

	for (i = 0; children != NULL && children[i] != NULL; i++) {
		char *child = g_strconcat (path, "/", children[i], NULL);

		if (g_str_has_suffix (children[i], "/")) {
			child[strlen (child) - 1] = '\0';
			collect_resources (child, out);
			g_free (child);
		} else {
			g_ptr_array_add (out, child);
		}
	}

	g_strfreev (children);
}

static void
test_bundled_set (void)
{
	char      **names;
	GList      *themes, *node;
	GPtrArray  *files;
	gboolean    listed = FALSE;
	guint       i;
	int         symbolic_under_scalable = 0;

	names = g_resources_enumerate_children (BUNDLE_CATALOG "/icons",
						G_RESOURCE_LOOKUP_FLAGS_NONE, NULL);
	if (names == NULL || names[0] == NULL) {
		g_print ("no bundled theme set in this build - skipping\n");
		g_strfreev (names);
		return;
	}
	g_strfreev (names);

	/* The catalog reaches the picker, carrying what the picker needs. */
	themes = nemo_appearance_list_themes (NEMO_THEME_KIND_ICON, NEMO_THEME_FITS_BOTH);
	for (node = themes; node != NULL; node = node->next) {
		NemoThemeInfo *info = node->data;

		/* Adwaita is the sample deliberately: it is the tail of the fallback
		 * chain, so it is the one icon set that cannot be dropped from the
		 * bundle by a change of mind about which themes to ship. This check
		 * used to name Fluent, and went red the day that set was dropped. */
		if (g_strcmp0 (info->name, "Adwaita") == 0) {
			listed = TRUE;
			check (info->bundled);
			check (info->dir == NULL);		/* not a directory anywhere */
			check (info->style != NULL && info->style[0] != '\0');
		}

		/* The legacy shim is a fallback, never something to choose. */
		check (g_strcmp0 (info->name, "AdwaitaLegacy") != 0);
	}
	check (listed);
	g_list_free_full (themes, (GDestroyNotify) nemo_theme_info_free);

	/* A bundled widget theme's sheet, at exactly the path the loader builds. */
	check (resource_exists (BUNDLE_WIDGETS "/Fluent/gtk-3.0/gtk.css"));

	/* No icon may be left in a symbolic/ folder, and the monochrome ones
	 * still have to be there under their own name. */
	files = g_ptr_array_new_with_free_func (g_free);
	collect_resources (BUNDLE_ICONS "/Adwaita", files);
	check (files->len > 0);
	for (i = 0; i < files->len; i++) {
		const char *path = g_ptr_array_index (files, i);

		check (strstr (path, "/symbolic/") == NULL);
		if (strstr (path, "-symbolic.") != NULL && strstr (path, "/scalable/") != NULL) {
			symbolic_under_scalable++;
		}
	}
	check (symbolic_under_scalable > 0);
	g_ptr_array_unref (files);
}

static void
test_mode_resolution (void)
{
	set_mode ("light");
	check (nemo_appearance_is_dark () == FALSE);

	set_mode ("dark");
	check (nemo_appearance_is_dark () == TRUE);
}

int
main (int argc, char *argv[])
{
	char *tmp;

	tmp = g_dir_make_tmp ("nemo-appearance-test-XXXXXX", NULL);
	g_setenv ("XDG_CONFIG_HOME", tmp, TRUE);
	g_setenv ("APPDATA", tmp, TRUE);		/* the config root on Windows */
	g_setenv ("HOME", tmp, TRUE);

	if (!gtk_init_check (&argc, &argv)) {
		g_print ("SKIP: no display\n");
		g_free (tmp);
		return 0;
	}

	nemo_global_preferences_init ();

	/* The drop-in folders nemo_appearance_get_theme_roots () points at. */
	root = g_build_filename (tmp, "nemo-anywhere", NULL);

	make_icon_theme ("ZzTestPair", NULL);
	make_icon_theme ("ZzTestPair-dark", NULL);
	make_icon_theme ("ZzTestLone", NULL);
	make_icon_theme ("ZzTestDeclared-dark",
			 "X-Nemo-Modes=light;dark\nX-Nemo-Style=Test Style\n");
	make_icon_theme ("hicolor", NULL);

	make_widget_theme ("ZzWidgetPair", FALSE, NULL);
	make_widget_theme ("ZzWidgetPair-dark", FALSE, NULL);
	make_widget_theme ("ZzWidgetBoth", TRUE, NULL);

	test_mode_resolution ();
	test_inferred_pairing ();
	test_declared_modes ();
	test_widget_dark_sheet ();
	test_theme_for_mode ();
	test_bundled_set ();

	g_free (root);
	g_free (tmp);

	if (failures > 0) {
		g_printerr ("%d check(s) failed\n", failures);
		return 1;
	}

	g_print ("OK\n");
	return 0;
}
