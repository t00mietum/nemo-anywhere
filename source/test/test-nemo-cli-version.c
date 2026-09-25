/* --version and --about print and leave, so they must work with no display at
 * all. They used to fail with "Cannot open display" before the flag was read.
 * Takes the path to nemo-anywhere as its one argument. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include "test-check.h"

static void
check_prints (const char *program, const char *flag)
{
	const char *argv[] = { program, flag, NULL };
	char **envp;
	char *out = NULL, *err = NULL;
	int status = -1;
	GError *error = NULL;

	envp = g_get_environ ();
	envp = g_environ_unsetenv (envp, "DISPLAY");
	envp = g_environ_unsetenv (envp, "WAYLAND_DISPLAY");

	check (g_spawn_sync (NULL, (char **) argv, envp, G_SPAWN_DEFAULT, NULL, NULL,
			     &out, &err, &status, &error));
	if (error != NULL) {
		g_printerr ("%s: %s\n", flag, error->message);
		g_clear_error (&error);
	} else {
		check (g_spawn_check_wait_status (status, NULL));
		check (strstr (out, "nemo-anywhere ") != NULL);
		check (strstr (out, "Copyright ") != NULL);
		if (err != NULL && *err != '\0') {
			g_printerr ("%s wrote to stderr: %s", flag, err);
		}
		check (err == NULL || strstr (err, "display") == NULL);
	}

	g_free (out);
	g_free (err);
	g_strfreev (envp);
}

int
main (int argc, char *argv[])
{
	if (argc < 2) {
		g_printerr ("usage: %s <path to nemo-anywhere>\n", argv[0]);
		return 77;
	}

	check_prints (argv[1], "--version");
	check_prints (argv[1], "--about");

	if (failures > 0) {
		g_printerr ("%d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("OK\n");
	return EXIT_SUCCESS;
}
