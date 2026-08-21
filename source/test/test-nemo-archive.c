/* The parts of archive creation that can be decided without writing anything:
 * which program would be reached for given a format and a set of options, what
 * switches it would be handed, how the name and the extension move together,
 * and how a volume size is read. The writing itself needs real files and a real
 * job queue, so it is not covered here. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>

#include <libnemo-private/nemo-archive.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

/* Does argv carry this exact switch? */
static gboolean
has_arg (char       **argv,
	 const char  *wanted)
{
	int i;

	for (i = 0; argv != NULL && argv[i] != NULL; i++) {
		if (g_strcmp0 (argv[i], wanted) == 0) {
			return TRUE;
		}
	}

	return FALSE;
}

static gboolean
has_prefix_arg (char       **argv,
		const char  *prefix)
{
	int i;

	for (i = 0; argv != NULL && argv[i] != NULL; i++) {
		if (g_str_has_prefix (argv[i], prefix)) {
			return TRUE;
		}
	}

	return FALSE;
}

static void
check_extensions (void)
{
	char *text;

	/* Changing the format swaps the extension rather than stacking one on
	   top of the other, and the two-part ones are read whole. */
	text = nemo_archive_apply_extension ("photos.tar.gz", NEMO_ARCHIVE_FORMAT_ZIP);
	check (g_strcmp0 (text, "photos.zip") == 0);
	g_free (text);

	text = nemo_archive_apply_extension ("photos.zip", NEMO_ARCHIVE_FORMAT_TAR_XZ);
	check (g_strcmp0 (text, "photos.tar.xz") == 0);
	g_free (text);

	/* A dot in the name that is not an archive suffix stays put. */
	text = nemo_archive_apply_extension ("report.v2", NEMO_ARCHIVE_FORMAT_7Z);
	check (g_strcmp0 (text, "report.v2.7z") == 0);
	g_free (text);

	/* A file actually called ".zip" has no base to strip. */
	text = nemo_archive_strip_extension (".zip");
	check (g_strcmp0 (text, ".zip") == 0);
	g_free (text);

	/* Case does not matter on the way in, but the suffix we write is ours. */
	text = nemo_archive_apply_extension ("Notes.ZIP", NEMO_ARCHIVE_FORMAT_ZIP);
	check (g_strcmp0 (text, "Notes.zip") == 0);
	g_free (text);
}

/* Builds a list of GFile * from uris. */
static GList *
files_for_uris (const char * const *uris)
{
	GList *files = NULL;
	int i;

	for (i = 0; uris[i] != NULL; i++) {
		files = g_list_prepend (files, g_file_new_for_uri (uris[i]));
	}

	return g_list_reverse (files);
}

/* The name offered for a selection. One item is named after itself; several
 * borrow the folder's name only when they are the whole folder. */
static void
check_names (void)
{
	static const char * const one[] = { "file:///tmp/photos/holiday.jpg", NULL };
	static const char * const one_folder[] = { "file:///tmp/photos", NULL };
	static const char * const several[] = {
		"file:///tmp/photos/one.jpg", "file:///tmp/photos/two.jpg", NULL
	};
	static const char * const root[] = { "file:///", NULL };
	GList *files;
	char *text;

	/* A single file, extension and all, keeps its whole name - "holiday.jpg"
	   compresses to "holiday.jpg.zip", not "holiday.zip". */
	files = files_for_uris (one);
	text = nemo_archive_suggest_name (files, FALSE, NEMO_ARCHIVE_FORMAT_ZIP);
	check (g_strcmp0 (text, "holiday.jpg.zip") == 0);
	g_free (text);
	g_list_free_full (files, g_object_unref);

	/* A single folder is named after itself, and the two-part suffix is
	   carried whole. */
	files = files_for_uris (one_folder);
	text = nemo_archive_suggest_name (files, FALSE, NEMO_ARCHIVE_FORMAT_TAR_GZ);
	check (g_strcmp0 (text, "photos.tar.gz") == 0);
	g_free (text);
	g_list_free_full (files, g_object_unref);

	/* Everything in the folder: the archive takes the folder's name. */
	files = files_for_uris (several);
	text = nemo_archive_suggest_name (files, TRUE, NEMO_ARCHIVE_FORMAT_ZIP);
	check (g_strcmp0 (text, "photos.zip") == 0);
	g_free (text);

	/* The same files as a part of the folder: no name, so the user picks
	   one rather than getting the folder's by accident. */
	text = nemo_archive_suggest_name (files, FALSE, NEMO_ARCHIVE_FORMAT_ZIP);
	check (text == NULL);
	g_free (text);
	g_list_free_full (files, g_object_unref);

	/* A root has no name to borrow. */
	files = files_for_uris (root);
	text = nemo_archive_suggest_name (files, FALSE, NEMO_ARCHIVE_FORMAT_ZIP);
	check (text == NULL);
	g_free (text);
	g_list_free_full (files, g_object_unref);

	/* Nothing selected cannot be compressed, so there is nothing to call it. */
	text = nemo_archive_suggest_name (NULL, TRUE, NEMO_ARCHIVE_FORMAT_ZIP);
	check (text == NULL);
	g_free (text);
}

