/* What the three owner columns show. Owner is the user name alone, Owner name
 * the display name alone, and Owner - name both when they differ. An empty
 * display name, which is what an empty GECOS field gives, counts as none: it
 * once came out as "alice - ". */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>

#include <libnemo-extension/nemo-column.h>
#include <libnemo-private/nemo-column-utilities.h>
#include <libnemo-private/nemo-file.h>
#include <libnemo-private/nemo-file-private.h>

static int failures = 0;

static void
set_owner (NemoFile *file, const char *owner, const char *owner_real)
{
	g_clear_pointer (&file->details->owner, g_ref_string_release);
	g_clear_pointer (&file->details->owner_real, g_ref_string_release);

	if (owner != NULL) {
		file->details->owner = g_ref_string_new_intern (owner);
	}
	if (owner_real != NULL) {
		file->details->owner_real = g_ref_string_new_intern (owner_real);
	}
}

static void
check_attribute (NemoFile *file, const char *attribute, const char *expected)
{
	char *got = nemo_file_get_string_attribute (file, attribute);

	if (g_strcmp0 (got, expected) != 0) {
		g_printerr ("FAIL: %s is '%s', wanted '%s' (owner '%s', real '%s')\n",
			    attribute,
			    got != NULL ? got : "(null)",
			    expected != NULL ? expected : "(null)",
			    file->details->owner != NULL ? file->details->owner : "(null)",
			    file->details->owner_real != NULL ? file->details->owner_real : "(null)");
		failures++;
	}

	g_free (got);
}

static void
check_case (NemoFile *file, const char *owner, const char *owner_real,
	    const char *want_owner, const char *want_name, const char *want_both)
{
	set_owner (file, owner, owner_real);
	check_attribute (file, "owner", want_owner);
	check_attribute (file, "owner_name", want_name);
	check_attribute (file, "owner_and_name", want_both);
}

static gboolean
has_column (GList *columns, const char *wanted)
{
	GList *l;
	gboolean found = FALSE;

	for (l = columns; l != NULL && !found; l = l->next) {
		char *name;

		g_object_get (l->data, "name", &name, NULL);
		found = g_strcmp0 (name, wanted) == 0;
		g_free (name);
	}

	return found;
}

int
main (int argc, char *argv[])
{
	NemoFile *file;
	GList *columns;

	/* NemoFile's class setup watches the icon theme, which wants a screen. */
	if (!gtk_init_check (&argc, &argv)) {
		g_print ("SKIP: no display\n");
		return 77;
	}

	file = nemo_file_get_by_uri ("file:///nemo-owner-columns-test/file.txt");

	check_case (file, "alice", "Alice Smith", "alice", "Alice Smith", "alice - Alice Smith");
	check_case (file, "alice", "", "alice", NULL, "alice");
	check_case (file, "alice", NULL, "alice", NULL, "alice");
	/* GIO falls back to the user name when there is no GECOS field at all. */
	check_case (file, "alice", "alice", "alice", "alice", "alice");
	check_case (file, NULL, NULL, NULL, NULL, NULL);

	nemo_file_unref (file);

	/* GIO has no display name on Windows, so the two name columns are not
	   offered there. */
	columns = nemo_get_all_columns ();
	if (!has_column (columns, "owner")) {
		g_printerr ("FAIL: no owner column\n");
		failures++;
	}
#ifdef G_OS_WIN32
	if (has_column (columns, "owner_name") || has_column (columns, "owner_and_name")) {
		g_printerr ("FAIL: an owner name column is offered on Windows\n");
		failures++;
	}
#else
	if (!has_column (columns, "owner_name") || !has_column (columns, "owner_and_name")) {
		g_printerr ("FAIL: an owner name column is missing\n");
		failures++;
	}
#endif
	nemo_column_list_free (columns);

	if (failures > 0) {
		g_printerr ("%d check(s) failed\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("OK\n");
	return EXIT_SUCCESS;
}
