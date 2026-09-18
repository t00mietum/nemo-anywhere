/* Deleting or moving a link takes the link and nothing it points at. Runs the
 * real jobs; whatever they ask is answered yes from here, since nobody is
 * there to click. One job per run, for the reason given in
 * test-nemo-link-copy-job.c.
 *
 * Argument: "delete" (default) or "move". The move needs a second file system
 * so the job falls back to copy and delete, and asks for the contents. A move
 * takes the link anyway. On Windows it used to walk into a junction, which GIO
 * calls a folder, and move the contents out of the folder it pointed at.
 */

#include "test.h"

#include <libnemo-private/nemo-file-operations.h>
#include <libnemo-private/nemo-link-copy.h>
#ifdef G_OS_WIN32
#include <windows.h>
#include <libnemo-private/nemo-shortcut-win32.h>
#endif

#include <glib/gstdio.h>
#include <stdlib.h>
#include <string.h>

#include "test-scratch.h"

#define JOB_TIMEOUT_SECONDS 30

static int failures;
static gboolean job_finished;

#define check(expr)							\
	G_STMT_START {							\
		if (!(expr)) {						\
			g_printerr ("FAIL %s:%d: %s\n",			\
				    __FILE__, __LINE__, #expr);		\
			failures++;					\
		}							\
	} G_STMT_END

static void
delete_done (GHashTable *debuting_uris, gboolean user_cancel, gpointer data)
{
	job_finished = TRUE;
	gtk_main_quit ();
}

static void
move_done (GHashTable *debuting_uris, gboolean success, gpointer data)
{
	job_finished = TRUE;
	gtk_main_quit ();
}

static gboolean
give_up (gpointer data)
{
	g_printerr ("FAIL: the job did not finish within %d seconds\n",
		    JOB_TIMEOUT_SECONDS);
	failures++;
	gtk_main_quit ();
	return G_SOURCE_REMOVE;
}

/* The go-ahead is the last button on every dialog these jobs put up. An error
   dialog gets its last button too, which is Skip, and says so on stderr. */
static gboolean
answer_dialogs (gpointer data)
{
	GList *windows, *l;

	windows = gtk_window_list_toplevels ();
	for (l = windows; l != NULL; l = l->next) {
		GtkWidget *area;
		GList *buttons;
		char *text = NULL;

		if (!GTK_IS_DIALOG (l->data) || !gtk_widget_get_mapped (l->data)) {
			continue;
		}

		if (GTK_IS_MESSAGE_DIALOG (l->data)) {
			g_object_get (l->data, "text", &text, NULL);
		}
		g_printerr ("answering: %s\n", text != NULL ? text : "(a dialog)");
		g_free (text);

		area = gtk_dialog_get_action_area (GTK_DIALOG (l->data));
		buttons = gtk_container_get_children (GTK_CONTAINER (area));
		if (buttons != NULL) {
			gtk_button_clicked (GTK_BUTTON (g_list_last (buttons)->data));
		}
		g_list_free (buttons);
	}
	g_list_free (windows);

	return G_SOURCE_CONTINUE;
}

static gboolean
exists (const char *path)
{
	GStatBuf st;

	return g_lstat (path, &st) == 0;
}

static gboolean
make_folder_link (const char *target, const char *link)
{
#ifdef G_OS_WIN32
	return nemo_link_create (target, link, NULL, NEMO_LINK_JUNCTION, NULL);
#else
	return nemo_link_create (target, link, NULL, NEMO_LINK_DIR_SYMLINK, NULL);
#endif
}

static void
run_job (GtkWidget *window, GList *sources, GFile *dest)
{
	guint timeout_id, answer_id;

	answer_id = g_timeout_add (100, answer_dialogs, NULL);
	timeout_id = g_timeout_add_seconds (JOB_TIMEOUT_SECONDS, give_up, NULL);

	if (dest == NULL) {
		nemo_file_operations_delete (sources, GTK_WINDOW (window), delete_done, NULL);
	} else {
		g_setenv ("NEMO_LINK_COPY", "copy", TRUE);
		nemo_file_operations_move (sources, NULL, dest, GTK_WINDOW (window), move_done, NULL);
	}

	gtk_main ();
	g_source_remove (timeout_id);
	g_source_remove (answer_id);

	check (job_finished);
}

/* A folder full of links, and a link on its own, deleted together. */
static int
test_delete (const char *root, const char *outside, const char *precious, const char *deep)
{
	GtkWidget *window;
	GList *sources;
	char *victim, *inner, *inner_file, *dir_link, *file_link, *shortcut, *lone_link;

	victim = g_build_filename (root, "victim", NULL);
	inner = g_build_filename (victim, "inner", NULL);
	inner_file = g_build_filename (inner, "scratch.txt", NULL);
	dir_link = g_build_filename (victim, "folder-link", NULL);
	file_link = g_build_filename (victim, "file-link", NULL);
	lone_link = g_build_filename (root, "lone-link", NULL);
	g_mkdir_with_parents (inner, 0700);
	check (g_file_set_contents (inner_file, "x", -1, NULL));

	if (!make_folder_link (outside, dir_link) || !make_folder_link (outside, lone_link)) {
		g_printerr ("SKIP: no folder links here\n");
		return 77;
	}

	if (nemo_link_kinds_supported (root) & NEMO_LINK_FILE_SYMLINK) {
		check (nemo_link_create (precious, file_link, NULL, NEMO_LINK_FILE_SYMLINK, NULL));
	}

#ifdef G_OS_WIN32
	shortcut = g_build_filename (victim, "shortcut.lnk", NULL);
	check (nemo_shortcut_win32_create (outside, shortcut, NULL, NULL, NULL, NULL));
#else
	{
		char *uri = g_filename_to_uri (outside, NULL, NULL);
		char *entry = g_strdup_printf ("[Desktop Entry]\nType=Link\nName=Outside\nURL=%s\n", uri);

		shortcut = g_build_filename (victim, "outside.desktop", NULL);
		check (g_file_set_contents (shortcut, entry, -1, NULL));
		g_free (entry);
		g_free (uri);
	}
#endif

	window = test_window_new ("link delete test", 5);
	gtk_widget_show (window);

	sources = g_list_prepend (NULL, g_file_new_for_path (lone_link));
	sources = g_list_prepend (sources, g_file_new_for_path (victim));
	run_job (window, sources, NULL);
	g_list_free_full (sources, g_object_unref);

	check (!exists (victim));
	check (!exists (lone_link));
	check (exists (precious));
	check (exists (deep));

	g_free (shortcut);
	g_free (lone_link);
	g_free (file_link);
	g_free (dir_link);
	g_free (inner_file);
	g_free (inner);
	g_free (victim);

	return 0;
}

/* Somewhere on another file system than root, or NULL. */
static char *
other_volume (const char *root)
{
#ifdef G_OS_WIN32
	wchar_t *wide_root;
	DWORD drives, root_serial = 0;
	char letter;

	wide_root = g_utf8_to_utf16 (root, 3, NULL, NULL, NULL);
	if (wide_root == NULL ||
	    !GetVolumeInformationW (wide_root, NULL, 0, &root_serial, NULL, NULL, NULL, 0)) {
		g_free (wide_root);
		return NULL;
	}
	g_free (wide_root);

	drives = GetLogicalDrives ();
	for (letter = 'C'; letter <= 'Z'; letter++) {
		wchar_t drive[] = { (wchar_t) letter, L':', L'\\', 0 };
		DWORD serial = 0;

		if (!(drives & (1u << (letter - 'A'))) || GetDriveTypeW (drive) != DRIVE_FIXED) {
			continue;
		}
		if (GetVolumeInformationW (drive, NULL, 0, &serial, NULL, NULL, NULL, 0) &&
		    serial != root_serial) {
			return g_strdup_printf ("%c:\\", letter);
		}
	}
	return NULL;
#else
	GStatBuf root_st, other_st;

	if (g_stat (root, &root_st) == 0 && g_stat ("/dev/shm", &other_st) == 0 &&
	    root_st.st_dev != other_st.st_dev) {
		return g_strdup ("/dev/shm");
	}
	return NULL;
#endif
}

/* A folder link moved to another file system, told to take what it holds. It
   goes as a link, and the folder it pointed at keeps what it holds. */
static int
test_move (const char *root, const char *outside, const char *precious, const char *deep)
{
	GtkWidget *window;
	GList *sources;
	GFile *dest, *landed_gf;
	NemoLinkKind landed_kind;
	char *base, *other, *link, *landed, *landed_file;

	base = other_volume (root);
	if (base == NULL) {
		g_printerr ("SKIP: no second file system here\n");
		return 77;
	}

	other = test_scratch_dir_in (base, "nemo-link-move-XXXXXX", NULL);
	if (other == NULL) {
		g_printerr ("SKIP: could not make a folder in %s\n", base);
		g_free (base);
		return 77;
	}

	link = g_build_filename (root, "folder-link", NULL);
	landed = g_build_filename (other, "folder-link", NULL);
	landed_file = g_build_filename (landed, "precious.txt", NULL);
	check (make_folder_link (outside, link));

	window = test_window_new ("link move test", 5);
	gtk_widget_show (window);

	sources = g_list_prepend (NULL, g_file_new_for_path (link));
	dest = g_file_new_for_path (other);
	run_job (window, sources, dest);
	g_list_free_full (sources, g_object_unref);
	g_object_unref (dest);

	check (!exists (link));
	check (exists (landed_file));
	check (exists (precious));
	check (exists (deep));

	landed_gf = g_file_new_for_path (landed);
	landed_kind = nemo_link_kind (landed_gf, NULL);
	g_object_unref (landed_gf);
#ifdef G_OS_WIN32
	check (landed_kind == NEMO_LINK_JUNCTION);
#else
	check (landed_kind == NEMO_LINK_DIR_SYMLINK);
#endif

	g_free (landed_file);
	g_free (landed);
	g_free (link);
	g_free (other);
	g_free (base);

	return 0;
}

int
main (int argc, char *argv[])
{
	const char *how;
	char *root, *outside, *sub, *precious, *deep;
	int res;

	test_init (&argc, &argv);

	how = (argc > 1) ? argv[1] : "delete";

	root = test_scratch_dir ("nemo-link-delete-XXXXXX", NULL);
	outside = g_build_filename (root, "outside", NULL);
	sub = g_build_filename (outside, "sub", NULL);
	precious = g_build_filename (outside, "precious.txt", NULL);
	deep = g_build_filename (sub, "deep.txt", NULL);
	g_mkdir_with_parents (sub, 0700);
	check (g_file_set_contents (precious, "keep me", -1, NULL));
	check (g_file_set_contents (deep, "and me", -1, NULL));

	if (strcmp (how, "move") == 0) {
		res = test_move (root, outside, precious, deep);
	} else {
		res = test_delete (root, outside, precious, deep);
	}

	g_free (deep);
	g_free (precious);
	g_free (sub);
	g_free (outside);
	g_free (root);

	if (res == 77) {
		return 77;
	}
	if (failures > 0) {
		g_printerr ("%d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("OK\n");
	return EXIT_SUCCESS;
}
