/* The window title names the folder and then the program, so a taskbar button
 * says which program it belongs to. The folder part is whatever the tabs show,
 * which is a name or a full path depending on the preference, so this only
 * checks the wrapping around it. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <libnemo-private/nemo-file-utilities.h>

static int failures = 0;

static void
check_is (const char *location_title, const char *expected)
{
	char *got = nemo_compute_window_title (location_title);

	if (g_strcmp0 (got, expected) != 0) {
		g_printerr ("FAIL: %s -> %s (wanted %s)\n",
			    location_title != NULL ? location_title : "(null)",
			    got != NULL ? got : "(null)",
			    expected != NULL ? expected : "(null)");
		failures++;
	}

	g_free (got);
}

/* Long enough to be cut, and made of one character so the expected string can
   be built the same way rather than pasted in. */
static void
check_long_one_is_truncated (void)
{
	char *location_title;
	char *left;
	char *right;
	char *expected;

	location_title = g_strnfill (240, 'a');
	left = g_strnfill (88, 'a');
	right = g_strnfill (89, 'a');
	expected = g_strdup_printf ("%s...%s - Nemo Anywhere", left, right);

	check_is (location_title, expected);

	g_free (location_title);
	g_free (left);
	g_free (right);
	g_free (expected);
}

int
main (int argc, char *argv[])
{
	/* A folder name, which is what the title holds with the full-path
	   preference off. */
	check_is ("Documents", "Documents - Nemo Anywhere");
	check_is ("Home", "Home - Nemo Anywhere");

	/* A path, which is what it holds with the preference on. Both separators,
	   since the display separator follows the platform. */
	check_is ("/home/somebody/Documents",
		  "/home/somebody/Documents - Nemo Anywhere");
	check_is ("C:\\Users\\somebody\\Documents",
		  "C:\\Users\\somebody\\Documents - Nemo Anywhere");

	/* A space anywhere in it, folder or parent, puts the whole thing in
	   quotes, so the " - " before the program name cannot be mistaken for
	   part of it. */
	check_is ("My Documents", "\"My Documents\" - Nemo Anywhere");
	check_is ("/home/some body/Documents",
		  "\"/home/some body/Documents\" - Nemo Anywhere");
	check_is ("a\tb", "\"a\tb\" - Nemo Anywhere");

	/* A drive root keeps the name the sidebar gives it, not a bare separator.
	   That was a bug once; see the closed item in the backlog. */
	check_is ("Windows (C:)", "\"Windows (C:)\" - Nemo Anywhere");

	/* Nothing to name yet - a window that has not loaded a location. */
	check_is (NULL, "Nemo Anywhere");
	check_is ("", "Nemo Anywhere");

	/* A quote in a folder name is left alone, so the title can be ambiguous.
	   Escaping it would read worse than the odd name does. */
	check_is ("it's mine", "\"it's mine\" - Nemo Anywhere");
	check_is ("say \"hi\"", "\"say \"hi\"\" - Nemo Anywhere");

	check_long_one_is_truncated ();

	if (failures > 0) {
		g_printerr ("%d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("window-title: all checks passed\n");
	return EXIT_SUCCESS;
}
