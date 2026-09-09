/* Exercises the crash reporter: a child process is made to die both ways it
 * can, and the report it leaves behind is read back. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>

#include <libnemo-private/nemo-crash.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

static volatile int *null_pointer = NULL;

/* The child half. Prints where the report will go, then dies on purpose. */
static int
run_child (const char *how)
{
	const char *path;

	nemo_crash_handler_install ();

	path = nemo_crash_report_path ();
	g_print ("%s\n", path != NULL ? path : "none");
	fflush (stdout);

	if (strcmp (how, "off") == 0) {
		return 0;
	}

	if (strcmp (how, "abort") == 0) {
		abort ();
	}

	*null_pointer = 1;

	return 0;
}

typedef struct {
	char *report_path;
	char *stderr_text;
	int status;
	gboolean spawned;
} ChildResult;

static void
child_result_clear (ChildResult *result)
{
	g_free (result->report_path);
	g_free (result->stderr_text);
}

static ChildResult
run_crashing_child (const char *self, const char *config_root, const char *how,
		    gboolean handler_on)
{
	ChildResult result = { NULL, NULL, 0, FALSE };
	g_autoptr (GError) error = NULL;
	g_auto (GStrv) envp = NULL;
	g_autofree char *output = NULL;
	char *argv[3];
	char *newline;

	envp = g_get_environ ();
	envp = g_environ_setenv (envp, "XDG_CONFIG_HOME", config_root, TRUE);
	envp = g_environ_setenv (envp, "APPDATA", config_root, TRUE);
	envp = g_environ_setenv (envp, "HOME", config_root, TRUE);
	envp = g_environ_setenv (envp, "NEMO_NO_CRASH_DIALOG", "1", TRUE);

	if (handler_on) {
		envp = g_environ_unsetenv (envp, "NEMO_NO_CRASH_HANDLER");
	} else {
		envp = g_environ_setenv (envp, "NEMO_NO_CRASH_HANDLER", "1", TRUE);
	}

	argv[0] = (char *) self;
	argv[1] = (char *) how;
	argv[2] = NULL;

	result.spawned = g_spawn_sync (NULL, argv, envp, G_SPAWN_DEFAULT, NULL, NULL,
				       &output, &result.stderr_text, &result.status,
				       &error);

	if (!result.spawned) {
		g_printerr ("could not start the child: %s\n", error->message);
		return result;
	}

	result.report_path = g_strdup (output != NULL ? output : "");
	newline = strchr (result.report_path, '\n');
	if (newline != NULL) {
		*newline = '\0';
	}

	return result;
}

static void
check_report (const char *path, const char *expect_cause)
{
	g_autofree char *text = NULL;
	g_auto (GStrv) lines = NULL;
	gboolean saw_frame = FALSE;
	int i;

	if (!g_file_get_contents (path, &text, NULL, NULL)) {
		g_printerr ("FAIL no report at %s\n", path);
		failures++;
		return;
	}

	check (strstr (text, "nemo-anywhere v") != NULL);
	check (strstr (text, "died on ") != NULL);
	check (strstr (text, expect_cause) != NULL);
	check (strstr (text, "\npid ") != NULL);

	lines = g_strsplit (text, "\n", -1);

	/* Anything past the header carrying an address is a frame. Which frames
	   show up depends on the build, so only their presence is checked. */
	for (i = 4; lines[i] != NULL; i++) {
		if (strstr (lines[i], "0x") != NULL) {
			saw_frame = TRUE;
		}
	}

	check (saw_frame);
}

int
main (int argc, char *argv[])
{
	g_autofree char *config_root = NULL;
	g_autoptr (GError) error = NULL;
	ChildResult result;

	if (argc == 2) {
		return run_child (argv[1]);
	}

	config_root = g_dir_make_tmp ("nemo-crash-XXXXXX", &error);
	if (config_root == NULL) {
		g_printerr ("FAIL could not make a config root: %s\n", error->message);
		return 1;
	}

	/* A real fault: the unhandled-exception filter on Windows, SIGSEGV
	   everywhere else. */
	result = run_crashing_child (argv[0], config_root, "fault", TRUE);
	if (result.spawned) {
		check (result.status != 0);
		check (result.report_path[0] != '\0');
		check (strcmp (result.report_path, "none") != 0);

		if (result.report_path[0] != '\0') {
			check_report (result.report_path,
#ifdef G_OS_WIN32
				      "access violation");
#else
				      "SIGSEGV");
#endif
		}

		/* The same report goes to stderr, which is where a launcher log
		   picks it up. */
		check (strstr (result.stderr_text, "nemo-anywhere v") != NULL);
		check (strstr (result.stderr_text, result.report_path) != NULL);
	}
	child_result_clear (&result);

	/* An assertion failure, which never reaches a fault handler on Windows. */
	result = run_crashing_child (argv[0], config_root, "abort", TRUE);
	if (result.spawned) {
		check (result.status != 0);

		if (result.report_path[0] != '\0') {
			check_report (result.report_path,
#ifdef G_OS_WIN32
				      "aborted");
#else
				      "SIGABRT");
#endif
		}
	}
	child_result_clear (&result);

	/* Switched off, nothing is installed and there is nowhere to write. */
	result = run_crashing_child (argv[0], config_root, "off", FALSE);
	if (result.spawned) {
		check (result.status == 0);
		check (strcmp (result.report_path, "none") == 0);
	}
	child_result_clear (&result);

	if (failures == 0) {
		g_print ("crash reporter: all checks passed\n");
	}

	return failures == 0 ? 0 : 1;
}