/* The name each archive gets when a selection is compressed separately. */
static void
check_each_names (void)
{
	char *text;

	/* Named after the item, extension and all, the same as a single
	   selection would be offered. */
	text = nemo_archive_each_name ("holiday.jpg", NEMO_ARCHIVE_FORMAT_ZIP);
	check (g_strcmp0 (text, "holiday.jpg.zip") == 0);
	g_free (text);

	text = nemo_archive_each_name ("photos", NEMO_ARCHIVE_FORMAT_TAR_GZ);
	check (g_strcmp0 (text, "photos.tar.gz") == 0);
	g_free (text);

	/* An archive suffix is kept rather than swapped. There is no name field
	   to correct here, so swapping it would put the new archive on top of
	   the file being read. */
	text = nemo_archive_each_name ("old.zip", NEMO_ARCHIVE_FORMAT_ZIP);
	check (g_strcmp0 (text, "old.zip.zip") == 0);
	check (g_strcmp0 (text, "old.zip") != 0);
	g_free (text);

	text = nemo_archive_each_name ("notes.rar", NEMO_ARCHIVE_FORMAT_ZIP);
	check (g_strcmp0 (text, "notes.rar.zip") == 0);
	g_free (text);

	/* Nothing an archive could be named after or written to. */
	check (nemo_archive_each_name (NULL, NEMO_ARCHIVE_FORMAT_ZIP) == NULL);
	check (nemo_archive_each_name ("", NEMO_ARCHIVE_FORMAT_ZIP) == NULL);
	check (nemo_archive_each_name (".", NEMO_ARCHIVE_FORMAT_ZIP) == NULL);
	check (nemo_archive_each_name ("photos/holiday.jpg", NEMO_ARCHIVE_FORMAT_ZIP) == NULL);
}

static void
check_sizes (void)
{
	guint64 bytes = 0;

	/* The suffixes an archiver uses, and the "B" that comes with them. */
	check (nemo_archive_parse_size ("700 MB", &bytes) && bytes == 700ULL * 1024 * 1024);
	check (nemo_archive_parse_size ("700m", &bytes) && bytes == 700ULL * 1024 * 1024);
	check (nemo_archive_parse_size ("1 GB", &bytes) && bytes == 1024ULL * 1024 * 1024);
	check (nemo_archive_parse_size ("1.5g", &bytes) && bytes == 1610612736ULL);

	/* A bare number is bytes, not the last unit used. */
	check (nemo_archive_parse_size ("4096", &bytes) && bytes == 4096);

	/* Nothing usable is a refusal, not a zero - a zero would silently mean
	   "one volume" and quietly drop the split. */
	check (!nemo_archive_parse_size ("", &bytes));
	check (!nemo_archive_parse_size ("lots", &bytes));
	check (!nemo_archive_parse_size ("100 quatloos", &bytes));
	check (!nemo_archive_parse_size ("0", &bytes));
	check (!nemo_archive_parse_size ("-5m", &bytes));

	/* A size written out comes back the same way it went in. */
	{
		char *text = nemo_archive_format_size (700ULL * 1024 * 1024);

		check (g_strcmp0 (text, "700 MB") == 0);
		g_free (text);
	}
}

