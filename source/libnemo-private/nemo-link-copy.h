/* nemo-link-copy.h - what happens to a link when it is copied somewhere else.
 * Every platform can hold a symlink; Windows adds the junction. The copy engine
 * asks here what a link should become, and the answer comes from a dialog when
 * there is more than one sensible one.
 *
 * Copyright © 2026 Bubbles
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by the
 * Free Software Foundation.
 */

#ifndef NEMO_LINK_COPY_H
#define NEMO_LINK_COPY_H

#include <gtk/gtk.h>
#include <gio/gio.h>

G_BEGIN_DECLS

/* A bitmask so a set of files can report every kind it holds at once. NONE
   doubles as "copy what it points at" wherever a choice is being carried. */
typedef enum {
	NEMO_LINK_NONE         = 0,
	NEMO_LINK_FILE_SYMLINK = 1 << 0,
	NEMO_LINK_DIR_SYMLINK  = 1 << 1,
	NEMO_LINK_JUNCTION     = 1 << 2   /* Windows only */
} NemoLinkKind;

#define NEMO_LINK_ANY (NEMO_LINK_FILE_SYMLINK | NEMO_LINK_DIR_SYMLINK | NEMO_LINK_JUNCTION)

/* What each kind found in the source should become. */
typedef struct {
	NemoLinkKind file_symlink_as;
	NemoLinkKind dir_symlink_as;
	NemoLinkKind junction_as;
} NemoLinkChoice;

/* What kind of link this is, or NONE. info may be NULL, and is only read for
   the answer it already holds - nothing is queried through it. */
NemoLinkKind nemo_link_kind (GFile     *file,
                             GFileInfo *info);

/* What a link points at, spelled the way the link itself spells it, so a
   relative one answers with its relative text. Caller frees. */
gboolean nemo_link_read_target (GFile   *file,
                                char   **target,
                                GError **error);

/* Make a link of the kind asked for. A junction wants an absolute local
   directory, so a relative target is resolved against base_dir first (the
   directory the original link sat in; NULL to skip). */
gboolean nemo_link_create (const char    *target,
                           const char    *link_path,
                           const char    *base_dir,
                           NemoLinkKind   kind,
                           GError       **error);

/* Which kinds can be created inside dir_path. */
guint nemo_link_kinds_supported (const char *dir_path);

/* Every kind keeps its own kind where the destination allows it, otherwise the
   nearest one that still points at the same target, otherwise a copy. */
void nemo_link_choice_init (NemoLinkChoice *choice,
                            guint           supported);

/* Whether choice asks for anything other than a plain copy. */
gboolean nemo_link_choice_makes_links (const NemoLinkChoice *choice);

/* What to make for a source of this kind. */
NemoLinkKind nemo_link_choice_for (const NemoLinkChoice *choice,
                                   NemoLinkKind          found);

/* Returns FALSE if the operation was cancelled. present and supported are
   NemoLinkKind bitmasks. */
gboolean nemo_link_choice_ask (GtkWindow      *parent,
                               GFile          *destination,
                               guint           present,
                               guint           supported,
                               gboolean        is_move,
                               NemoLinkChoice *choice);

G_END_DECLS

#endif /* NEMO_LINK_COPY_H */
