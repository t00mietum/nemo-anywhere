/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-archive.c - create an archive from a selection.

   Copyright (C) 2026 t00mietum.

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; version 2 of the
   License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
   Boston, MA 02110-1335, USA.
*/

#include <config.h>
#include "nemo-archive.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <glib/gi18n.h>
#include <archive.h>
#include <archive_entry.h>

#include <eel/eel-stock-dialogs.h>

#include "nemo-archive-commands.h"
#include "nemo-command-template.h"
#include "nemo-dir-enum.h"
#include "nemo-file-changes-queue.h"
#include "nemo-global-preferences.h"
#include "nemo-job-queue.h"
#include "nemo-progress-info.h"

#define READ_BUFFER_SIZE (64 * 1024)

/* Attributes one scan pass needs. id::file is what tells us we have already
   been through a directory, which is the loop guard when links are followed. */
#define SCAN_ATTRIBUTES \
	G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
	G_FILE_ATTRIBUTE_STANDARD_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_SIZE "," \
	G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK "," \
	G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET "," \
	G_FILE_ATTRIBUTE_UNIX_MODE "," \
	G_FILE_ATTRIBUTE_TIME_MODIFIED "," \
	G_FILE_ATTRIBUTE_ID_FILE

typedef struct {
	const char      *id;
	const char      *name;		/* untranslated; run through _() at use */
	const char      *extension;
	gboolean         by_libarchive;
	NemoArchiveCaps  caps_libarchive;
	gboolean         by_7z;
	NemoArchiveCaps  caps_7z;
	gboolean         by_rar;
	NemoArchiveCaps  caps_rar;
} FormatInfo;

/* What each writer can do, established against libarchive 3.8 and the switch
   lists of 7-Zip and rar. libarchive writes no rar at all, and has no write
   support for volumes, solid blocks, duplicate-as-reference or 7z encryption,
   which is the whole reason the commands are still worth reaching for. */
static const FormatInfo formats[NEMO_ARCHIVE_N_FORMATS] = {
	{ "zip", N_("ZIP"), ".zip",
	  TRUE,  NEMO_ARCHIVE_CAP_LEVEL | NEMO_ARCHIVE_CAP_PASSWORD | NEMO_ARCHIVE_CAP_STORE_LINKS,
	  TRUE,  NEMO_ARCHIVE_CAP_LEVEL | NEMO_ARCHIVE_CAP_PASSWORD | NEMO_ARCHIVE_CAP_SPLIT |
		 NEMO_ARCHIVE_CAP_STORE_LINKS,
	  FALSE, 0 },

	{ "tar", N_("TAR, uncompressed"), ".tar",
	  TRUE,  NEMO_ARCHIVE_CAP_STORE_LINKS,
	  FALSE, 0,
	  FALSE, 0 },

	{ "tar.gz", N_("TAR, gzip compressed"), ".tar.gz",
	  TRUE,  NEMO_ARCHIVE_CAP_LEVEL | NEMO_ARCHIVE_CAP_STORE_LINKS,
	  FALSE, 0,
	  FALSE, 0 },

	{ "tar.xz", N_("TAR, xz compressed"), ".tar.xz",
	  TRUE,  NEMO_ARCHIVE_CAP_LEVEL | NEMO_ARCHIVE_CAP_STORE_LINKS,
	  FALSE, 0,
	  FALSE, 0 },

	{ "7z", N_("7z"), ".7z",
	  TRUE,  NEMO_ARCHIVE_CAP_LEVEL,
	  TRUE,  NEMO_ARCHIVE_CAP_LEVEL | NEMO_ARCHIVE_CAP_PASSWORD | NEMO_ARCHIVE_CAP_ENCRYPT_NAMES |
		 NEMO_ARCHIVE_CAP_SPLIT | NEMO_ARCHIVE_CAP_SOLID | NEMO_ARCHIVE_CAP_STORE_LINKS,
	  FALSE, 0 },

	{ "rar", N_("RAR"), ".rar",
	  FALSE, 0,
	  FALSE, 0,
	  TRUE,  NEMO_ARCHIVE_CAP_LEVEL | NEMO_ARCHIVE_CAP_PASSWORD | NEMO_ARCHIVE_CAP_ENCRYPT_NAMES |
		 NEMO_ARCHIVE_CAP_SPLIT | NEMO_ARCHIVE_CAP_SOLID | NEMO_ARCHIVE_CAP_DEDUPE |
		 NEMO_ARCHIVE_CAP_STORE_LINKS | NEMO_ARCHIVE_CAP_RECOVERY | NEMO_ARCHIVE_CAP_LOCK },
};

/* Every suffix we recognize as ours, longest first so ".tar.gz" is not read as
   ".gz". Used to swap the extension when the format changes. */
static const char * const known_extensions[] = {
	".tar.gz", ".tar.xz", ".tar.bz2", ".tgz", ".txz", ".tbz2",
	".zip", ".tar", ".7z", ".rar", NULL
};

typedef struct {
	GFile     *file;
	char      *rel_path;	/* utf-8, '/'-separated, relative to the base dir */
	GFileInfo *info;
	gboolean   as_link;
} ArchiveEntry;

/* One archive to write, and what goes in it. A plain compress has one of
   these; compressing a selection separately has one per item. */
typedef struct {
	GList *sources;		/* GFile *, owned */
	GFile *destination;	/* owned */
} ArchiveUnit;

typedef struct {
	GtkWindow          *parent_window;
	NemoProgressInfo   *progress;
	GCancellable       *cancellable;
	GIOSchedulerJob    *io_job;

	GList              *units;		/* ArchiveUnit *, in order */
	guint               unit_count;
	guint               unit_index;		/* which one is being written */
	GFile              *result_file;	/* what the callback is handed */

	/* The unit in hand. sources and destination are borrowed from it. */
	GList              *sources;		/* GFile * */
	GFile              *destination;
	GFile              *base_dir;

	NemoArchiveOptions  options;
	NemoArchiveBackend  backend;

	GList              *entries;		/* ArchiveEntry * */
	guint64             total_bytes;
	guint64             done_bytes;
	guint               file_count;

	char               *error_message;
	char               *error_details;
	gboolean            success;

	NemoArchiveCallback done_callback;
	gpointer            done_callback_data;
} ArchiveJob;

/* The GOutputStream libarchive writes through. Going out via GIO rather than a
   filename keeps one code path for local and remote destinations, and sidesteps
   the byte-versus-wide filename split on Windows. */
typedef struct {
	GOutputStream *stream;
	GCancellable  *cancellable;
	GError        *error;
} StreamSink;

static gboolean
format_is_valid (NemoArchiveFormat format)
{
	return format >= 0 && format < NEMO_ARCHIVE_N_FORMATS;
}

const char *
nemo_archive_format_id (NemoArchiveFormat format)
{
	g_return_val_if_fail (format_is_valid (format), "zip");

	return formats[format].id;
}

const char *
nemo_archive_format_name (NemoArchiveFormat format)
{
	g_return_val_if_fail (format_is_valid (format), "");

	return _(formats[format].name);
}

const char *
nemo_archive_format_extension (NemoArchiveFormat format)
{
	g_return_val_if_fail (format_is_valid (format), ".zip");

	return formats[format].extension;
}

