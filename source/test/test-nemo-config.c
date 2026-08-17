/* Exercises the settings store: typed reads and writes, defaults, the
 * detailed changed signal, property binding, and the on-disk round trip.
 * Runs against a throwaway XDG_CONFIG_HOME. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

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

static char *
read_file (void)
{
	char *path = nemo_config_get_path ();
	char *text = NULL;

	g_file_get_contents (path, &text, NULL, NULL);
	g_free (path);

	return text ? text : g_strdup ("");
}

/* --- change signal ------------------------------------------------------- */

static int   changed_count;
static char *changed_key;

static void
on_changed (NemoConfigGroup *group, const char *key, gpointer data)
{
	changed_count++;
	g_free (changed_key);
	changed_key = g_strdup (key);
}

/* --- bind target --------------------------------------------------------- */

static void
test_defaults (NemoConfigGroup *prefs, NemoConfigGroup *list_view)
{
	char **cols;

	/* Straight from the table, with nothing on disk yet. */
	check (nemo_config_get_boolean (prefs, "show-hidden-files") == FALSE);
	check (nemo_config_get_boolean (prefs, "always-use-browser") == TRUE);
	check (nemo_config_get_enum (prefs, "click-policy") == NEMO_CLICK_POLICY_DOUBLE);

	cols = nemo_config_get_strv (list_view, "default-visible-columns");
	check (g_strv_length (cols) > 0);
	check (g_strcmp0 (cols[0], "name") == 0);
	g_strfreev (cols);
}

static void
test_scalars (NemoConfigGroup *prefs, NemoConfigGroup *window_state)
{
	char *s;

	nemo_config_set_boolean (prefs, "show-hidden-files", TRUE);
	check (nemo_config_get_boolean (prefs, "show-hidden-files") == TRUE);

	nemo_config_set_int (window_state, "sidebar-width", 321);
	check (nemo_config_get_int (window_state, "sidebar-width") == 321);

	nemo_config_set_string (window_state, "geometry", "800x600+10+20");
	s = nemo_config_get_string (window_state, "geometry");
	check (g_strcmp0 (s, "800x600+10+20") == 0);
	g_free (s);

	/* Enums round-trip through their nick, not their number. */
	nemo_config_set_enum (prefs, "click-policy", NEMO_CLICK_POLICY_SINGLE);
	check (nemo_config_get_enum (prefs, "click-policy") == NEMO_CLICK_POLICY_SINGLE);
}

static void
test_strv (NemoConfigGroup *list_view)
{
	const char *set[] = { "name", "size", NULL };
	const char *none[] = { NULL };
	char      **got;

	nemo_config_set_strv (list_view, "default-visible-columns", set);
	got = nemo_config_get_strv (list_view, "default-visible-columns");
	check (g_strv_length (got) == 2);
	check (g_strcmp0 (got[1], "size") == 0);
	g_strfreev (got);

	/* An emptied list must stay empty, not spring back to the default. */
	nemo_config_set_strv (list_view, "default-visible-columns", none);
	got = nemo_config_get_strv (list_view, "default-visible-columns");
	check (g_strv_length (got) == 0);
	g_strfreev (got);
}

/* A value equal to the default is dropped, so a later change to that
 * default still reaches the user (and the file stays small). */
static void
test_default_not_stored (NemoConfigGroup *prefs)
{
	char *text;

	nemo_config_set_boolean (prefs, "always-use-browser", TRUE);  /* == default */
	nemo_config_flush ();

	text = read_file ();
	check (strstr (text, "always-use-browser") == NULL);
	g_free (text);

	nemo_config_set_boolean (prefs, "always-use-browser", FALSE); /* != default */
	nemo_config_flush ();

	text = read_file ();
	check (strstr (text, "always-use-browser") != NULL);
	g_free (text);

	/* reset drops it again */
	nemo_config_reset (prefs, "always-use-browser");
	check (nemo_config_get_boolean (prefs, "always-use-browser") == TRUE);
}

static void
test_changed_signal (NemoConfigGroup *prefs)
{
	gulong id;

	changed_count = 0;
	id = g_signal_connect (prefs, "changed::show-hidden-files",
	                       G_CALLBACK (on_changed), NULL);

	nemo_config_set_boolean (prefs, "show-hidden-files", FALSE);
	check (changed_count == 1);
	check (g_strcmp0 (changed_key, "show-hidden-files") == 0);

	/* A different key must not reach a detailed handler. */
	nemo_config_set_boolean (prefs, "sort-directories-first", FALSE);
	check (changed_count == 1);

	g_signal_handler_disconnect (prefs, id);
	g_clear_pointer (&changed_key, g_free);
}