static void
check_backends (void)
{
	NemoArchiveOptions options;

	/* The tar family and a plain zip are libarchive's, and it is always
	   linked in - none of them can depend on an installed program. */
	nemo_archive_options_init (&options);
	check (nemo_archive_pick_backend (NEMO_ARCHIVE_FORMAT_TAR, &options) ==
	       NEMO_ARCHIVE_BACKEND_LIBARCHIVE);
	check (nemo_archive_pick_backend (NEMO_ARCHIVE_FORMAT_TAR_GZ, &options) ==
	       NEMO_ARCHIVE_BACKEND_LIBARCHIVE);
	check (nemo_archive_pick_backend (NEMO_ARCHIVE_FORMAT_ZIP, &options) ==
	       NEMO_ARCHIVE_BACKEND_LIBARCHIVE);
	check (nemo_archive_format_available (NEMO_ARCHIVE_FORMAT_TAR));

	/* An encrypted zip is still libarchive's - it writes AES itself. */
	options.password = g_strdup ("secret");
	check (nemo_archive_pick_backend (NEMO_ARCHIVE_FORMAT_ZIP, &options) ==
	       NEMO_ARCHIVE_BACKEND_LIBARCHIVE);

	/* An encrypted 7z is not: libarchive has no write-side encryption for
	   it at all, so an encrypted one has to go to 7z or come back as
	   nothing. Stated against the capability rather than against the
	   choice, because on a box that has 7z the choice would come out right
	   for the wrong reason. */
	check ((nemo_archive_backend_caps (NEMO_ARCHIVE_FORMAT_7Z,
					   NEMO_ARCHIVE_BACKEND_LIBARCHIVE) &
		NEMO_ARCHIVE_CAP_PASSWORD) == 0);
	check (nemo_archive_pick_backend (NEMO_ARCHIVE_FORMAT_7Z, &options) !=
	       NEMO_ARCHIVE_BACKEND_LIBARCHIVE);
	nemo_archive_options_clear (&options);

	/* Same for splitting, which libarchive cannot write in any format. */
	nemo_archive_options_init (&options);
	options.split_size = 700ULL * 1024 * 1024;
	check ((nemo_archive_backend_caps (NEMO_ARCHIVE_FORMAT_ZIP,
					   NEMO_ARCHIVE_BACKEND_LIBARCHIVE) &
		NEMO_ARCHIVE_CAP_SPLIT) == 0);
	check (nemo_archive_pick_backend (NEMO_ARCHIVE_FORMAT_ZIP, &options) !=
	       NEMO_ARCHIVE_BACKEND_LIBARCHIVE);
	nemo_archive_options_clear (&options);

	/* rar is never libarchive's to write, whatever the options. */
	nemo_archive_options_init (&options);
	check (nemo_archive_pick_backend (NEMO_ARCHIVE_FORMAT_RAR, &options) !=
	       NEMO_ARCHIVE_BACKEND_LIBARCHIVE);
	check (nemo_archive_backend_caps (NEMO_ARCHIVE_FORMAT_RAR,
					  NEMO_ARCHIVE_BACKEND_LIBARCHIVE) == 0);

	/* Storing links, a solid archive and a recovery record are preferences,
	   not requirements: a plain tar still gets written even with all three
	   asked for, and by libarchive, which can honour none of them. */
	options.store_links = TRUE;
	options.solid = TRUE;
	options.recovery_record = TRUE;
	check (nemo_archive_pick_backend (NEMO_ARCHIVE_FORMAT_TAR, &options) ==
	       NEMO_ARCHIVE_BACKEND_LIBARCHIVE);
	/* Same for a 7z - which writer gets it depends on what is installed,
	   but it does get written. */
	check (nemo_archive_pick_backend (NEMO_ARCHIVE_FORMAT_7Z, &options) !=
	       NEMO_ARCHIVE_BACKEND_NONE);
	nemo_archive_options_clear (&options);
}