gboolean
nemo_archive_format_from_id (const char        *id,
			     NemoArchiveFormat *format)
{
	int i;

	if (id == NULL) {
		return FALSE;
	}

	for (i = 0; i < NEMO_ARCHIVE_N_FORMATS; i++) {
		if (g_strcmp0 (id, formats[i].id) == 0) {
			if (format != NULL) {
				*format = (NemoArchiveFormat) i;
			}
			return TRUE;
		}
	}

	return FALSE;
}

/* PATH does not change under a running app, so one probe per backend is enough.
   Both the dialog on the main thread and the job on its own can ask, hence the
   lock. */
static GMutex   probe_lock;
static char    *probed_program[4];
static gboolean probed[4];

char *
nemo_archive_find_command (const char * const *names,
			   const char         *win_subdir)
{
	int i;
	char *found;

	for (i = 0; names[i] != NULL; i++) {
		found = g_find_program_in_path (names[i]);
		if (found != NULL) {
			return found;
		}
	}

#ifdef G_OS_WIN32
	/* Neither installer puts itself on PATH, and both land under Program
	   Files, so look there before giving up. */
	{
		static const char * const roots[] = { "ProgramW6432", "ProgramFiles", "ProgramFiles(x86)", NULL };
		int r;

		for (r = 0; roots[r] != NULL; r++) {
			const char *root = g_getenv (roots[r]);

			if (root == NULL) {
				continue;
			}

			for (i = 0; names[i] != NULL; i++) {
				char *exe = g_strconcat (names[i], ".exe", NULL);
				char *path = g_build_filename (root, win_subdir, exe, NULL);

				g_free (exe);

				if (g_file_test (path, G_FILE_TEST_IS_EXECUTABLE)) {
					return path;
				}
				g_free (path);
			}
		}
	}
#else
	(void) win_subdir;
#endif

	return NULL;
}

static const char *
backend_program (NemoArchiveBackend backend)
{
	static const char * const seven_zip_names[] = { "7z", "7zz", "7za", NULL };
	static const char * const rar_names[] = { "rar", NULL };

	if (backend != NEMO_ARCHIVE_BACKEND_7Z && backend != NEMO_ARCHIVE_BACKEND_RAR) {
		return NULL;
	}

	g_mutex_lock (&probe_lock);

	if (!probed[backend]) {
		if (backend == NEMO_ARCHIVE_BACKEND_7Z) {
			probed_program[backend] = nemo_archive_find_command (seven_zip_names, "7-Zip");
		} else {
			probed_program[backend] = nemo_archive_find_command (rar_names, "WinRAR");
		}
		probed[backend] = TRUE;
	}

	g_mutex_unlock (&probe_lock);

	return probed_program[backend];
}

gboolean
nemo_archive_backend_present (NemoArchiveBackend backend)
{
	if (backend == NEMO_ARCHIVE_BACKEND_LIBARCHIVE) {
		return TRUE;
	}

	return backend_program (backend) != NULL;
}

NemoArchiveCaps
nemo_archive_backend_caps (NemoArchiveFormat  format,
			   NemoArchiveBackend backend)
{
	const FormatInfo *info;

	g_return_val_if_fail (format_is_valid (format), 0);

	info = &formats[format];

	switch (backend) {
	case NEMO_ARCHIVE_BACKEND_LIBARCHIVE:
		return info->by_libarchive ? info->caps_libarchive : 0;
	case NEMO_ARCHIVE_BACKEND_7Z:
		return info->by_7z ? info->caps_7z : 0;
	case NEMO_ARCHIVE_BACKEND_RAR:
		return info->by_rar ? info->caps_rar : 0;
	case NEMO_ARCHIVE_BACKEND_NONE:
	default:
		return 0;
	}
}

static gboolean
backend_writes_format (NemoArchiveFormat  format,
		       NemoArchiveBackend backend)
{
	const FormatInfo *info = &formats[format];

	switch (backend) {
	case NEMO_ARCHIVE_BACKEND_LIBARCHIVE:
		return info->by_libarchive;
	case NEMO_ARCHIVE_BACKEND_7Z:
		return info->by_7z;
	case NEMO_ARCHIVE_BACKEND_RAR:
		return info->by_rar;
	case NEMO_ARCHIVE_BACKEND_NONE:
	default:
		return FALSE;
	}
}

/* Preference order: libarchive first, since it needs nothing installed and can
   report real progress; a command only when it is the one that can do the job. */
static const NemoArchiveBackend backend_order[] = {
	NEMO_ARCHIVE_BACKEND_LIBARCHIVE,
	NEMO_ARCHIVE_BACKEND_7Z,
	NEMO_ARCHIVE_BACKEND_RAR
};

gboolean
nemo_archive_format_available (NemoArchiveFormat format)
{
	guint i;

	g_return_val_if_fail (format_is_valid (format), FALSE);

	for (i = 0; i < G_N_ELEMENTS (backend_order); i++) {
		if (backend_writes_format (format, backend_order[i]) &&
		    nemo_archive_backend_present (backend_order[i])) {
			return TRUE;
		}
	}

	return FALSE;
}

NemoArchiveCaps
nemo_archive_format_caps (NemoArchiveFormat format)
{
	NemoArchiveCaps caps = 0;
	guint i;

	g_return_val_if_fail (format_is_valid (format), 0);

	for (i = 0; i < G_N_ELEMENTS (backend_order); i++) {
		if (nemo_archive_backend_present (backend_order[i])) {
			caps |= nemo_archive_backend_caps (format, backend_order[i]);
		}
	}

	return caps;
}

/* Two kinds of option. Encryption and splitting change whether the archive is
   what was asked for at all, so a backend that cannot do them is not a
   candidate - writing a readable archive when one was asked to be locked is the
   worst possible outcome. The rest is tuning: preferred where it can be had,
   dropped where it cannot, rather than failing the whole job. */
static NemoArchiveCaps
required_caps (const NemoArchiveOptions *options)
{
	NemoArchiveCaps caps = 0;

	if (options->password != NULL && options->password[0] != '\0') {
		caps |= NEMO_ARCHIVE_CAP_PASSWORD;

		if (options->encrypt_names) {
			caps |= NEMO_ARCHIVE_CAP_ENCRYPT_NAMES;
		}
	}
	if (options->split_size > 0) {
		caps |= NEMO_ARCHIVE_CAP_SPLIT;
	}

	return caps;
}

static NemoArchiveCaps
preferred_caps (const NemoArchiveOptions *options)
{
	NemoArchiveCaps caps = 0;

	if (options->solid) {
		caps |= NEMO_ARCHIVE_CAP_SOLID;
	}
	if (options->dedupe) {
		caps |= NEMO_ARCHIVE_CAP_DEDUPE;
	}
	if (options->store_links) {
		caps |= NEMO_ARCHIVE_CAP_STORE_LINKS;
	}
	if (options->recovery_record) {
		caps |= NEMO_ARCHIVE_CAP_RECOVERY;
	}
	if (options->lock) {
		caps |= NEMO_ARCHIVE_CAP_LOCK;
	}

	return caps;
}

