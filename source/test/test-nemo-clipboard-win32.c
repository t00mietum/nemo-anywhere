/* Copying a path put nothing on the Windows clipboard that anyone else could
 * read straight away: the toolkit only advertises the text and hands it over
 * when asked, and in a remote desktop session the redirector asks, gets no
 * answer in time, and puts the client's own clipboard back - so the copy read
 * as having done nothing at all.
 *
 * The check reads the clipboard the way another program would, with no message
 * loop running, which is exactly what the advertise-and-wait path cannot serve.
 */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gtk/gtk.h>
#include <windows.h>

#include <libnemo-private/nemo-clipboard.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

/* Whatever CF_UNICODETEXT holds right now, as utf-8, or NULL. */
static char *
clipboard_text (void)
{
	HANDLE block;
	gpointer text;
	char *ret = NULL;

	if (!OpenClipboard (NULL)) {
		return NULL;
	}

	block = GetClipboardData (CF_UNICODETEXT);
	if (block != NULL) {
		text = GlobalLock (block);
		if (text != NULL) {
			ret = g_utf16_to_utf8 (text, -1, NULL, NULL, NULL);
			GlobalUnlock (block);
		}
	}

	CloseClipboard ();
	return ret;
}

int
main (int argc, char *argv[])
{
	GtkWidget *window;
	const char *first = "C:\\some\\path\\one.txt";
	const char *second = "C:\\some\\path\\two.txt\r\nC:\\some\\path\\three.txt";
	char *got;

	if (!gtk_init_check (&argc, &argv)) {
		g_print ("SKIP: no display\n");
		return EXIT_SUCCESS;
	}

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_widget_realize (window);

	nemo_clipboard_set_text (window, first);
	got = clipboard_text ();
	check (g_strcmp0 (got, first) == 0);
	g_free (got);

	/* A second copy has to replace the first, not sit behind it. */
	nemo_clipboard_set_text (window, second);
	got = clipboard_text ();
	check (g_strcmp0 (got, second) == 0);
	g_free (got);

	gtk_widget_destroy (window);

	if (failures > 0) {
		g_printerr ("%d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("clipboard text: all checks passed\n");
	return EXIT_SUCCESS;
}
