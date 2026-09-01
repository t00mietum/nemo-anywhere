/* What a long path looks like in a title once the middle is left out. The end
 * has to survive, since that is what tells one tab from another, and so does the
 * root, which says which drive or share it is on. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <libnemo-private/nemo-file-utilities.h>

static int failures = 0;

static void
check_is (const char *path, char separator, gsize limit, const char *expected)
{
	char *got = nemo_path_shorten (path, separator, limit);

	if (g_strcmp0 (got, expected) != 0) {
		g_printerr ("FAIL: %s -> %s (wanted %s)\n",
			    path != NULL ? path : "(null)",
			    got != NULL ? got : "(null)",
			    expected != NULL ? expected : "(null)");
		failures++;
	}

	g_free (got);
}

int
main (int argc, char *argv[])
{
	/* Anything inside the limit is left exactly as it was. */
	check_is ("C:\\Users\\somebody\\Documents", '\\', 52,
		  "C:\\Users\\somebody\\Documents");
	check_is ("/home/somebody/Documents", '/', 52,
		  "/home/somebody/Documents");
	check_is (NULL, '/', 52, NULL);

	/* The case from the backlog. */
	check_is ("C:\\opt\\0-0\\users\\somebody\\data\\prs\\dev\\github.com\\someone\\nemo-anywhere\\github",
		  '\\', 52,
		  "C:\\opt\\0-0\\users\\somebody\\...\\nemo-anywhere\\github");

	/* A posix path keeps its leading slash, which splits to an empty first
	   component - the root is still the first thing shown. */
	check_is ("/home/somebody/data/prs/dev/github.com/someone/nemo-anywhere/github",
		  '/', 52,
		  "/home/somebody/data/prs/dev/.../nemo-anywhere/github");

	/* Too few components to leave anything out, however long they are. */
	check_is ("C:\\a-very-long-folder-name-that-goes-on-and-on-and-on\\here",
		  '\\', 52,
		  "C:\\a-very-long-folder-name-that-goes-on-and-on-and-on\\here");

	/* Nothing but the root and the last two survive when the limit is tiny. */
	check_is ("C:\\one\\two\\three\\four\\five\\six", '\\', 8,
		  "C:\\...\\five\\six");

	if (failures > 0) {
		g_printerr ("%d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("path-shorten: all checks passed\n");
	return EXIT_SUCCESS;
}
