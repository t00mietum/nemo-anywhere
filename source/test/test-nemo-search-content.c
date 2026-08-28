/* Content search over a temp dir. On win32 GIO calls the extension the content
 * type (".txt"), so the old is_a("text/plain") test answered no for every file
 * and "Containing:" never found anything at all. Text with an extension GIO does
 * not know, and text with no extension, have to be found by their bytes. */

#include <config.h>

#include <string.h>
#include <gtk/gtk.h>
#include <libnemo-private/nemo-global-preferences.h>
#include <libnemo-private/nemo-search-engine-advanced.h>
#include <libnemo-private/nemo-query.h>

static gboolean done;
static GList *found;

static void
hits_added_cb (NemoSearchEngine *engine, GList *hits, gpointer data)
{
	for (GList *l = hits; l != NULL; l = l->next) {
		FileSearchResult *result = l->data;
		found = g_list_prepend (found, g_path_get_basename (result->uri));
	}
}

static void
finished_cb (NemoSearchEngine *engine, gpointer data)
{
	done = TRUE;
}

static void
write_file (const char *dir, const char *name, const char *contents, gssize len)
{
	char *path = g_build_filename (dir, name, NULL);

	if (!g_file_set_contents (path, contents, len, NULL)) {
		g_error ("could not write %s", path);
	}

	g_free (path);
}

static gboolean
was_found (const char *name)
{
	for (GList *l = found; l != NULL; l = l->next) {
		if (g_strcmp0 (l->data, name) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

int
main (int argc, char *argv[])
{
	NemoSearchEngine *engine;
	NemoQuery        *query;
	char             *dir, *uri;
	int               spins = 0;
	int               failures = 0;

	/* Every test that reads a preference points these at a throwaway dir first. */
	char *scratch = g_dir_make_tmp ("nemo-search-content-home-XXXXXX", NULL);
	g_setenv ("HOME", scratch, TRUE);
	g_setenv ("APPDATA", scratch, TRUE);
	g_setenv ("XDG_CONFIG_HOME", scratch, TRUE);

	gtk_init_check (&argc, &argv);
	nemo_global_preferences_init ();

	dir = g_dir_make_tmp ("nemo-search-content-XXXXXX", NULL);

	write_file (dir, "plain.txt", "the needle is here\n", -1);
	write_file (dir, "notes.md", "# heading\n\nthe needle is here too\n", -1);
	write_file (dir, "readme", "no extension, needle all the same\n", -1);
	write_file (dir, "other.txt", "nothing of interest\n", -1);
	/* A PNG header, then the word - binary, so it must not be read as text. */
	write_file (dir, "image.png", "\x89PNG\r\n\x1a\n\x00\x00\x00\x0dneedle", 21);

	engine = nemo_search_engine_advanced_new ();
	g_signal_connect (engine, "hits-added", G_CALLBACK (hits_added_cb), NULL);
	g_signal_connect (engine, "finished", G_CALLBACK (finished_cb), NULL);

	query = nemo_query_new ();
	uri = g_filename_to_uri (dir, NULL, NULL);
	nemo_query_set_location (query, uri);
	g_free (uri);
	nemo_query_set_content_pattern (query, "needle");
	nemo_search_engine_set_query (engine, query);
	g_object_unref (query);

	nemo_search_engine_start (engine);

	while (!done && spins++ < 500) {
		g_main_context_iteration (NULL, FALSE);
		g_usleep (10000);
	}

	if (!done) {
		g_printerr ("FAIL: search never finished\n");
		return 1;
	}

	const char *expected[] = { "plain.txt", "notes.md", "readme" };

	for (gsize i = 0; i < G_N_ELEMENTS (expected); i++) {
		if (!was_found (expected[i])) {
			g_printerr ("FAIL: '%s' contains the pattern and was not found\n", expected[i]);
			failures++;
		}
	}

	if (was_found ("other.txt")) {
		g_printerr ("FAIL: 'other.txt' does not contain the pattern and was found\n");
		failures++;
	}

	if (was_found ("image.png")) {
		g_printerr ("FAIL: 'image.png' is binary and was searched anyway\n");
		failures++;
	}

	g_object_unref (engine);
	g_list_free_full (found, g_free);
	g_free (dir);
	g_free (scratch);

	if (failures > 0) {
		return 1;
	}

	g_print ("nemo-search-content: all checks passed\n");
	return 0;
}
