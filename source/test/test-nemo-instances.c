/* Two launches are two processes that both stay up, and --quit takes every one
 * of them down. Needs the built program (argv[1]) and a display; without a
 * display it skips. The --quit half needs a session bus and skips without one. */

#include <config.h>

#include <gio/gio.h>
#include <glib/gstdio.h>

#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

static GPid
launch (const char *exe, const char *arg)
{
	char *argv[] = { (char *) exe, (char *) arg, NULL };
	GPid pid = 0;
	GError *error = NULL;

	if (!g_spawn_async (NULL, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD,
	                    NULL, NULL, &pid, &error)) {
		g_printerr ("spawn: %s\n", error->message);
		g_error_free (error);
	}

	return pid;
}

static gboolean
alive (GPid pid)
{
	int status;

	return pid > 0 && waitpid (pid, &status, WNOHANG) == 0;
}

/* TRUE when the process went away inside the time. */
static gboolean
wait_gone (GPid pid, int seconds)
{
	int i;

	for (i = 0; i < seconds * 10; i++) {
		if (!alive (pid)) {
			return TRUE;
		}
		g_usleep (100 * 1000);
	}

	return FALSE;
}

int
main (int argc, char *argv[])
{
	const char *exe;
	char *tmp;
	GPid first, second, quitter;
	GDBusConnection *bus;

	if (argc < 2) {
		g_printerr ("usage: %s <nemo-anywhere>\n", argv[0]);
		return 1;
	}
	exe = argv[1];

	if (g_getenv ("DISPLAY") == NULL && g_getenv ("WAYLAND_DISPLAY") == NULL) {
		g_print ("SKIP: no display\n");
		return 77;
	}

	tmp = g_dir_make_tmp ("nemo-instances-test-XXXXXX", NULL);
	g_setenv ("XDG_CONFIG_HOME", tmp, TRUE);
	g_setenv ("APPDATA", tmp, TRUE);
	g_setenv ("HOME", tmp, TRUE);

	first = launch (exe, tmp);
	second = launch (exe, tmp);
	check (first > 0 && second > 0);

	/* A second copy that handed its arguments over would be gone well inside
	 * this; both are meant to stay. */
	g_usleep (4 * G_USEC_PER_SEC);
	check (alive (first));
	check (alive (second));

	bus = g_bus_get_sync (G_BUS_TYPE_SESSION, NULL, NULL);
	if (bus != NULL) {
		quitter = launch (exe, "--quit");
		check (wait_gone (quitter, 20));
		check (wait_gone (first, 20));
		check (wait_gone (second, 20));
		g_object_unref (bus);
	} else {
		g_print ("no session bus: --quit not checked\n");
	}

	if (alive (first)) {
		kill (first, SIGTERM);
		wait_gone (first, 5);
	}
	if (alive (second)) {
		kill (second, SIGTERM);
		wait_gone (second, 5);
	}
	g_spawn_close_pid (first);
	g_spawn_close_pid (second);

	g_free (tmp);

	if (failures == 0) {
		g_print ("OK\n");
	}

	return failures == 0 ? 0 : 1;
}