static void
test_bind (NemoConfigGroup *prefs)
{
	GtkWidget *toggle = gtk_check_button_new ();

	g_object_ref_sink (toggle);

	nemo_config_set_boolean (prefs, "show-hidden-files", TRUE);
	nemo_config_bind (prefs, "show-hidden-files", toggle, "active",
	                  NEMO_CONFIG_BIND_DEFAULT);

	/* config -> widget, applied at bind time */
	check (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)) == TRUE);

	/* config -> widget, on change */
	nemo_config_set_boolean (prefs, "show-hidden-files", FALSE);
	check (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (toggle)) == FALSE);

	/* widget -> config */
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
	check (nemo_config_get_boolean (prefs, "show-hidden-files") == TRUE);

	/* the binding must not outlive the widget */
	g_object_unref (toggle);
	nemo_config_set_boolean (prefs, "show-hidden-files", FALSE);
}

/* The preferences dialog binds every combo and radio through a mapping that
 * fills in the nick and leaves the number alone. Taking the number wrote the
 * zero-valued nick whatever was picked - for the executable-text setting that
 * meant "run it" no matter what the dialog showed. */
static gboolean
viewer_get_mapping (GValue *value, const NemoConfigValue *config_value, gpointer data)
{
	g_value_set_boolean (value, g_strcmp0 (config_value->s, "compact-view") == 0);
	return TRUE;
}

static gboolean
viewer_set_mapping (const GValue *value, NemoConfigValue *config_value, gpointer data)
{
	config_value->s = g_strdup (g_value_get_boolean (value) ? "compact-view"
	                                                        : "list-view");
	return TRUE;
}

static void
test_enum_bind_by_nick (NemoConfigGroup *prefs)
{
	GtkWidget *toggle = gtk_check_button_new ();
	char      *text;

	g_object_ref_sink (toggle);
	nemo_config_bind_with_mapping (prefs, "default-folder-viewer",
	                               toggle, "active", NEMO_CONFIG_BIND_DEFAULT,
	                               viewer_get_mapping, viewer_set_mapping,
	                               NULL, NULL);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (toggle), TRUE);
	nemo_config_flush ();

	text = read_file ();
	check (strstr (text, "default-folder-viewer: compact-view") != NULL);
	/* icon-view is the zero-valued nick, i.e. what the bug stored */
	check (strstr (text, "icon-view") == NULL);
	g_free (text);

	g_object_unref (toggle);
	nemo_config_reset (prefs, "default-folder-viewer");
}

/* Setting a comment appends a line rather than replacing one, so re-applying
 * it on every write grew the same comment without bound. */
static void
test_comment_written_once (NemoConfigGroup *window_state)
{
	const char *summary = "# Width of the side pane";
	char       *text, *at;
	int         seen = 0;

	nemo_config_set_int (window_state, "sidebar-width", 201);
	nemo_config_set_int (window_state, "sidebar-width", 202);
	nemo_config_set_int (window_state, "sidebar-width", 203);
	nemo_config_flush ();

	text = read_file ();
	for (at = text; (at = strstr (at, summary)) != NULL; at++)
		seen++;
	check (seen == 1);
	g_free (text);
}

static void
test_persistence (void)
{
	char *text;

	nemo_config_set_boolean (nemo_config_get_group ("preferences"),
	                         "show-hidden-files", TRUE);
	nemo_config_flush ();

	text = read_file ();
	/* Written as SHCL, grouped, with the summary carried across as a comment. */
	check (strstr (text, "preferences:") != NULL);
	check (strstr (text, "show-hidden-files: true") != NULL);
	check (strstr (text, "# Whether to show hidden files") != NULL);
	g_free (text);
}

/* An external edit is picked up and reported as a per-key change. */
static void
test_external_edit (NemoConfigGroup *prefs)
{
	char  *path = nemo_config_get_path ();
	int    spins = 0;

	nemo_config_set_boolean (prefs, "show-hidden-files", FALSE);
	nemo_config_flush ();

	changed_count = 0;
	g_signal_connect (prefs, "changed::show-hidden-files",
	                  G_CALLBACK (on_changed), NULL);

	g_file_set_contents (path,
	                     "preferences:\n\tshow-hidden-files: true\n", -1, NULL);

	/* the monitor is async; give it a bounded chance to fire */
	while (changed_count == 0 && spins++ < 200) {
		g_main_context_iteration (NULL, FALSE);
		g_usleep (10000);
	}

	check (changed_count >= 1);
	check (nemo_config_get_boolean (prefs, "show-hidden-files") == TRUE);

	/* A key that was already in the file, edited to a different value. The
	 * diff has to compare the values, not just notice a key appear or go. */
	changed_count = 0;
	spins = 0;
	g_file_set_contents (path,
	                     "preferences:\n\tshow-hidden-files: false\n", -1, NULL);

	while (changed_count == 0 && spins++ < 200) {
		g_main_context_iteration (NULL, FALSE);
		g_usleep (10000);
	}

	check (changed_count >= 1);
	check (nemo_config_get_boolean (prefs, "show-hidden-files") == FALSE);

	g_free (path);
}