NemoArchiveBackend
nemo_archive_pick_backend (NemoArchiveFormat         format,
			   const NemoArchiveOptions *options)
{
	NemoArchiveCaps needed;
	NemoArchiveCaps preferred;
	int pass;
	guint i;

	g_return_val_if_fail (format_is_valid (format), NEMO_ARCHIVE_BACKEND_NONE);
	g_return_val_if_fail (options != NULL, NEMO_ARCHIVE_BACKEND_NONE);

	needed = required_caps (options);

	/* A preference nothing can honour for this format is dropped before
	   matching, so an option left on by default - a recovery record, which
	   only rar has - does not steer a tar away from the writer that suits
	   the preferences that ARE available. */
	preferred = preferred_caps (options) & nemo_archive_format_caps (format);

	for (pass = 0; pass < 2; pass++) {
		NemoArchiveCaps target = (pass == 0) ? (needed | preferred) : needed;

		for (i = 0; i < G_N_ELEMENTS (backend_order); i++) {
			NemoArchiveBackend backend = backend_order[i];

			if (!nemo_archive_backend_present (backend) ||
			    !backend_writes_format (format, backend)) {
				continue;
			}
			if ((nemo_archive_backend_caps (format, backend) & target) == target) {
				return backend;
			}
		}
	}

	return NEMO_ARCHIVE_BACKEND_NONE;
}

void
nemo_archive_options_init (NemoArchiveOptions *options)
{
	g_return_if_fail (options != NULL);

	memset (options, 0, sizeof (NemoArchiveOptions));
	options->format = NEMO_ARCHIVE_FORMAT_ZIP;
	options->level = NEMO_ARCHIVE_LEVEL_DEFAULT;
	options->store_links = TRUE;
	options->follow_link_dirs = FALSE;
	options->recovery_record = TRUE;
}

void
nemo_archive_options_clear (NemoArchiveOptions *options)
{
	g_return_if_fail (options != NULL);

	g_clear_pointer (&options->password, g_free);
}

void
nemo_archive_options_copy (const NemoArchiveOptions *source,
			   NemoArchiveOptions       *dest)
{
	g_return_if_fail (source != NULL);
	g_return_if_fail (dest != NULL);

	*dest = *source;
	dest->password = g_strdup (source->password);
}

char *
nemo_archive_strip_extension (const char *name)
{
	int i;
	gsize len;

	g_return_val_if_fail (name != NULL, NULL);

	len = strlen (name);

	for (i = 0; known_extensions[i] != NULL; i++) {
		gsize ext_len = strlen (known_extensions[i]);

		/* A suffix only, and never the whole name - ".zip" on its own is
		   a file called .zip, not an empty name. */
		if (len > ext_len &&
		    g_ascii_strcasecmp (name + len - ext_len, known_extensions[i]) == 0) {
			return g_strndup (name, len - ext_len);
		}
	}

	return g_strdup (name);
}

char *
nemo_archive_apply_extension (const char        *name,
			      NemoArchiveFormat  format)
{
	char *base;
	char *result;

	g_return_val_if_fail (name != NULL, NULL);
	g_return_val_if_fail (format_is_valid (format), NULL);

	base = nemo_archive_strip_extension (name);
	result = g_strconcat (base, formats[format].extension, NULL);
	g_free (base);

	return result;
}

/* One item names the archive after itself, whether it is a file or a folder.
   Several only borrow the folder's name when they are the whole folder; a part
   of one has no name a user would agree with, so none is offered. */
char *
nemo_archive_suggest_name (GList             *files,
			   gboolean           whole_folder,
			   NemoArchiveFormat  format)
{
	char *base = NULL;
	char *result;

	g_return_val_if_fail (format_is_valid (format), NULL);

	if (files == NULL) {
		return NULL;
	}

	if (files->next == NULL) {
		base = g_file_get_basename (G_FILE (files->data));
	} else if (whole_folder) {
		GFile *parent = g_file_get_parent (G_FILE (files->data));

		if (parent != NULL) {
			base = g_file_get_basename (parent);
			g_object_unref (parent);
		}
	}

	/* A root has no basename worth using, and neither does anything that
	   came back as "." or a bare separator. */
	if (base == NULL || base[0] == '\0' ||
	    g_strcmp0 (base, ".") == 0 || g_strcmp0 (base, G_DIR_SEPARATOR_S) == 0) {
		g_free (base);
		return NULL;
	}

	result = nemo_archive_apply_extension (base, format);
	g_free (base);

	return result;
}

/* Compressing a selection separately names each archive after the item it came
   from, extension and all: "notes.rar" becomes "notes.rar.zip". The name field
   strips a suffix it recognizes, which cannot be done here - re-zipping a zip
   would then write the archive over the file being read. */
char *
nemo_archive_each_name (const char        *item_name,
			NemoArchiveFormat  format)
{
	g_return_val_if_fail (format_is_valid (format), NULL);

	if (item_name == NULL || item_name[0] == '\0' ||
	    g_strcmp0 (item_name, ".") == 0 ||
	    strchr (item_name, '/') != NULL ||
	    strchr (item_name, G_DIR_SEPARATOR) != NULL) {
		return NULL;
	}

	return g_strconcat (item_name, formats[format].extension, NULL);
}

/* Sizes are read the way archivers write them: k, m, g and t are 1024-based,
   and a bare number is bytes. */
gboolean
nemo_archive_parse_size (const char *text,
			 guint64    *bytes)
{
	const char *p;
	char *end = NULL;
	double value;
	guint64 multiplier;

	if (text == NULL) {
		return FALSE;
	}

	p = text;
	while (g_ascii_isspace (*p)) {
		p++;
	}
	if (*p == '\0') {
		return FALSE;
	}

	value = g_ascii_strtod (p, &end);
	if (end == p || value <= 0.0) {
		return FALSE;
	}

	while (g_ascii_isspace (*end)) {
		end++;
	}

	switch (g_ascii_tolower (*end)) {
	case '\0':
		multiplier = 1;
		break;
	case 'b':
		multiplier = 1;
		end++;
		break;
	case 'k':
		multiplier = G_GUINT64_CONSTANT (1024);
		end++;
		break;
	case 'm':
		multiplier = G_GUINT64_CONSTANT (1024) * 1024;
		end++;
		break;
	case 'g':
		multiplier = G_GUINT64_CONSTANT (1024) * 1024 * 1024;
		end++;
		break;
	case 't':
		multiplier = G_GUINT64_CONSTANT (1024) * 1024 * 1024 * 1024;
		end++;
		break;
	default:
		return FALSE;
	}

	/* A trailing "b" as in "MB" names the same unit; it is not another factor. */
	if (multiplier > 1 && g_ascii_tolower (*end) == 'b') {
		end++;
	}
	while (g_ascii_isspace (*end)) {
		end++;
	}
	if (*end != '\0') {
		return FALSE;
	}

	if (bytes != NULL) {
		*bytes = (guint64) (value * (double) multiplier);
	}

	return TRUE;
}

char *
nemo_archive_format_size (guint64 bytes)
{
	const guint64 kb = G_GUINT64_CONSTANT (1024);
	const guint64 mb = kb * 1024;
	const guint64 gb = mb * 1024;

	if (bytes == 0) {
		return g_strdup ("");
	}
	if (bytes >= gb && bytes % gb == 0) {
		return g_strdup_printf ("%llu GB", (unsigned long long) (bytes / gb));
	}
	if (bytes >= mb && bytes % mb == 0) {
		return g_strdup_printf ("%llu MB", (unsigned long long) (bytes / mb));
	}
	if (bytes >= kb && bytes % kb == 0) {
		return g_strdup_printf ("%llu KB", (unsigned long long) (bytes / kb));
	}

	return g_strdup_printf ("%llu", (unsigned long long) bytes);
}

