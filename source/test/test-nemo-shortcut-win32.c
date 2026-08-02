/* Exercises Windows .lnk shortcut creation (nemo_shortcut_win32_create): make a
 * shortcut to a real file, then load it back through the shell IShellLinkW to
 * confirm the stored target round-trips. Windows-only; Linux compiles it out. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

#ifdef G_OS_WIN32

#include <libnemo-private/nemo-shortcut-win32.h>

#define COBJMACROS
#include <windows.h>
#include <shlobj.h>
#include <objidl.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

/* Read the target a .lnk points at, via the shell - proves it is a genuine,
 * loadable shortcut and not just an arbitrary file we wrote. */
static char *
resolve_target (const char *lnk_path)
{
	IShellLinkW *link = NULL;
	IPersistFile *pf = NULL;
	gunichar2 *w_lnk;
	wchar_t buf[MAX_PATH];
	char *ret = NULL;

	w_lnk = g_utf8_to_utf16 (lnk_path, -1, NULL, NULL, NULL);
	if (w_lnk == NULL) {
		return NULL;
	}

	CoInitializeEx (NULL, COINIT_APARTMENTTHREADED);

	if (SUCCEEDED (CoCreateInstance (&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
					 &IID_IShellLinkW, (void **) &link)) &&
	    SUCCEEDED (IShellLinkW_QueryInterface (link, &IID_IPersistFile, (void **) &pf)) &&
	    SUCCEEDED (IPersistFile_Load (pf, w_lnk, STGM_READ)) &&
	    SUCCEEDED (IShellLinkW_GetPath (link, buf, MAX_PATH, NULL, 0))) {
		ret = g_utf16_to_utf8 ((const gunichar2 *) buf, -1, NULL, NULL, NULL);
	}

	if (pf != NULL) {
		IPersistFile_Release (pf);
	}
	if (link != NULL) {
		IShellLinkW_Release (link);
	}
	CoUninitialize ();
	g_free (w_lnk);
	return ret;
}

int
main (int argc, char *argv[])
{
	char *dir, *target, *lnk;
	GError *error = NULL;
	gboolean ok;

	dir = g_dir_make_tmp ("nemo-lnk-XXXXXX", NULL);
	g_assert (dir != NULL);

	target = g_build_filename (dir, "target.txt", NULL);
	check (g_file_set_contents (target, "hello", -1, NULL));

	lnk = g_build_filename (dir, "shortcut.lnk", NULL);

	ok = nemo_shortcut_win32_create (target, lnk, NULL, NULL, "test shortcut", &error);
	check (ok);
	check (error == NULL);
	check (g_file_test (lnk, G_FILE_TEST_EXISTS));

	if (ok) {
		char *resolved = resolve_target (lnk);
		check (resolved != NULL);
		/* Case-insensitive: the shell may normalise the drive/casing. */
		check (resolved != NULL && g_ascii_strcasecmp (resolved, target) == 0);
		g_free (resolved);
	}

	g_clear_error (&error);
	g_unlink (lnk);
	g_unlink (target);
	g_rmdir (dir);
	g_free (lnk);
	g_free (target);
	g_free (dir);

	if (failures == 0) {
		g_print ("shortcut-win32: all checks passed\n");
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
