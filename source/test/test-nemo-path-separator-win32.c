/* Windows takes either separator, so which one is shown is a setting and a
 * typed forward slash can be refused. Checks both, plus the live reaction to a
 * setting change. Runs against a throwaway config root. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <gtk/gtk.h>

#include <libnemo-private/nemo-config.h>
#include <libnemo-private/nemo-file-utilities.h>
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
check_applied (const char *before, const char *want)
{
	char *got = g_strdup (before);

	nemo_path_apply_display_separator (got);

	if (g_strcmp0 (got, want) != 0) {
		g_printerr ("FAIL %s -> %s, wanted %s\n", before, got, want);
		failures++;
	}

	g_free (got);
}

static void
test_backslash_is_the_default (void)
{
	check (nemo_path_get_display_separator () == '\\');

	check_applied ("C:/Users/someone", "C:\\Users\\someone");
	check_applied ("C:\\Users\\someone", "C:\\Users\\someone");

	/* A name with no separator at all is left alone. */
	check_applied ("readme.txt", "readme.txt");
}

static void
test_slash_can_be_chosen (void)
{
	nemo_config_set_string (nemo_preferences, NEMO_PREFERENCES_PATH_SEPARATOR, "slash");

	check (nemo_path_get_display_separator () == '/');
	check_applied ("C:\\Users\\someone", "C:/Users/someone");

	/* A UNC path keeps its shape - both leading separators convert together. */
	check_applied ("\\\\server\\share\\file", "//server/share/file");

	nemo_config_set_string (nemo_preferences, NEMO_PREFERENCES_PATH_SEPARATOR, "backslash");
	check (nemo_path_get_display_separator () == '\\');
}

static void
test_slash_input_can_be_refused (void)
{
	/* Allowed by default. */
	check (nemo_path_input_is_allowed ("C:/Users/someone"));

	nemo_config_set_boolean (nemo_preferences, NEMO_PREFERENCES_ALLOW_SLASH_INPUT, FALSE);

	check (!nemo_path_input_is_allowed ("C:/Users/someone"));

	/* A backslash is always a separator, so this one still goes through. */
	check (nemo_path_input_is_allowed ("C:\\Users\\someone"));

	/* A uri is all slashes by definition and has nothing to do with the setting. */
	check (nemo_path_input_is_allowed ("smb://server/share"));
	check (nemo_path_input_is_allowed ("network:///"));

	nemo_config_set_boolean (nemo_preferences, NEMO_PREFERENCES_ALLOW_SLASH_INPUT, TRUE);
	check (nemo_path_input_is_allowed ("C:/Users/someone"));
}

int
main (int argc, char *argv[])
{
	char *tmp;

	tmp = g_dir_make_tmp ("nemo-separator-test-XXXXXX", NULL);
	g_setenv ("XDG_CONFIG_HOME", tmp, TRUE);
	g_setenv ("APPDATA", tmp, TRUE);
	g_setenv ("HOME", tmp, TRUE);

	gtk_init (&argc, &argv);

	nemo_global_preferences_init ();

	test_backslash_is_the_default ();
	test_slash_can_be_chosen ();
	test_slash_input_can_be_refused ();

	g_free (tmp);

	if (failures > 0) {
		g_printerr ("%d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("path separator: all checks passed\n");
	return EXIT_SUCCESS;
}