static void
archive_unit_free (ArchiveUnit *unit)
{
	g_list_free_full (unit->sources, g_object_unref);
	g_clear_object (&unit->destination);
	g_free (unit);
}

static void
archive_entry_free_full (ArchiveEntry *entry)
{
	g_clear_object (&entry->file);
	g_clear_object (&entry->info);
	g_free (entry->rel_path);
	g_free (entry);
}

static gboolean
job_aborted (ArchiveJob *job)
{
	return g_cancellable_is_cancelled (job->cancellable);
}

static gboolean
job_stores_links (ArchiveJob *job)
{
	return job->options.store_links &&
	       (nemo_archive_backend_caps (job->options.format, job->backend) &
		NEMO_ARCHIVE_CAP_STORE_LINKS) != 0;
}

static void
add_entry (ArchiveJob *job,
	   GFile      *file,
	   const char *rel_path,
	   GFileInfo  *info,
	   gboolean    as_link)
{
	ArchiveEntry *entry = g_new0 (ArchiveEntry, 1);

	entry->file = g_object_ref (file);
	entry->rel_path = g_strdup (rel_path);
	entry->info = g_object_ref (info);
	entry->as_link = as_link;

	job->entries = g_list_prepend (job->entries, entry);

	if (!as_link && g_file_info_get_file_type (info) == G_FILE_TYPE_REGULAR) {
		job->total_bytes += (guint64) g_file_info_get_size (info);
		job->file_count++;
	}
}

static void scan_item (ArchiveJob *job, GFile *file, const char *rel_path,
		       GFileInfo *info, GHashTable *seen);

static void
scan_directory (ArchiveJob *job,
		GFile      *dir,
		const char *rel_path,
		GHashTable *seen)
{
	GFileEnumerator *children;
	GFileInfo *child_info;

	children = nemo_enumerate_children (dir, SCAN_ATTRIBUTES,
					      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					      job->cancellable, NULL);
	if (children == NULL) {
		return;
	}

	while (!job_aborted (job)) {
		GFile *child;
		char *child_rel;

		child_info = g_file_enumerator_next_file (children, job->cancellable, NULL);
		if (child_info == NULL) {
			break;
		}

		child = g_file_get_child (dir, g_file_info_get_name (child_info));
		child_rel = g_strconcat (rel_path, "/", g_file_info_get_name (child_info), NULL);

		scan_item (job, child, child_rel, child_info, seen);

		g_free (child_rel);
		g_object_unref (child);
		g_object_unref (child_info);
	}

	g_file_enumerator_close (children, NULL, NULL);
	g_object_unref (children);
}

/* info is what a no-follow query said about this name. A link is either stored
   as a link or followed, and following into a folder is the case that can loop,
   which is what the seen set is for. */
static void
scan_item (ArchiveJob *job,
	   GFile      *file,
	   const char *rel_path,
	   GFileInfo  *info,
	   GHashTable *seen)
{
	GFileInfo *effective;
	GFileType type;

	if (job_aborted (job)) {
		return;
	}

	if (g_file_info_get_is_symlink (info)) {
		if (job_stores_links (job) &&
		    g_file_info_get_symlink_target (info) != NULL) {
			add_entry (job, file, rel_path, info, TRUE);
			return;
		}

		effective = g_file_query_info (file, SCAN_ATTRIBUTES,
					       G_FILE_QUERY_INFO_NONE,
					       job->cancellable, NULL);
		if (effective == NULL) {
			return;		/* dangling - there is nothing to store */
		}
		if (g_file_info_get_file_type (effective) == G_FILE_TYPE_DIRECTORY &&
		    !job->options.follow_link_dirs) {
			g_object_unref (effective);
			return;
		}
	} else {
		effective = g_object_ref (info);
	}

	type = g_file_info_get_file_type (effective);

	if (type == G_FILE_TYPE_DIRECTORY) {
		const char *id = g_file_info_get_attribute_string (effective, G_FILE_ATTRIBUTE_ID_FILE);

		if (id != NULL) {
			if (g_hash_table_contains (seen, id)) {
				g_object_unref (effective);
				return;
			}
			g_hash_table_add (seen, g_strdup (id));
		}

		add_entry (job, file, rel_path, effective, FALSE);
		scan_directory (job, file, rel_path, seen);
	} else if (type == G_FILE_TYPE_REGULAR) {
		add_entry (job, file, rel_path, effective, FALSE);
	}
	/* Anything else - a socket, a device node - means nothing in an archive. */

	g_object_unref (effective);
}

static void
scan_sources (ArchiveJob *job)
{
	GHashTable *seen;
	GList *l;

	seen = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	nemo_progress_info_set_status (job->progress, _("Preparing to compress"));

	for (l = job->sources; l != NULL && !job_aborted (job); l = l->next) {
		GFile *file = G_FILE (l->data);
		GFileInfo *info;
		char *name;

		info = g_file_query_info (file, SCAN_ATTRIBUTES,
					  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
					  job->cancellable, NULL);
		if (info == NULL) {
			continue;
		}

		name = g_file_get_basename (file);
		scan_item (job, file, name, info, seen);
		nemo_progress_info_pulse_progress (job->progress);

		g_free (name);
		g_object_unref (info);
	}

	job->entries = g_list_reverse (job->entries);

	g_hash_table_destroy (seen);
}

static la_ssize_t
sink_write (struct archive *a,
	    void           *client_data,
	    const void     *buffer,
	    size_t          length)
{
	StreamSink *sink = client_data;
	gsize written = 0;

	if (!g_output_stream_write_all (sink->stream, buffer, length, &written,
					sink->cancellable, &sink->error)) {
		archive_set_error (a, EIO, "%s",
				   sink->error != NULL ? sink->error->message : "write failed");
		return -1;
	}

	return (la_ssize_t) written;
}

/* The stream is closed by the caller, which is where a flush failure can still
   be turned into a message. */
static int
sink_close (struct archive *a,
	    void           *client_data)
{
	(void) a;
	(void) client_data;

	return ARCHIVE_OK;
}

/* Names go in as wide characters on Windows, where the byte-oriented setters
   would run the name through the ANSI code page and mangle anything outside it. */
static void
entry_set_paths (struct archive_entry *entry,
		 const char           *rel_path,
		 const char           *link_target)
{
#ifdef G_OS_WIN32
	gunichar2 *wide;

	wide = g_utf8_to_utf16 (rel_path, -1, NULL, NULL, NULL);
	if (wide != NULL) {
		archive_entry_copy_pathname_w (entry, (const wchar_t *) wide);
		g_free (wide);
	} else {
		archive_entry_set_pathname (entry, rel_path);
	}

	if (link_target != NULL) {
		wide = g_utf8_to_utf16 (link_target, -1, NULL, NULL, NULL);
		if (wide != NULL) {
			archive_entry_copy_symlink_w (entry, (const wchar_t *) wide);
			g_free (wide);
		} else {
			archive_entry_set_symlink (entry, link_target);
		}
	}
#else
	archive_entry_set_pathname (entry, rel_path);

	if (link_target != NULL) {
		archive_entry_set_symlink (entry, link_target);
	}
#endif
}

