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
