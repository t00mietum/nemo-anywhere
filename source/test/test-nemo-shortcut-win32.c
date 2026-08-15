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

/* The shell always reports canonical long paths, while TEMP on a stock Windows
 * box is often still the 8.3 short form (C:\Users\COLLIE~1\...) - which is what
 * g_dir_make_tmp builds on. Compare both sides on the long form or a perfectly
 * good shortcut reads as a mismatch. Falls back to the input unchanged, so a
 * path the API won't expand still gets compared rather than dropped. */
static char *
long_path (const char *path)
{
	gunichar2 *w_in;
	wchar_t *buf;
	DWORD needed;
	char *ret;

	w_in = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
	if (w_in == NULL) {
		return g_strdup (path);
	}

	needed = GetLongPathNameW ((LPCWSTR) w_in, NULL, 0);
	if (needed == 0) {
		g_free (w_in);
		return g_strdup (path);
	}

	buf = g_new (wchar_t, needed);
	if (GetLongPathNameW ((LPCWSTR) w_in, buf, needed) == 0) {
		ret = g_strdup (path);
	} else {
		ret = g_utf16_to_utf8 ((const gunichar2 *) buf, -1, NULL, NULL, NULL);
		if (ret == NULL) {
			ret = g_strdup (path);
		}
	}

	g_free (buf);
	g_free (w_in);
	return ret;
}

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
		char *want = long_path (target);
		char *resolved = resolve_target (lnk);
		char *got = resolved ? long_path (resolved) : NULL;

		check (resolved != NULL);
		/* Case-insensitive: the shell may normalise the drive/casing. */
		check (got != NULL && g_ascii_strcasecmp (got, want) == 0);
		g_free (resolved);
		g_free (got);

		/* Same round-trip through the public read helper (follow-on-open). */
		char *readback = NULL;
		GError *rerr = NULL;
		check (nemo_shortcut_win32_read (lnk, &readback, &rerr));
		check (rerr == NULL);
		got = readback ? long_path (readback) : NULL;
		check (got != NULL && g_ascii_strcasecmp (got, want) == 0);
		g_clear_error (&rerr);
		g_free (readback);
		g_free (got);
		g_free (want);
	}

	g_clear_error (&error);

	/* Creating over something that already exists must report the clash rather
	 * than overwrite it - the caller relies on G_IO_ERROR_EXISTS to retry under
	 * a free name, and anything already at that path must survive untouched. */
	{
		char *occupied = g_build_filename (dir, "occupied.lnk", NULL);
		char *contents = NULL;
		GError *eerr = NULL;

		check (g_file_set_contents (occupied, "do not clobber", -1, NULL));

		check (!nemo_shortcut_win32_create (target, occupied, NULL, NULL, NULL, &eerr));
		check (eerr != NULL);
		check (eerr != NULL && g_error_matches (eerr, G_IO_ERROR, G_IO_ERROR_EXISTS));

		check (g_file_get_contents (occupied, &contents, NULL, NULL));
		check (contents != NULL && strcmp (contents, "do not clobber") == 0);

		g_free (contents);
		g_clear_error (&eerr);
		g_unlink (occupied);
		g_free (occupied);
	}

	/* A refused create must always leave an error behind: the file-operations
	 * caller reads error->message straight off the failure path. */
	{
		char *nested = g_build_filename (dir, "no-such-dir", "x.lnk", NULL);
		GError *eerr = NULL;

		check (!nemo_shortcut_win32_create (target, nested, NULL, NULL, NULL, &eerr));
		check (eerr != NULL);
		check (eerr == NULL || eerr->message != NULL);

		g_clear_error (&eerr);
		g_free (nested);
	}

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