/* A file that exists but cannot be read (share lock, or the delete half of a
 * non-atomic external save) must not swap defaults into memory - a queued
 * save would then wipe the real file. Simulated by putting a directory at
 * the config path, which fails the read on any platform and any uid. */
static void
test_unreadable_file_kept (NemoConfigGroup *prefs)
{
	char *path = nemo_config_get_path ();
	int   spins = 0;

	nemo_config_set_boolean (prefs, "show-hidden-files", TRUE);
	nemo_config_flush ();

	g_remove (path);
	g_mkdir (path, 0700);

	/* let the monitor's DELETED/CREATED events land */
	while (spins++ < 100) {
		g_main_context_iteration (NULL, FALSE);
		g_usleep (10000);
	}

	check (nemo_config_get_boolean (prefs, "show-hidden-files") == TRUE);

	g_rmdir (path);
	nemo_config_set_boolean (prefs, "show-hidden-files", FALSE);
	nemo_config_flush ();
	g_free (path);
}

/* strstr and even g_strstr_len stop at an embedded NUL, which is the very
 * byte under test - so search the raw buffer by hand. */
static gboolean
buf_contains (const char *buf, gsize len, const char *needle)
{
	gsize nlen = strlen (needle);
	gsize i;

	for (i = 0; nlen > 0 && i + nlen <= len; i++)
		if (memcmp (buf + i, needle, nlen) == 0)
			return TRUE;
	return FALSE;
}

/* SHCL is NUL-transparent, so a NUL that came in from the file must survive
 * the next save instead of truncating everything after it. */
static void
test_nul_survives_save (NemoConfigGroup *window_state)
{
	char        *path = nemo_config_get_path ();
	const char   before[] = "window-state:\n\tgeometry: \"a\0b\"\n\tsidebar-width: 444\n";
	char        *text = NULL;
	gsize        len = 0;
	int          spins = 0;

	changed_count = 0;
	g_signal_connect (window_state, "changed::sidebar-width",
	                  G_CALLBACK (on_changed), NULL);

	g_file_set_contents (path, before, sizeof (before) - 1, NULL);
	while (changed_count == 0 && spins++ < 200) {
		g_main_context_iteration (NULL, FALSE);
		g_usleep (10000);
	}
	check (nemo_config_get_int (window_state, "sidebar-width") == 444);

	nemo_config_set_int (window_state, "sidebar-width", 445);
	nemo_config_flush ();

	g_file_get_contents (path, &text, &len, NULL);
	check (text != NULL && memchr (text, '\0', len) != NULL);
	check (text != NULL && buf_contains (text, len, "sidebar-width"));
	g_free (text);
	g_free (path);
}

int
main (int argc, char *argv[])
{
	char            *tmp;
	NemoConfigGroup *prefs, *list_view, *window_state;

	tmp = g_dir_make_tmp ("nemo-config-test-XXXXXX", NULL);
	g_setenv ("XDG_CONFIG_HOME", tmp, TRUE);
	g_setenv ("HOME", tmp, TRUE);

	gtk_init (&argc, &argv);

	nemo_config_init ();

	prefs        = nemo_config_get_group ("preferences");
	list_view    = nemo_config_get_group ("list-view");
	window_state = nemo_config_get_group ("window-state");

	test_defaults (prefs, list_view);
	test_scalars (prefs, window_state);
	test_strv (list_view);
	test_default_not_stored (prefs);
	test_changed_signal (prefs);
	test_bind (prefs);
	test_enum_bind_by_nick (prefs);
	test_comment_written_once (window_state);
	test_persistence ();
	test_external_edit (prefs);
	test_unreadable_file_kept (prefs);
	test_nul_survives_save (window_state);

	nemo_config_shutdown ();
	g_free (tmp);

	if (failures == 0)
		g_print ("nemo-config: all checks passed\n");

	return failures == 0 ? 0 : 1;
}