static gboolean
configure_writer (struct archive  *a,
		  ArchiveJob      *job,
		  GError         **error)
{
	const NemoArchiveOptions *options = &job->options;
	int level = CLAMP (options->level, 0, NEMO_ARCHIVE_LEVEL_MAX);
	gboolean has_password = options->password != NULL && options->password[0] != '\0';
	char *opts = NULL;

	/* Blocking is a tar convention: libarchive pads the output to 10240 bytes
	   by default, which a tar reader expects and every other format reads as
	   junk after the end of the archive. */
	if (options->format == NEMO_ARCHIVE_FORMAT_ZIP ||
	    options->format == NEMO_ARCHIVE_FORMAT_7Z) {
		archive_write_set_bytes_per_block (a, 0);
	}

	switch (options->format) {
	case NEMO_ARCHIVE_FORMAT_ZIP:
		archive_write_set_format_zip (a);
		if (level == NEMO_ARCHIVE_LEVEL_STORE) {
			opts = g_strdup ("zip:compression=store");
		} else {
			opts = g_strdup_printf ("zip:compression=deflate,zip:compression-level=%d", level);
		}
		break;
	case NEMO_ARCHIVE_FORMAT_TAR:
		archive_write_set_format_pax_restricted (a);
		break;
	case NEMO_ARCHIVE_FORMAT_TAR_GZ:
		archive_write_set_format_pax_restricted (a);
		archive_write_add_filter_gzip (a);
		opts = g_strdup_printf ("gzip:compression-level=%d", level);
		break;
	case NEMO_ARCHIVE_FORMAT_TAR_XZ:
		archive_write_set_format_pax_restricted (a);
		archive_write_add_filter_xz (a);
		/* The only built-in format that can be spread over cores. Deflate and
		   libarchive's own lzma2 have no such option, so they are left alone
		   rather than handed one they would refuse. */
		opts = g_strdup_printf ("xz:compression-level=%d,xz:threads=%d",
					level, nemo_global_preferences_get_cpu_thread_count ());
		break;
	case NEMO_ARCHIVE_FORMAT_7Z:
		archive_write_set_format_7zip (a);
		if (level == NEMO_ARCHIVE_LEVEL_STORE) {
			opts = g_strdup ("7zip:compression=copy");
		} else {
			opts = g_strdup_printf ("7zip:compression=lzma2,7zip:compression-level=%d", level);
		}
		break;
	default:
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
			     _("This archive format cannot be written here."));
		return FALSE;
	}

	if (opts != NULL) {
		/* Tuning only - an option a build of libarchive does not know about
		   costs compression, not correctness. */
		if (archive_write_set_options (a, opts) != ARCHIVE_OK) {
			g_warning ("archive: %s not accepted: %s", opts, archive_error_string (a));
		}
		g_free (opts);
	}

	if (has_password) {
		/* Encryption is not tuning: silently writing a readable archive
		   when one was asked to be locked would be the worst outcome. */
		if (options->format != NEMO_ARCHIVE_FORMAT_ZIP ||
		    archive_write_set_options (a, "zip:encryption=aes256") != ARCHIVE_OK ||
		    archive_write_set_passphrase (a, options->password) != ARCHIVE_OK) {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
				     _("This archive format cannot be encrypted here."));
			return FALSE;
		}
	}

	return TRUE;
}

static gboolean
write_entry_data (ArchiveJob      *job,
		  struct archive  *a,
		  ArchiveEntry    *entry,
		  GError         **error)
{
	GFileInputStream *in;
	char buffer[READ_BUFFER_SIZE];
	gssize count;
	gboolean ok = TRUE;

	in = g_file_read (entry->file, job->cancellable, error);
	if (in == NULL) {
		return FALSE;
	}

	while (TRUE) {
		count = g_input_stream_read (G_INPUT_STREAM (in), buffer, sizeof (buffer),
					     job->cancellable, error);
		if (count <= 0) {
			ok = (count == 0);
			break;
		}

		if (archive_write_data (a, buffer, (size_t) count) < 0) {
			g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s", archive_error_string (a));
			ok = FALSE;
			break;
		}

		job->done_bytes += (guint64) count;
		nemo_progress_info_set_progress (job->progress,
						 (double) job->done_bytes,
						 (double) job->total_bytes);
	}

	g_input_stream_close (G_INPUT_STREAM (in), NULL, NULL);
	g_object_unref (in);

	return ok;
}

static gboolean
write_entry (ArchiveJob      *job,
	     struct archive  *a,
	     ArchiveEntry    *entry,
	     GError         **error)
{
	struct archive_entry *ae;
	GFileType type;
	guint32 mode;
	gboolean ok = TRUE;

	ae = archive_entry_new ();
	type = g_file_info_get_file_type (entry->info);
	mode = g_file_info_get_attribute_uint32 (entry->info, G_FILE_ATTRIBUTE_UNIX_MODE) & 0777;

	entry_set_paths (ae, entry->rel_path,
			 entry->as_link ? g_file_info_get_symlink_target (entry->info) : NULL);

	if (entry->as_link) {
		archive_entry_set_filetype (ae, AE_IFLNK);
		archive_entry_set_size (ae, 0);
		if (mode == 0) {
			mode = 0777;
		}
	} else if (type == G_FILE_TYPE_DIRECTORY) {
		archive_entry_set_filetype (ae, AE_IFDIR);
		archive_entry_set_size (ae, 0);
		if (mode == 0) {
			mode = 0755;
		}
	} else {
		archive_entry_set_filetype (ae, AE_IFREG);
		archive_entry_set_size (ae, g_file_info_get_size (entry->info));
		/* Windows has no unix mode to read, so give the entry the one a
		   reader on a unix box would expect. */
		if (mode == 0) {
			mode = 0644;
		}
	}

	archive_entry_set_perm (ae, mode);

	if (g_file_info_has_attribute (entry->info, G_FILE_ATTRIBUTE_TIME_MODIFIED)) {
		archive_entry_set_mtime (ae,
					 (time_t) g_file_info_get_attribute_uint64 (entry->info,
										   G_FILE_ATTRIBUTE_TIME_MODIFIED),
					 0);
	}

	if (archive_write_header (a, ae) != ARCHIVE_OK) {
		g_set_error (error, G_IO_ERROR, G_IO_ERROR_FAILED, "%s", archive_error_string (a));
		ok = FALSE;
	} else if (!entry->as_link && type == G_FILE_TYPE_REGULAR) {
		ok = write_entry_data (job, a, entry, error);
	}

	archive_entry_free (ae);

	return ok;
}

/* The first failure is the one worth showing; anything after it is fallout. */
static void
job_fail (ArchiveJob *job,
	  const char *message,
	  const char *details)
{
	if (job->error_message != NULL) {
		return;
	}

	job->error_message = g_strdup (message);
	job->error_details = g_strdup (details);
}