static void
check_commands (void)
{
	NemoArchiveOptions options;
	GList *names = NULL;
	char **argv;

	names = g_list_append (names, (gpointer) "one.txt");
	names = g_list_append (names, (gpointer) "a folder");

	/* rar: every option the user asked for has to reach the command line,
	   or it is silently not honoured. */
	nemo_archive_options_init (&options);
	options.format = NEMO_ARCHIVE_FORMAT_RAR;
	options.level = NEMO_ARCHIVE_LEVEL_MAX;
	options.password = g_strdup ("secret");
	options.encrypt_names = TRUE;
	options.split_size = 100ULL * 1024 * 1024;
	options.solid = TRUE;
	options.dedupe = TRUE;
	options.recovery_record = TRUE;
	options.lock = TRUE;

	argv = nemo_archive_build_command (NEMO_ARCHIVE_BACKEND_RAR, options.format, &options,
					   "rar", "/tmp/out.rar", names);
	if (argv == NULL) {
		g_printerr ("FAIL %s:%d: no rar command line built\n", __FILE__, __LINE__);
		failures++;
		return;
	}

	check (g_strcmp0 (argv[0], "rar") == 0);
	check (g_strcmp0 (argv[1], "a") == 0);
	check (has_arg (argv, "-m5"));
	/* Encrypted names means -hp, and -p would leave the list readable. */
	check (has_arg (argv, "-hpsecret"));
	check (!has_arg (argv, "-psecret"));
	check (has_arg (argv, "-v104857600b"));
	check (has_arg (argv, "-s"));
	check (has_arg (argv, "-oi"));
	check (has_prefix_arg (argv, "-rr"));
	check (has_arg (argv, "-k"));
	/* The archive and the names come after the end-of-switches marker, so a
	   file whose name starts with a dash is a file. */
	check (has_arg (argv, "--"));
	check (has_arg (argv, "/tmp/out.rar"));
	check (has_arg (argv, "a folder"));
	g_strfreev (argv);
	nemo_archive_options_clear (&options);

	/* The options left off have to be absent, not defaulted on by the tool:
	   rar makes solid archives by default, so -s- has to be explicit. */
	nemo_archive_options_init (&options);
	options.format = NEMO_ARCHIVE_FORMAT_RAR;
	options.solid = FALSE;
	options.recovery_record = FALSE;

	argv = nemo_archive_build_command (NEMO_ARCHIVE_BACKEND_RAR, options.format, &options,
					   "rar", "/tmp/out.rar", names);
	check (has_arg (argv, "-s-"));
	check (!has_prefix_arg (argv, "-rr"));
	check (!has_arg (argv, "-k"));
	check (!has_prefix_arg (argv, "-p"));
	check (!has_prefix_arg (argv, "-hp"));
	g_strfreev (argv);
	nemo_archive_options_clear (&options);

	/* 7z: the level is on its own scale, and encrypted headers are -mhe. */
	nemo_archive_options_init (&options);
	options.format = NEMO_ARCHIVE_FORMAT_7Z;
	options.level = NEMO_ARCHIVE_LEVEL_MAX;
	options.password = g_strdup ("secret");
	options.encrypt_names = TRUE;
	options.solid = TRUE;

	argv = nemo_archive_build_command (NEMO_ARCHIVE_BACKEND_7Z, options.format, &options,
					   "7z", "/tmp/out.7z", names);
	check (has_arg (argv, "-t7z"));
	check (has_arg (argv, "-mx=9"));
	check (has_arg (argv, "-psecret"));
	check (has_arg (argv, "-mhe=on"));
	check (has_arg (argv, "-ms=on"));
	g_strfreev (argv);
	nemo_archive_options_clear (&options);

	/* Store means store all the way down: level 0 is -mx=0, not the default. */
	nemo_archive_options_init (&options);
	options.format = NEMO_ARCHIVE_FORMAT_7Z;
	options.level = NEMO_ARCHIVE_LEVEL_STORE;

	argv = nemo_archive_build_command (NEMO_ARCHIVE_BACKEND_7Z, options.format, &options,
					   "7z", "/tmp/out.7z", names);
	check (has_arg (argv, "-mx=0"));
	check (has_arg (argv, "-ms=off"));
	g_strfreev (argv);
	nemo_archive_options_clear (&options);

	/* A zip written by 7z is -tzip; the format is not read off the name. */
	nemo_archive_options_init (&options);
	options.format = NEMO_ARCHIVE_FORMAT_ZIP;
	options.split_size = 1024;

	argv = nemo_archive_build_command (NEMO_ARCHIVE_BACKEND_7Z, options.format, &options,
					   "7z", "/tmp/out.zip", names);
	check (has_arg (argv, "-tzip"));
	check (has_arg (argv, "-v1024b"));
	g_strfreev (argv);
	nemo_archive_options_clear (&options);

	/* libarchive is not a command, so there is no command line to build. */
	nemo_archive_options_init (&options);
	check (nemo_archive_build_command (NEMO_ARCHIVE_BACKEND_LIBARCHIVE, options.format,
					   &options, "x", "/tmp/out.zip", names) == NULL);
	nemo_archive_options_clear (&options);

	g_list_free (names);
}

int
main (int argc, char *argv[])
{
	check_extensions ();
	check_names ();
	check_each_names ();
	check_sizes ();
	check_backends ();
	check_commands ();

	if (failures > 0) {
		g_printerr ("%d check(s) failed\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("OK\n");
	return EXIT_SUCCESS;
}
