/* Windows keeps hidden files behind an attribute and treats a leading dot as an
 * ordinary character, so dot-files get a switch of their own there. This checks
 * the one decision point every filter goes through. Runs against a throwaway
 * config root. */

#include <config.h>

#include <stdlib.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <libnemo-private/nemo-config.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-global-preferences.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

static void
test_hidden_by_default (void)
{
	check (nemo_file_name_is_hidden_dot_file (".bashrc"));
	check (nemo_file_name_is_hidden_dot_file (".config"));

	check (!nemo_file_name_is_hidden_dot_file ("readme.txt"));
	check (!nemo_file_name_is_hidden_dot_file ("archive.tar.gz"));
	check (!nemo_file_name_is_hidden_dot_file (NULL));

	/* A name that only contains a dot elsewhere is an ordinary file. */
	check (!nemo_file_name_is_hidden_dot_file ("v1.2.3"));
}

static void
test_switch_reveals_them (void)
{
	nemo_config_set_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_DOT_FILES, TRUE);

	check (!nemo_file_name_is_hidden_dot_file (".bashrc"));
	check (!nemo_file_name_is_hidden_dot_file ("readme.txt"));

	nemo_config_set_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_DOT_FILES, FALSE);

	check (nemo_file_name_is_hidden_dot_file (".bashrc"));
}

/* The two switches are independent: revealing attribute-hidden files must not
 * also reveal dot-files. */
static void
test_independent_of_show_hidden (void)
{
	nemo_config_set_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_HIDDEN_FILES, TRUE);

	check (nemo_file_name_is_hidden_dot_file (".bashrc"));

	nemo_config_set_boolean (nemo_preferences, NEMO_PREFERENCES_SHOW_HIDDEN_FILES, FALSE);
}

int
main (int argc, char *argv[])
{
	char *tmp;

	tmp = g_dir_make_tmp ("nemo-dot-files-test-XXXXXX", NULL);
	g_setenv ("XDG_CONFIG_HOME", tmp, TRUE);
	g_setenv ("APPDATA", tmp, TRUE);
	g_setenv ("HOME", tmp, TRUE);

	gtk_init (&argc, &argv);

	nemo_global_preferences_init ();

	test_hidden_by_default ();
	test_switch_reveals_them ();
	test_independent_of_show_hidden ();

	g_free (tmp);

	if (failures > 0) {
		g_printerr ("%d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("dot-file switch: all checks passed\n");
	return EXIT_SUCCESS;
}
