/* There is no wrapper script in front of the installed program any more, so it
 * points XDG_DATA_DIRS and PATH at its own prefix itself. Proving that needs a
 * binary sitting in a prefix, so the test copies itself into one and runs the
 * copy: the child sets the environment up and prints what it got, the parent
 * checks it. POSIX only. */

#include <config.h>

#include <libnemo-private/nemo-file-utilities.h>

#include <gio/gio.h>
#include <glib/gstdio.h>

#include <string.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

/* The copy's half: set up and report, one variable per line. */
static int
report (void)
{
	nemo_setup_runtime_environment ();

	g_print ("XDG_DATA_DIRS=%s\n", g_getenv ("XDG_DATA_DIRS"));
	g_print ("PATH=%s\n", g_getenv ("PATH"));

	return 0;
}

/* The value the child printed for var, or NULL. */
static char *
value_of (const char *output, const char *var)
{
	char **lines = g_strsplit (output, "\n", -1);
	char *prefixed = g_strconcat (var, "=", NULL);
	char *found = NULL;
	guint i;

	for (i = 0; lines[i] != NULL && found == NULL; i++) {
		if (g_str_has_prefix (lines[i], prefixed)) {
			found = g_strdup (lines[i] + strlen (prefixed));
		}
	}

	g_free (prefixed);
	g_strfreev (lines);

	return found;
}

/* Run the copy at exe with XDG_DATA_DIRS set to preset (NULL to unset it). */
static char *
run_copy (const char *exe, const char *preset)
{
	char *child_argv[] = { (char *) exe, (char *) "--report", NULL };
	char *out = NULL;
	GError *error = NULL;

	/* The child inherits ours, so setting it here is enough. */
	if (preset != NULL) {
		g_setenv ("XDG_DATA_DIRS", preset, TRUE);
	} else {
		g_unsetenv ("XDG_DATA_DIRS");
	}

	if (!g_spawn_sync (NULL, child_argv, NULL, G_SPAWN_DEFAULT, NULL, NULL,
			   &out, NULL, NULL, &error)) {
		g_printerr ("spawn: %s\n", error->message);
		g_error_free (error);
		g_free (out);
		out = NULL;
	}

	return out;
}

int
main (int argc, char *argv[])
{
	char *tmp;
	char *bin;
	char *share;
	char *exe;
	char *self;
	char *out;
	char *dirs;
	char *path;

	if (argc > 1 && strcmp (argv[1], "--report") == 0) {
		return report ();
	}

	tmp = g_dir_make_tmp ("nemo-runtime-env-XXXXXX", NULL);
	g_assert_nonnull (tmp);

	bin = g_build_filename (tmp, "bin", NULL);
	share = g_build_filename (tmp, "share", NULL);
	g_mkdir_with_parents (bin, 0755);
	g_mkdir_with_parents (share, 0755);

	self = nemo_get_exe_path ();
	g_assert_nonnull (self);
	exe = g_build_filename (bin, "probe", NULL);
	{
		char *bytes = NULL;
		gsize len = 0;

		g_assert_true (g_file_get_contents (self, &bytes, &len, NULL));
		g_assert_true (g_file_set_contents (exe, bytes, len, NULL));
		g_free (bytes);
	}
	g_chmod (exe, 0755);

	/* Nothing set: our share dir goes in front of the system default. */
	out = run_copy (exe, NULL);
	g_assert_nonnull (out);
	dirs = value_of (out, "XDG_DATA_DIRS");
	path = value_of (out, "PATH");
	check (dirs != NULL && g_str_has_prefix (dirs, share));
	check (dirs != NULL && strstr (dirs, "/usr/share") != NULL);
	check (path != NULL && g_str_has_prefix (path, bin));
	g_free (dirs);
	g_free (path);
	g_free (out);

	/* Something already set: kept, with ours in front. */
	out = run_copy (exe, "/opt/somewhere/share");
	g_assert_nonnull (out);
	dirs = value_of (out, "XDG_DATA_DIRS");
	check (dirs != NULL && g_str_has_prefix (dirs, share));
	check (dirs != NULL && strstr (dirs, "/opt/somewhere/share") != NULL);
	g_free (dirs);
	g_free (out);

	/* Already in front: left exactly as it was, not doubled up. */
	{
		char *preset = g_strconcat (share, ":/opt/somewhere/share", NULL);

		out = run_copy (exe, preset);
		g_assert_nonnull (out);
		dirs = value_of (out, "XDG_DATA_DIRS");
		check (g_strcmp0 (dirs, preset) == 0);
		g_free (dirs);
		g_free (out);
		g_free (preset);
	}

	g_free (exe);
	g_free (self);
	g_free (share);
	g_free (bin);

	{
		char *cmd[] = { (char *) "rm", (char *) "-rf", tmp, NULL };

		g_spawn_sync (NULL, cmd, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL,
			      NULL, NULL, NULL, NULL);
	}
	g_free (tmp);

	return failures == 0 ? 0 : 1;
}
