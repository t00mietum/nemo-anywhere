/* Argument quoting for the two Windows shell hand-offs, "Open as Administrator"
 * and "Open in Terminal". Both build a command line that the target process
 * re-splits under MSVCRT rules, and neither can be driven unattended - one puts
 * up a UAC prompt, the other opens a console - so the quoting is checked here
 * instead. Windows-only; the Linux build compiles it out. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#ifdef G_OS_WIN32

#include <src/nemo-view-win32.h>

static int failures = 0;

static void
check_quote (const char *arg, const char *want)
{
	char *got = nemo_view_win32_quote_arg (arg);

	if (g_strcmp0 (got, want) != 0) {
		g_printerr ("FAIL %s -> %s, wanted %s\n", arg, got, want);
		failures++;
	}
	g_free (got);
}

int
main (int argc, char *argv[])
{
	/* Ordinary paths: interior separators are literal, spaces are covered by
	 * the surrounding quotes and nothing else changes. */
	check_quote ("C:\\Users\\someone", "\"C:\\Users\\someone\"");
	check_quote ("C:\\Program Files\\Thing", "\"C:\\Program Files\\Thing\"");

	/* A drive root. The trailing backslash has to be doubled or it escapes the
	 * closing quote, and the target sees one unterminated argument instead of a
	 * path - which is what "Open in Terminal" on C:\ used to send. */
	check_quote ("C:\\", "\"C:\\\\\"");
	check_quote ("\\\\server\\share\\", "\"\\\\server\\share\\\\\"");

	/* A quote in the name, and a backslash run before one. */
	check_quote ("a\"b", "\"a\\\"b\"");
	check_quote ("a\\\"b", "\"a\\\\\\\"b\"");

	/* Nothing to quote is still a quoted empty argument, not an omitted one. */
	check_quote ("", "\"\"");

	/* "Open with Explorer" puts a window on the screen, so it is not something
	 * an unattended run can do. Set NEMO_PROBE_EXPLORER to a folder to watch it
	 * open that folder and then pick a file out of it. */
	{
		const char *probe_dir = g_getenv ("NEMO_PROBE_EXPLORER");

		if (probe_dir == NULL) {
			g_print ("SKIP open with Explorer (opens a window; set NEMO_PROBE_EXPLORER to a folder to run it)\n");
		} else {
			char *file = g_build_filename (probe_dir, "explorer-probe.txt", NULL);

			if (!g_file_set_contents (file, "probe\n", -1, NULL)) {
				g_printerr ("FAIL cannot write %s\n", file);
				failures++;
			}

			nemo_view_win32_open_in_explorer (probe_dir, TRUE);
			g_usleep (2 * G_USEC_PER_SEC);
			nemo_view_win32_open_in_explorer (file, FALSE);

			g_print ("probed open with Explorer on %s\n", file);
			g_free (file);
		}
	}

	if (failures == 0) {
		g_print ("view-win32: all checks passed\n");
	}
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#else /* !G_OS_WIN32 */

int
main (int argc, char *argv[])
{
	return EXIT_SUCCESS;
}

#endif