static void
job_fail_from_error (ArchiveJob *job,
		     GError     *error)
{
	if (error != NULL && g_error_matches (error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
		return;
	}

	job_fail (job, _("The archive could not be created."),
		  error != NULL ? error->message : NULL);
}

/* With several archives to write, which one is in hand says more than how much
   is in it, so it takes the status line and the per-archive wording is dropped.
   Takes ownership of that wording either way. */
static void
set_unit_status (ArchiveJob *job,
		 char       *one_archive_form)
{
	char *name;

	if (job->unit_count < 2) {
		nemo_progress_info_take_status (job->progress, one_archive_form);
		return;
	}

	g_free (one_archive_form);

	name = g_file_get_basename (job->destination);
	nemo_progress_info_take_status (job->progress,
					g_strdup_printf (_("Compressing %s (%d of %d)"),
							 name, job->unit_index + 1, job->unit_count));
	g_free (name);
}

static gboolean
run_libarchive (ArchiveJob *job)
{
	struct archive *a;
	StreamSink sink;
	GFileOutputStream *out;
	GError *error = NULL;
	GList *l;
	gboolean ok = TRUE;

	scan_sources (job);

	if (job_aborted (job)) {
		return FALSE;
	}
	if (job->entries == NULL) {
		job_fail (job, _("The archive could not be created."),
			  _("Nothing in the selection could be read."));
		return FALSE;
	}

	out = g_file_replace (job->destination, NULL, FALSE,
			      G_FILE_CREATE_REPLACE_DESTINATION, job->cancellable, &error);
	if (out == NULL) {
		job_fail_from_error (job, error);
		g_clear_error (&error);
		return FALSE;
	}

	sink.stream = G_OUTPUT_STREAM (out);
	sink.cancellable = job->cancellable;
	sink.error = NULL;

	a = archive_write_new ();

	if (!configure_writer (a, job, &error)) {
		job_fail (job, _("The archive could not be created."),
			  error != NULL ? error->message : NULL);
		g_clear_error (&error);
		ok = FALSE;
	} else if (archive_write_open2 (a, &sink, NULL, sink_write, sink_close, NULL) != ARCHIVE_OK) {
		job_fail (job, _("The archive could not be created."), archive_error_string (a));
		ok = FALSE;
	}

	if (ok) {
		set_unit_status (job, g_strdup_printf (ngettext ("Compressing %'d file",
								 "Compressing %'d files",
								 job->file_count),
						       job->file_count));
	}

	for (l = job->entries; ok && l != NULL; l = l->next) {
		ArchiveEntry *entry = l->data;

		if (job_aborted (job)) {
			ok = FALSE;
			break;
		}

		nemo_progress_info_set_details (job->progress, entry->rel_path);

		ok = write_entry (job, a, entry, &error);
		if (!ok) {
			job_fail_from_error (job, error);
			g_clear_error (&error);
		}
	}

	if (ok && archive_write_close (a) != ARCHIVE_OK) {
		job_fail (job, _("The archive could not be created."), archive_error_string (a));
		ok = FALSE;
	}

	archive_write_free (a);

	if (!g_output_stream_close (G_OUTPUT_STREAM (out), NULL, &error) && ok) {
		job_fail_from_error (job, error);
		ok = FALSE;
	}
	g_clear_error (&error);
	g_object_unref (out);

	if (sink.error != NULL) {
		job_fail_from_error (job, sink.error);
		g_clear_error (&sink.error);
		ok = FALSE;
	}

	return ok;
}

/* 7-Zip takes 0,1,3,5,7,9; rar takes 0-5. Both are mapped from the 0-9 the
   dialog offers so the two ends of the scale mean the same thing everywhere. */
static int
seven_zip_level (int level)
{
	static const int steps[] = { 0, 1, 1, 3, 3, 5, 5, 7, 7, 9 };

	return steps[CLAMP (level, 0, NEMO_ARCHIVE_LEVEL_MAX)];
}

static int
rar_level (int level)
{
	static const int steps[] = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 5 };

	return steps[CLAMP (level, 0, NEMO_ARCHIVE_LEVEL_MAX)];
}

static void
free_values (char **values)
{
	int i;

	for (i = 0; values[i] != NULL; i++) {
		g_free (values[i]);
	}
}

char **
nemo_archive_build_command (NemoArchiveBackend        backend,
			    NemoArchiveFormat         format,
			    const NemoArchiveOptions *options,
			    const char               *program,
			    const char               *archive_path,
			    GList                    *names)
{
	/* One slot per switch a token stands for, plus the terminator. Only the
	   password ever needs two, and only for 7-Zip. */
	char *program_v[2]  = { NULL, NULL };
	char *format_v[2]   = { NULL, NULL };
	char *level_v[2]    = { NULL, NULL };
	char *threads_v[2]  = { NULL, NULL };
	char *password_v[3] = { NULL, NULL, NULL };
	char *split_v[2]    = { NULL, NULL };
	char *solid_v[2]    = { NULL, NULL };
	char *dedupe_v[2]   = { NULL, NULL };
	char *recovery_v[2] = { NULL, NULL };
	char *lock_v[2]     = { NULL, NULL };
	char *links_v[2]    = { NULL, NULL };
	char *archive_v[2]  = { NULL, NULL };
	GPtrArray *sources;
	const char *key, *fallback;
	GError *error = NULL;
	char **argv;
	char *text;
	GList *l;
	gboolean has_password;
	int level;
	int threads;

	g_return_val_if_fail (options != NULL, NULL);
	g_return_val_if_fail (program != NULL, NULL);
	g_return_val_if_fail (archive_path != NULL, NULL);

	has_password = options->password != NULL && options->password[0] != '\0';
	level = CLAMP (options->level, 0, NEMO_ARCHIVE_LEVEL_MAX);
	threads = nemo_global_preferences_get_cpu_thread_count ();

	if (backend == NEMO_ARCHIVE_BACKEND_7Z) {
		key = NEMO_ARCHIVE_COMMAND_KEY_7Z;
		fallback = NEMO_ARCHIVE_COMMAND_7Z_DEFAULT;

		format_v[0] = g_strdup (format == NEMO_ARCHIVE_FORMAT_7Z ? "-t7z" : "-tzip");
		level_v[0] = g_strdup_printf ("-mx=%d", seven_zip_level (level));
		threads_v[0] = g_strdup_printf ("-mmt=%d", threads);

		if (has_password) {
			/* A password on a command line is readable in the process
			   list; there is no other way to hand it to either tool.
			   It is a value rather than part of the line, so it never
			   reaches the config file. */
			password_v[0] = g_strconcat ("-p", options->password, NULL);

			if (format == NEMO_ARCHIVE_FORMAT_ZIP) {
				password_v[1] = g_strdup ("-mem=AES256");
			} else if (options->encrypt_names) {
				password_v[1] = g_strdup ("-mhe=on");
			}
		}
		if (options->split_size > 0) {
			split_v[0] = g_strdup_printf ("-v%llub", (unsigned long long) options->split_size);
		}
		if (format == NEMO_ARCHIVE_FORMAT_7Z) {
			solid_v[0] = g_strdup (options->solid ? "-ms=on" : "-ms=off");
		}
#ifndef G_OS_WIN32
		if (options->store_links) {
			links_v[0] = g_strdup ("-snl");
		}
#endif
	} else if (backend == NEMO_ARCHIVE_BACKEND_RAR) {
		key = NEMO_ARCHIVE_COMMAND_KEY_RAR;
		fallback = NEMO_ARCHIVE_COMMAND_RAR_DEFAULT;

		level_v[0] = g_strdup_printf ("-m%d", rar_level (level));
		threads_v[0] = g_strdup_printf ("-mt%d", threads);

		if (has_password) {
			password_v[0] = options->encrypt_names
				? g_strconcat ("-hp", options->password, NULL)
				: g_strconcat ("-p", options->password, NULL);
		}
		if (options->split_size > 0) {
			split_v[0] = g_strdup_printf ("-v%llub", (unsigned long long) options->split_size);
		}
		solid_v[0] = g_strdup (options->solid ? "-s" : "-s-");

		if (options->dedupe) {
			dedupe_v[0] = g_strdup ("-oi");
		}
		if (options->recovery_record) {
			recovery_v[0] = g_strdup ("-rr3p");
		}
		if (options->lock) {
			lock_v[0] = g_strdup ("-k");
		}

		links_v[0] = g_strdup (options->store_links ? "-ol"
				       : (options->follow_link_dirs ? "-ola" : "-ol-"));
	} else {
		return NULL;
	}

	program_v[0] = g_strdup (program);
	archive_v[0] = g_strdup (archive_path);

	sources = g_ptr_array_new_with_free_func (g_free);

	for (l = names; l != NULL; l = l->next) {
		g_ptr_array_add (sources, g_strdup (l->data));
	}
	g_ptr_array_add (sources, NULL);

	{
		const NemoCommandToken tokens[] = {
			/* The last column marks the ones that stand for something
			   the Compress dialog was asked for, so a line edited
			   past one of them can say which control went quiet. */
			{ "PROGRAM",        (const char *const *) program_v,     FALSE },
			{ "FORMAT",         (const char *const *) format_v,      TRUE },
			{ "LEVEL",          (const char *const *) level_v,       TRUE },
			{ "THREADS",        (const char *const *) threads_v,     FALSE },
			{ "PASSWORD",       (const char *const *) password_v,    TRUE },
			{ "SPLIT",          (const char *const *) split_v,       TRUE },
			{ "SOLID",          (const char *const *) solid_v,       TRUE },
			{ "DEDUPE",         (const char *const *) dedupe_v,      TRUE },
			{ "RECOVERY",       (const char *const *) recovery_v,    TRUE },
			{ "LOCK",           (const char *const *) lock_v,        TRUE },
			{ "LINKS",          (const char *const *) links_v,       TRUE },
			{ "TARGET_ARCHIVE", (const char *const *) archive_v,     FALSE },
			{ "SOURCE_ITEMS",   (const char *const *) sources->pdata, FALSE },
			{ NULL, NULL, FALSE }
		};

		text = nemo_command_template_from_config (NEMO_ARCHIVE_COMMANDS_GROUP, key, fallback);
		argv = nemo_command_template_expand (text, tokens, &error);

		if (argv == NULL) {
			g_warning ("The %s command line cannot be run as written (%s): %s",
				   key, error->message, text);
			g_clear_error (&error);
		} else {
			nemo_command_template_warn_unused (key, text, tokens);
		}
	}

	g_free (text);
	g_ptr_array_free (sources, TRUE);
	free_values (program_v);
	free_values (format_v);
	free_values (level_v);
	free_values (threads_v);
	free_values (password_v);
	free_values (split_v);
	free_values (solid_v);
	free_values (dedupe_v);
	free_values (recovery_v);
	free_values (lock_v);
	free_values (links_v);
	free_values (archive_v);

	return argv;
}

