/* Which of the three ways in wins. The define is checked by being absent: with
 * it on there is nothing to decide, so the whole thing skips.
 *
 * Each case runs as a child of this binary, since the environment variable is
 * read once per process and a second read in the same one would see the first
 * answer. The child exits 0 armed, 1 quiet, so the parent just reads a status.
 *
 * Runs against a throwaway config root. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <libnemo-private/nemo-config.h>
#include <libnemo-private/nemo-delete-testguard.h>
#include <libnemo-private/nemo-global-preferences.h>

#include "test-scratch.h"

#define CHILD_ARMED  0
#define CHILD_QUIET  1
#define CHILD_BROKEN 2

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

static void
point_config_at (const char *dir)
{
	g_setenv ("XDG_CONFIG_HOME", dir, TRUE);
	g_setenv ("APPDATA", dir, TRUE);
	g_setenv ("HOME", dir, TRUE);
}

/* "store" writes the setting and stops; "read" reports what arming decided. */
static int
run_as_child (const char *mode)
{
	NemoConfigGroup *debug;

	nemo_config_init ();

	if (strcmp (mode, "read") == 0) {
		int armed = nemo_delete_testguard_armed () ? CHILD_ARMED : CHILD_QUIET;

		nemo_config_shutdown ();
		return armed;
	}

	debug = nemo_config_get_group (NEMO_DEBUG_GROUP);
	if (debug == NULL) {
		return CHILD_BROKEN;
	}

	nemo_config_set_boolean (debug, NEMO_PREFERENCES_TESTGUARD_ALL_DELETES,
				 strcmp (mode, "store-on") == 0);
	nemo_config_flush ();
	nemo_config_shutdown ();

	return CHILD_ARMED;
}

/* `env_value` NULL leaves the variable unset. */
static int
spawn_child (const char *self, const char *mode, const char *env_value)
{
	char     *argv[] = { (char *) self, (char *) mode, NULL };
	GError   *error = NULL;
	int       status = -1;

	if (env_value != NULL) {
		g_setenv (NEMO_TESTGUARD_ENV_VAR, env_value, TRUE);
	} else {
		g_unsetenv (NEMO_TESTGUARD_ENV_VAR);
	}

	if (!g_spawn_sync (NULL, argv, NULL, G_SPAWN_DEFAULT,
			   NULL, NULL, NULL, NULL, &status, &error)) {
		g_printerr ("could not run %s %s: %s\n", self, mode, error->message);
		g_error_free (error);
		return -1;
	}

	/* A non-zero exit is the answer here, not a failure, and this is the
	   portable way back to the number the child returned. */
	if (!g_spawn_check_exit_status (status, &error)) {
		int code = error->domain == G_SPAWN_EXIT_ERROR ? error->code : -1;

		g_error_free (error);
		return code;
	}

	return CHILD_ARMED;
}

int
main (int argc, char *argv[])
{
	char *store_on, *store_off;

	if (NEMO_TESTGUARD_ALL_DELETES) {
		g_print ("SKIP: armed at compile time, nothing left to decide\n");
		return 77;
	}

	if (argc > 1) {
		return run_as_child (argv[1]);
	}

	store_on  = test_scratch_dir ("nemo-testguard-on-XXXXXX", NULL);
	store_off = test_scratch_dir ("nemo-testguard-off-XXXXXX", NULL);

	/* Nothing set anywhere. */
	point_config_at (store_off);
	check (spawn_child (argv[0], "read", NULL) == CHILD_QUIET);

	/* The setting on its own. */
	point_config_at (store_on);
	check (spawn_child (argv[0], "store-on", NULL) == CHILD_ARMED);
	check (spawn_child (argv[0], "read", NULL) == CHILD_ARMED);

	/* The variable beats the setting in both directions. */
	check (spawn_child (argv[0], "read", "0") == CHILD_QUIET);
	check (spawn_child (argv[0], "read", "off") == CHILD_QUIET);

	point_config_at (store_off);
	check (spawn_child (argv[0], "store-off", NULL) == CHILD_ARMED);
	check (spawn_child (argv[0], "read", "1") == CHILD_ARMED);
	check (spawn_child (argv[0], "read", "YES") == CHILD_ARMED);

	/* A value that is neither is ignored, so the setting still answers. */
	check (spawn_child (argv[0], "read", "maybe") == CHILD_QUIET);
	point_config_at (store_on);
	check (spawn_child (argv[0], "read", "maybe") == CHILD_ARMED);

	g_free (store_on);
	g_free (store_off);

	if (failures == 0)
		g_print ("nemo-delete-testguard: arming order holds\n");

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
