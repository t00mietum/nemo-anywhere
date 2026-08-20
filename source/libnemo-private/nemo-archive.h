/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-archive.h - create an archive from a selection.

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

/* Two ways to write an archive, picked per request. libarchive is linked in and
 * handles the tar, zip and 7z families; it cannot write rar at all, and has no
 * write support for split volumes, solid blocks, duplicate-as-reference or 7z
 * encryption. Those go out to a 7z or rar command when the box has one. What a
 * given format can offer is therefore the union over the backends actually
 * present, which is why the dialog asks rather than assuming.
 */

#ifndef NEMO_ARCHIVE_H
#define NEMO_ARCHIVE_H

#include <gtk/gtk.h>
#include <gio/gio.h>

typedef enum {
	NEMO_ARCHIVE_FORMAT_ZIP,
	NEMO_ARCHIVE_FORMAT_TAR,
	NEMO_ARCHIVE_FORMAT_TAR_GZ,
	NEMO_ARCHIVE_FORMAT_TAR_XZ,
	NEMO_ARCHIVE_FORMAT_7Z,
	NEMO_ARCHIVE_FORMAT_RAR,
	NEMO_ARCHIVE_N_FORMATS
} NemoArchiveFormat;

/* Which program would write it. NONE means nothing installed here can. */
typedef enum {
	NEMO_ARCHIVE_BACKEND_NONE,
	NEMO_ARCHIVE_BACKEND_LIBARCHIVE,
	NEMO_ARCHIVE_BACKEND_7Z,
	NEMO_ARCHIVE_BACKEND_RAR
} NemoArchiveBackend;

typedef enum {
	NEMO_ARCHIVE_CAP_LEVEL         = 1 << 0,	/* compression level is meaningful */
	NEMO_ARCHIVE_CAP_PASSWORD      = 1 << 1,
	NEMO_ARCHIVE_CAP_ENCRYPT_NAMES = 1 << 2,	/* the file list is encrypted too */
	NEMO_ARCHIVE_CAP_SPLIT         = 1 << 3,	/* multi-volume */
	NEMO_ARCHIVE_CAP_SOLID         = 1 << 4,
	NEMO_ARCHIVE_CAP_DEDUPE        = 1 << 5,	/* identical files stored once */
	NEMO_ARCHIVE_CAP_STORE_LINKS   = 1 << 6,	/* symlinks/junctions kept as links */
	NEMO_ARCHIVE_CAP_RECOVERY      = 1 << 7,	/* recovery record */
	NEMO_ARCHIVE_CAP_LOCK          = 1 << 8	/* refuse later modification */
} NemoArchiveCaps;

/* Levels are 0-9 as the user sees them; each backend maps that onto its own
 * scale. 0 means store without compressing. */
#define NEMO_ARCHIVE_LEVEL_STORE   0
#define NEMO_ARCHIVE_LEVEL_DEFAULT 5
#define NEMO_ARCHIVE_LEVEL_MAX     9

typedef struct {
	NemoArchiveFormat format;
	gint              level;
	char             *password;		/* NULL or empty means none */
	gboolean          encrypt_names;
	guint64           split_size;		/* bytes per volume; 0 means one file */
	gboolean          solid;
	gboolean          dedupe;
	gboolean          store_links;
	gboolean          follow_link_dirs;	/* descend into a linked folder */
	gboolean          recovery_record;
	gboolean          lock;
} NemoArchiveOptions;

typedef void (* NemoArchiveCallback) (GFile    *archive_file,
				      gboolean  success,
				      gpointer  callback_data);

void     nemo_archive_options_init  (NemoArchiveOptions *options);
void     nemo_archive_options_clear (NemoArchiveOptions *options);
void     nemo_archive_options_copy  (const NemoArchiveOptions *source,
				     NemoArchiveOptions       *dest);

const char        *nemo_archive_format_id        (NemoArchiveFormat format);
const char        *nemo_archive_format_name      (NemoArchiveFormat format);
const char        *nemo_archive_format_extension (NemoArchiveFormat format);
gboolean           nemo_archive_format_from_id   (const char        *id,
						  NemoArchiveFormat *format);

/* What this box can actually do: the union over the backends installed. */
gboolean           nemo_archive_format_available (NemoArchiveFormat format);
NemoArchiveCaps    nemo_archive_format_caps      (NemoArchiveFormat format);

/* Pure, so it can be reasoned about (and tested) without probing the box. */
NemoArchiveCaps    nemo_archive_backend_caps     (NemoArchiveFormat  format,
						  NemoArchiveBackend backend);
gboolean           nemo_archive_backend_present  (NemoArchiveBackend backend);
NemoArchiveBackend nemo_archive_pick_backend     (NemoArchiveFormat         format,
						  const NemoArchiveOptions *options);

/* Name handling for the dialog: the extension follows the format, and a
 * selection suggests a name. */
char    *nemo_archive_strip_extension (const char *name);
char    *nemo_archive_apply_extension (const char       *name,
				       NemoArchiveFormat format);
char    *nemo_archive_suggest_name    (GList            *files,
				       NemoArchiveFormat format);

/* The command line a 7z or rar backend would be run with, in the base folder
   holding the selection. Exposed so the switches can be checked without
   spawning anything. */
char   **nemo_archive_build_command (NemoArchiveBackend        backend,
				     NemoArchiveFormat         format,
				     const NemoArchiveOptions *options,
				     const char               *program,
				     const char               *archive_path,
				     GList                    *names);

/* "700 MB", "4480m", "1.5 GB" -> bytes. Returns FALSE on anything unreadable. */
gboolean nemo_archive_parse_size (const char *text,
				  guint64    *bytes);
char    *nemo_archive_format_size (guint64 bytes);

/* Queues the job. sources are GFile *; destination is the archive itself. */
void nemo_archive_create (GList                    *sources,
			  GFile                    *destination,
			  const NemoArchiveOptions *options,
			  GtkWindow                *parent_window,
			  NemoArchiveCallback       done_callback,
			  gpointer                  done_callback_data);

#endif /* NEMO_ARCHIVE_H */