/* Both tools write a running "NN%". The newest one in the buffer wins. */
static gboolean
scan_percent (const char *text,
	      gsize       length,
	      int        *percent)
{
	gboolean found = FALSE;
	gsize i;

	for (i = 0; i < length; i++) {
		gsize start;
		int value;

		if (!g_ascii_isdigit (text[i])) {
			continue;
		}

		start = i;
		value = 0;
		while (i < length && g_ascii_isdigit (text[i])) {
			value = value * 10 + (text[i] - '0');
			if (value > 100) {
				value = 101;	/* too big to be a percentage */
			}
			i++;
		}

		if (i > start && i < length && text[i] == '%' && value <= 100) {
			*percent = value;
			found = TRUE;
		}
	}

	return found;
}

static gboolean
run_command (ArchiveJob *job)
{
	const char *program = backend_program (job->backend);
	char *base_path = NULL;
	char *archive_path = NULL;
	char **argv = NULL;
	GList *names = NULL;
	GList *l;
	GSubprocessLauncher *launcher;
	GSubprocess *process;
	GInputStream *out;
	GError *error = NULL;
	GString *tail;
	char buffer[4096];
	gboolean ok = FALSE;

	base_path = g_file_get_path (job->base_dir);
	archive_path = g_file_get_path (job->destination);

	if (program == NULL || base_path == NULL || archive_path == NULL) {
		job_fail (job, _("The archive could not be created."),
			  _("This format needs a program that is not installed, or a folder on this computer."));
		g_free (base_path);
		g_free (archive_path);
		return FALSE;
	}

	/* Both tools ADD to an archive that is already there rather than
	   replacing it, so an overwrite has to be a delete first. */
	if (g_file_query_exists (job->destination, NULL)) {
		g_file_delete (job->destination, NULL, NULL);
	}

	for (l = job->sources; l != NULL; l = l->next) {
		names = g_list_prepend (names, g_file_get_basename (G_FILE (l->data)));
	}
	names = g_list_reverse (names);

	argv = nemo_archive_build_command (job->backend, job->options.format, &job->options,
					   program, archive_path, names);

	launcher = g_subprocess_launcher_new (G_SUBPROCESS_FLAGS_STDOUT_PIPE |
					      G_SUBPROCESS_FLAGS_STDERR_MERGE);
	g_subprocess_launcher_set_cwd (launcher, base_path);

	process = g_subprocess_launcher_spawnv (launcher, (const gchar * const *) argv, &error);
	g_object_unref (launcher);

	if (process == NULL) {
		job_fail (job, _("The archive could not be created."),
			  error != NULL ? error->message : NULL);
		g_clear_error (&error);
		goto out;
	}

	set_unit_status (job, g_strdup_printf (_("Compressing with %s"),
					       job->backend == NEMO_ARCHIVE_BACKEND_RAR ? "rar" : "7z"));

	tail = g_string_new (NULL);
	out = g_subprocess_get_stdout_pipe (process);

	while (TRUE) {
		gssize count = g_input_stream_read (out, buffer, sizeof (buffer),
						    job->cancellable, NULL);
		int percent = -1;

		if (count <= 0) {
			break;
		}

		g_string_append_len (tail, buffer, count);
		if (tail->len > 4096) {
			g_string_erase (tail, 0, tail->len - 4096);
		}

		if (scan_percent (buffer, (gsize) count, &percent)) {
			nemo_progress_info_set_progress (job->progress, percent, 100);
		}
	}

	if (job_aborted (job)) {
		g_subprocess_force_exit (process);
	}

	ok = g_subprocess_wait_check (process, NULL, &error);
	if (!ok && !job_aborted (job)) {
		job_fail (job, _("The archive could not be created."),
			  tail->len > 0 ? tail->str : (error != NULL ? error->message : NULL));
	}

	g_clear_error (&error);
	g_string_free (tail, TRUE);
	g_object_unref (process);

 out:
	g_strfreev (argv);
	g_list_free_full (names, g_free);
	g_free (base_path);
	g_free (archive_path);

	return ok && !job_aborted (job);
}

