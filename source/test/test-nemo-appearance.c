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

	g_free (root);
	g_free (tmp);

	if (failures > 0) {
		g_printerr ("%d check(s) failed\n", failures);
		return 1;
	}

	g_print ("OK\n");
	return 0;
}
