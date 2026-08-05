/* Exercises the settings store: typed reads and writes, defaults, the
 * detailed changed signal, property binding, and the on-disk round trip.
 * Runs against a throwaway XDG_CONFIG_HOME. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
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
	test_persistence ();
	test_external_edit (prefs);

	nemo_config_shutdown ();
	g_free (tmp);

	if (failures == 0)
		g_print ("nemo-config: all checks passed\n");

	return failures == 0 ? 0 : 1;
}