static gboolean
archive_job_done (gpointer user_data)
{
	ArchiveJob *job = user_data;

	nemo_file_changes_consume_changes (TRUE);

	if (job->error_message != NULL) {
		eel_show_error_dialog (job->error_message, job->error_details, job->parent_window);
	}

	if (job->done_callback != NULL) {
		job->done_callback (job->success ? job->result_file : NULL,
				    job->success, job->done_callback_data);
	}

	nemo_progress_info_finish (job->progress);

	if (job->parent_window != NULL) {
		g_object_remove_weak_pointer (G_OBJECT (job->parent_window),
					      (gpointer *) &job->parent_window);
	}

	g_list_free_full (job->entries, (GDestroyNotify) archive_entry_free_full);
	g_list_free_full (job->units, (GDestroyNotify) archive_unit_free);
	g_clear_object (&job->result_file);
	g_clear_object (&job->base_dir);
	g_clear_object (&job->progress);
	g_clear_object (&job->cancellable);
	nemo_archive_options_clear (&job->options);
	g_free (job->error_message);
	g_free (job->error_details);
	g_free (job);

	return FALSE;
}

/* Per-archive state, taken up and put down around each unit so nothing from
   one archive is still standing when the next starts. */
static void
start_unit (ArchiveJob  *job,
	    ArchiveUnit *unit)
{
	job->sources = unit->sources;
	job->destination = unit->destination;
	job->base_dir = unit->sources != NULL ?
		g_file_get_parent (G_FILE (unit->sources->data)) : NULL;

	job->entries = NULL;
	job->total_bytes = 0;
	job->done_bytes = 0;
	job->file_count = 0;

	nemo_progress_info_take_details (job->progress,
					 g_file_get_basename (unit->destination));
}

static void
finish_unit (ArchiveJob *job)
{
	g_list_free_full (job->entries, (GDestroyNotify) archive_entry_free_full);
	job->entries = NULL;

	g_clear_object (&job->base_dir);
	job->sources = NULL;
	job->destination = NULL;
}

static gboolean
archive_job (GIOSchedulerJob *io_job,
	     GCancellable    *cancellable,
	     gpointer         user_data)
{
	ArchiveJob *job = user_data;
	GList *l;

	(void) cancellable;

	job->io_job = io_job;
	job->success = TRUE;
	nemo_progress_info_start (job->progress);

	for (l = job->units; l != NULL && !job_aborted (job); l = l->next) {
		gboolean ok;

		start_unit (job, l->data);

		if (job->base_dir == NULL) {
			job_fail (job, _("The archive could not be created."),
				  _("The selection has no folder above it to compress from."));
			ok = FALSE;
		} else if (job->backend == NEMO_ARCHIVE_BACKEND_LIBARCHIVE) {
			ok = run_libarchive (job);
		} else {
			ok = run_command (job);
		}

		/* A half-written archive is worse than none: it looks like a
		   result. */
		if (!ok) {
			g_file_delete (job->destination, NULL, NULL);
		} else {
			nemo_file_changes_queue_file_added (job->destination);
		}

		/* One archive failing does not take the rest of them with it -
		   the first thing that went wrong is what gets reported. */
		job->success = job->success && ok;

		finish_unit (job);
		job->unit_index++;
	}

	if (job_aborted (job)) {
		job->success = FALSE;
	}

	g_io_scheduler_job_send_to_mainloop_async (io_job, archive_job_done, job, NULL);

	return FALSE;
}

static ArchiveUnit *
archive_unit_new (GList *sources,
		  GFile *destination)
{
	ArchiveUnit *unit = g_new0 (ArchiveUnit, 1);
	GList *l;

	for (l = sources; l != NULL; l = l->next) {
		unit->sources = g_list_prepend (unit->sources, g_object_ref (G_FILE (l->data)));
	}
	unit->sources = g_list_reverse (unit->sources);
	unit->destination = g_object_ref (destination);

	return unit;
}

/* Everything the two entry points share: the progress window, the backend that
   writes every archive in the job, and the queueing. Takes the units. */
static void
start_job (ArchiveJob               *job,
	   GList                    *units,
	   GFile                    *result_file,
	   const NemoArchiveOptions *options,
	   GtkWindow                *parent_window,
	   NemoArchiveCallback       done_callback,
	   gpointer                  done_callback_data)
{
	char *initial_details;

	job->units = units;
	job->unit_count = g_list_length (units);
	job->result_file = g_object_ref (result_file);

	nemo_archive_options_copy (options, &job->options);
	job->backend = nemo_archive_pick_backend (options->format, options);

	job->done_callback = done_callback;
	job->done_callback_data = done_callback_data;

	job->parent_window = parent_window;
	if (parent_window != NULL) {
		g_object_add_weak_pointer (G_OBJECT (parent_window), (gpointer *) &job->parent_window);
	}

	job->progress = nemo_progress_info_new ();
	job->cancellable = nemo_progress_info_get_cancellable (job->progress);
	g_object_ref (job->cancellable);

	nemo_progress_info_set_status (job->progress, _("Preparing to compress"));

	if (job->unit_count > 1) {
		initial_details = g_strdup_printf (ngettext ("%d archive", "%d archives",
							     job->unit_count),
						   job->unit_count);
	} else {
		initial_details = g_file_get_basename (((ArchiveUnit *) units->data)->destination);
	}
	nemo_progress_info_take_initial_details (job->progress, initial_details);

	if (job->backend == NEMO_ARCHIVE_BACKEND_NONE) {
		job_fail (job, _("The archive could not be created."),
			  _("Nothing installed here can write this archive with the options chosen."));
		archive_job_done (job);
		return;
	}

	nemo_job_queue_add_new_job (nemo_job_queue_get (), archive_job, job,
				    job->cancellable, job->progress, TRUE);
}

void
nemo_archive_create (GList                    *sources,
		     GFile                    *destination,
		     const NemoArchiveOptions *options,
		     GtkWindow                *parent_window,
		     NemoArchiveCallback       done_callback,
		     gpointer                  done_callback_data)
{
	GList *units;

	g_return_if_fail (sources != NULL);
	g_return_if_fail (G_IS_FILE (destination));
	g_return_if_fail (options != NULL);

	units = g_list_append (NULL, archive_unit_new (sources, destination));

	start_job (g_new0 (ArchiveJob, 1), units, destination, options,
		   parent_window, done_callback, done_callback_data);
}

void
nemo_archive_create_each (GList                    *sources,
			  GFile                    *destination_dir,
			  const NemoArchiveOptions *options,
			  GtkWindow                *parent_window,
			  NemoArchiveCallback       done_callback,
			  gpointer                  done_callback_data)
{
	GList *units = NULL;
	GList *l;

	g_return_if_fail (sources != NULL);
	g_return_if_fail (G_IS_FILE (destination_dir));
	g_return_if_fail (options != NULL);

	for (l = sources; l != NULL; l = l->next) {
		GFile *source = G_FILE (l->data);
		GList *one;
		GFile *destination;
		char *basename;
		char *name;

		basename = g_file_get_basename (source);
		name = nemo_archive_each_name (basename, options->format);
		g_free (basename);

		/* Nothing worth naming an archive after - a root, say. */
		if (name == NULL) {
			continue;
		}

		destination = g_file_get_child_for_display_name (destination_dir, name, NULL);
		if (destination == NULL) {
			destination = g_file_get_child (destination_dir, name);
		}
		g_free (name);

		one = g_list_append (NULL, source);
		units = g_list_append (units, archive_unit_new (one, destination));
		g_list_free (one);
		g_object_unref (destination);
	}

	if (units == NULL) {
		return;
	}

	start_job (g_new0 (ArchiveJob, 1), units, destination_dir, options,
		   parent_window, done_callback, done_callback_data);
}
