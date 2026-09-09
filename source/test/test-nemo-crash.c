/* Exercises the crash reporter: a child process is made to die both ways it
 * can, and the report it leaves behind is read back. */

#include <config.h>

#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <glib/gstdio.h>
#ifndef G_OS_WIN32
#include <sys/resource.h>
#include <sys/wait.h>
#endif

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
static void (*volatile null_function) (void) = NULL;

/* The child half. Prints where the report will go, then dies on purpose. */
static int
run_child (const char *how)
{
	const char *path;

#ifndef G_OS_WIN32
	/* Three deliberate crashes per run would otherwise drop three cores in
	   the build directory. */
	struct rlimit no_core = { 0, 0 };

	setrlimit (RLIMIT_CORE, &no_core);
#endif

	nemo_crash_handler_install ();

	path = nemo_crash_report_path ();
	g_print ("%s\n", path != NULL ? path : "none");
	fflush (stdout);

	if (strcmp (how, "abort") == 0) {
		abort ();
	}

	/* A call through a pointer that is no longer there: the shape a freed
	   object takes, and the one an unwinder is most likely to give up on. */
	if (strcmp (how, "nullcall") == 0) {
		null_function ();
	}

	*null_pointer = 1;

	return 0;
}

/* The point of the handler is that it hands the signal back, so the process
   still dies the way it would have without one. Reporting and then exiting
   normally would pass a plain "did not exit zero" check. */
static void
check_died_of (int status, int expect_signal, unsigned long expect_code)
{
#ifdef G_OS_WIN32
	(void) expect_signal;
	check ((unsigned long) status == expect_code);
#else
	(void) expect_code;
	check (WIFSIGNALED (status));
	if (WIFSIGNALED (status)) {
		check (WTERMSIG (status) == expect_signal);
	}
#endif
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
	for (i = 5; lines[i] != NULL; i++) {
		if (strstr (lines[i], "0x") != NULL) {
			saw_frame = TRUE;
		}
	}

	/* A build with no unwinder says so instead, and that is the whole
	   contract there. */
	check (saw_frame || strstr (text, "not available in this build") != NULL);
}

static guint
count_reports (const char *dir)
{
	g_autoptr (GDir) handle = g_dir_open (dir, 0, NULL);
	const char *entry;
	guint n = 0;

	if (handle == NULL) {
		return 0;
	}

	while ((entry = g_dir_read_name (handle)) != NULL) {
		if (g_str_has_prefix (entry, "crash-")) {
			n++;
		}
	}

	return n;
}

static void
remove_tree (const char *path)
{
	g_autoptr (GDir) handle = g_dir_open (path, 0, NULL);
	const char *entry;

	if (handle != NULL) {
		while ((entry = g_dir_read_name (handle)) != NULL) {
			g_autofree char *child = g_build_filename (path, entry, NULL);

			if (g_file_test (child, G_FILE_TEST_IS_DIR) &&
			    !g_file_test (child, G_FILE_TEST_IS_SYMLINK)) {
				remove_tree (child);
			} else {
				g_unlink (child);
			}
		}
	}

	g_rmdir (path);
}

int
main (int argc, char *argv[])
{
	g_autofree char *config_root = NULL;
	g_autofree char *crash_dir = NULL;
	g_autoptr (GError) error = NULL;
	ChildResult result;
	guint before = 0;

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
		check_died_of (result.status, SIGSEGV, 0xc0000005UL);
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

	/* Learned from the child rather than rebuilt here, so the test cannot
	   disagree with the code about where reports go. */
	if (result.report_path != NULL && result.report_path[0] != '\0') {
		crash_dir = g_path_get_dirname (result.report_path);
	}

	child_result_clear (&result);

	/* An assertion failure, which never reaches a fault handler on Windows. */
	result = run_crashing_child (argv[0], config_root, "abort", TRUE);
	if (result.spawned) {
		check_died_of (result.status, SIGABRT, 0x40000015UL);
		check (result.report_path[0] != '\0');
		check (strcmp (result.report_path, "none") != 0);

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

	/* A jump to nowhere. The stack has to be recovered from the return
	   address, since there is no function at the address that faulted. */
	result = run_crashing_child (argv[0], config_root, "nullcall", TRUE);
	if (result.spawned) {
		check_died_of (result.status, SIGSEGV, 0xc0000005UL);
		check (result.report_path[0] != '\0');

		if (result.report_path[0] != '\0') {
			check_report (result.report_path,
#ifdef G_OS_WIN32
				      "access violation");
#else
				      "SIGSEGV");
#endif
		}
	}
	child_result_clear (&result);

	/* Switched off, a real crash leaves nothing behind. */
	before = crash_dir != NULL ? count_reports (crash_dir) : 0;
	check (before > 0);

	result = run_crashing_child (argv[0], config_root, "fault", FALSE);
	if (result.spawned) {
		check (result.status != 0);
		check (strcmp (result.report_path, "none") == 0);
		check (crash_dir != NULL && count_reports (crash_dir) == before);
	}
	child_result_clear (&result);

	remove_tree (config_root);

	if (failures == 0) {
		g_print ("crash reporter: all checks passed\n");
	}

	return failures == 0 ? 0 : 1;
}
