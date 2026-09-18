/* Emptying the trash asks whenever the request did not come from the command in
 * a window. The bus's EmptyTrash used to go straight through once the trash
 * confirmation preference was off, with nothing in the log either. */

#include <config.h>

#include <stdlib.h>
#include <glib.h>
#include <gtk/gtk.h>

#include <libnemo-private/nemo-config.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-file-operations.h>

#include "test-scratch.h"

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

int
main (int argc, char *argv[])
{
	char *tmp;

	tmp = test_scratch_dir ("nemo-empty-trash-XXXXXX", NULL);
	g_setenv ("XDG_CONFIG_HOME", tmp, TRUE);
	g_setenv ("APPDATA", tmp, TRUE);
	g_setenv ("HOME", tmp, TRUE);

	nemo_global_preferences_init ();

	nemo_config_set_boolean (nemo_preferences, NEMO_PREFERENCES_CONFIRM_TRASH, TRUE);
	check (nemo_file_operations_empty_trash_asks (TRUE));
	check (nemo_file_operations_empty_trash_asks (FALSE));

	/* The preference is the person's to turn off, and it only covers them. */
	nemo_config_set_boolean (nemo_preferences, NEMO_PREFERENCES_CONFIRM_TRASH, FALSE);
	check (!nemo_file_operations_empty_trash_asks (TRUE));
	check (nemo_file_operations_empty_trash_asks (FALSE));

	g_free (tmp);

	if (failures > 0) {
		g_printerr ("%d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("OK\n");
	return EXIT_SUCCESS;
}
