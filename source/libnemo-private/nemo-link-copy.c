/* nemo-link-copy.c
 *
 * Copyright © 2026 Bubbles
 *
 * This program is free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by the
 * Free Software Foundation.
 */

#include <config.h>
#include "nemo-link-copy.h"

#include <glib/gi18n.h>

#ifdef G_OS_WIN32
#include "nemo-link-win32.h"
#endif

NemoLinkKind
nemo_link_kind (GFile     *file,
                GFileInfo *info)
{
	const char *path;

	if (info != NULL &&
	    g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK) &&
	    !g_file_info_get_is_symlink (info)) {
		return NEMO_LINK_NONE;
	}

	path = g_file_peek_path (file);
	if (path == NULL) {
		return NEMO_LINK_NONE;
	}

#ifdef G_OS_WIN32
	/* The reparse tag is the only thing that tells a junction from a symlink,
	   and it also keeps the other things the file system uses reparse points
	   for - cloud placeholders, store app aliases - out of the way. */
	return nemo_win32_link_kind (path);
#else
	if (info == NULL && !g_file_test (path, G_FILE_TEST_IS_SYMLINK)) {
		return NEMO_LINK_NONE;
	}
	if (info != NULL && g_file_info_get_file_type (info) != G_FILE_TYPE_SYMBOLIC_LINK &&
	    !g_file_info_get_is_symlink (info)) {
		return NEMO_LINK_NONE;
	}

	/* Follows the link, which is the only way to know what it points at. One
	   extra stat per link, and links are rare in a copy. */
	return g_file_test (path, G_FILE_TEST_IS_DIR) ? NEMO_LINK_DIR_SYMLINK
						      : NEMO_LINK_FILE_SYMLINK;
#endif
}

gboolean
nemo_link_read_target (GFile   *file,
                       char   **target,
                       GError **error)
{
#ifdef G_OS_WIN32
	const char *path = g_file_peek_path (file);

	if (path == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
				     _("That is not a local file."));
		return FALSE;
	}

	return nemo_win32_link_read_target (path, target, error);
#else
	GFileInfo *info;
	const char *found;

	*target = NULL;

	info = g_file_query_info (file, G_FILE_ATTRIBUTE_STANDARD_SYMLINK_TARGET,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, error);
	if (info == NULL) {
		return FALSE;
	}

	found = g_file_info_get_symlink_target (info);
	if (found != NULL) {
		*target = g_strdup (found);
	}
	g_object_unref (info);

	if (*target == NULL) {
		g_set_error_literal (error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
				     _("That is not a link."));
		return FALSE;
	}

	return TRUE;
#endif
}

gboolean
nemo_link_create (const char    *target,
                  const char    *link_path,
                  const char    *base_dir,
                  NemoLinkKind   kind,
                  GError       **error)
{
#ifdef G_OS_WIN32
	return nemo_win32_link_create (target, link_path, base_dir, kind, error);
#else
	GFile *link = g_file_new_for_path (link_path);
	gboolean ok;

	(void) base_dir;
	(void) kind;

	ok = g_file_make_symbolic_link (link, target, NULL, error);
	g_object_unref (link);

	return ok;
#endif
}

guint
nemo_link_kinds_supported (const char *dir_path)
{
	if (dir_path == NULL) {
		return 0;
	}

#ifdef G_OS_WIN32
	return nemo_win32_link_kinds_supported (dir_path);
#else
	/* Nothing short of trying says whether a given file system will take a
	   symlink, and a refusal comes back as an ordinary copy error. */
	return NEMO_LINK_FILE_SYMLINK | NEMO_LINK_DIR_SYMLINK;
#endif
}

static NemoLinkKind
first_allowed (const NemoLinkKind *order,
               guint               supported)
{
	int i;

	for (i = 0; order[i] != NEMO_LINK_NONE; i++) {
		if (supported & order[i]) {
			return order[i];
		}
	}

	return NEMO_LINK_NONE;
}

void
nemo_link_choice_init (NemoLinkChoice *choice,
                       guint           supported)
{
	/* Same kind first, then the nearest kind that still points at the original
	   target, then a plain copy. */
	static const NemoLinkKind file_order[] = {
		NEMO_LINK_FILE_SYMLINK, NEMO_LINK_NONE
	};
	static const NemoLinkKind dir_sym_order[] = {
		NEMO_LINK_DIR_SYMLINK, NEMO_LINK_JUNCTION, NEMO_LINK_NONE
	};
	static const NemoLinkKind junction_order[] = {
		NEMO_LINK_JUNCTION, NEMO_LINK_DIR_SYMLINK, NEMO_LINK_NONE
	};

	choice->file_symlink_as = first_allowed (file_order, supported);
	choice->dir_symlink_as = first_allowed (dir_sym_order, supported);
	choice->junction_as = first_allowed (junction_order, supported);
}

gboolean
nemo_link_choice_makes_links (const NemoLinkChoice *choice)
{
	return choice->file_symlink_as != NEMO_LINK_NONE ||
	       choice->dir_symlink_as != NEMO_LINK_NONE ||
	       choice->junction_as != NEMO_LINK_NONE;
}

NemoLinkKind
nemo_link_choice_for (const NemoLinkChoice *choice,
                      NemoLinkKind          found)
{
	switch (found) {
	case NEMO_LINK_FILE_SYMLINK:
		return choice->file_symlink_as;
	case NEMO_LINK_DIR_SYMLINK:
		return choice->dir_symlink_as;
	case NEMO_LINK_JUNCTION:
		return choice->junction_as;
	default:
		return NEMO_LINK_NONE;
	}
}

/* One column per kind, so the rows line up whatever they offer. The junction
   column is only built where junctions exist at all. */
enum {
	COLUMN_SYMLINK,
	COLUMN_JUNCTION,
	COLUMN_COPY,
	N_COLUMNS
};

typedef struct {
	NemoLinkKind  found;
	const char   *label;
	NemoLinkKind  offers[N_COLUMNS];
	GtkWidget    *buttons[N_COLUMNS];
} Row;

static void
add_row (GtkGrid      *grid,
         int           at,
         Row          *row,
         guint         supported,
         gboolean      with_junctions,
         NemoLinkKind  selected)
{
	GtkWidget *label;
	GtkWidget *group = NULL;
	int column;

	label = gtk_label_new (row->label);
	gtk_widget_set_halign (label, GTK_ALIGN_START);
	gtk_grid_attach (grid, label, 0, at, 1, 1);

	for (column = 0; column < N_COLUMNS; column++) {
		NemoLinkKind offer = row->offers[column];
		GtkWidget *button;

		row->buttons[column] = NULL;

		if (column == COLUMN_JUNCTION && !with_junctions) {
			continue;
		}
		if (offer == NEMO_LINK_NONE && column != COLUMN_COPY) {
			continue;
		}

		button = gtk_radio_button_new_with_label_from_widget (
				group ? GTK_RADIO_BUTTON (group) : NULL,
				column == COLUMN_SYMLINK ? _("Symlink") :
				column == COLUMN_JUNCTION ? _("Junction") : _("Copy"));
		if (group == NULL) {
			group = button;
		}

		/* A copy is always possible; the rest need the destination's blessing. */
		if (column != COLUMN_COPY && !(supported & offer)) {
			gtk_widget_set_sensitive (button, FALSE);
		}
		if (offer == selected) {
			gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), TRUE);
		}

		gtk_grid_attach (grid, button, column + 1, at, 1, 1);
		row->buttons[column] = button;
	}
}

static NemoLinkKind
read_row (const Row *row)
{
	int column;

	for (column = 0; column < N_COLUMNS; column++) {
		if (row->buttons[column] != NULL &&
		    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (row->buttons[column]))) {
			return row->offers[column];
		}
	}

	return NEMO_LINK_NONE;
}

gboolean
nemo_link_choice_ask (GtkWindow      *parent,
                      GFile          *destination,
                      guint           present,
                      guint           supported,
                      gboolean        is_move,
                      NemoLinkChoice *choice)
{
	GtkWidget *dialog, *area, *box, *grid, *note;
	Row rows[3];
	gboolean with_junctions;
	int used = 0;
	int i;
	char *dest_name;
	char *secondary;
	int response;

	nemo_link_choice_init (choice, supported);
	with_junctions = ((present | supported) & NEMO_LINK_JUNCTION) != 0;

	dest_name = destination ? g_file_get_basename (destination) : NULL;
	if (dest_name != NULL) {
		secondary = g_strdup_printf (_("Choose what to put in \"%s\"."), dest_name);
	} else {
		secondary = g_strdup (_("Choose what to put in the destination folder."));
	}
	g_free (dest_name);

	dialog = gtk_message_dialog_new (parent, 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE, NULL);
	g_object_set (dialog,
		      "text", _("Some of these are links"),
		      "secondary-text", secondary,
		      NULL);
	g_free (secondary);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog),
			       is_move ? _("_Move") : _("_Copy"),
			       GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);

	if (present & NEMO_LINK_FILE_SYMLINK) {
		Row row = { NEMO_LINK_FILE_SYMLINK, _("File symlinks:"),
			    { NEMO_LINK_FILE_SYMLINK, NEMO_LINK_NONE, NEMO_LINK_NONE },
			    { NULL, NULL, NULL } };
		rows[used++] = row;
	}
	if (present & NEMO_LINK_DIR_SYMLINK) {
		Row row = { NEMO_LINK_DIR_SYMLINK, _("Folder symlinks:"),
			    { NEMO_LINK_DIR_SYMLINK, NEMO_LINK_JUNCTION, NEMO_LINK_NONE },
			    { NULL, NULL, NULL } };
		rows[used++] = row;
	}
	if (present & NEMO_LINK_JUNCTION) {
		Row row = { NEMO_LINK_JUNCTION, _("Folder junctions:"),
			    { NEMO_LINK_DIR_SYMLINK, NEMO_LINK_JUNCTION, NEMO_LINK_NONE },
			    { NULL, NULL, NULL } };
		rows[used++] = row;
	}

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	note = gtk_label_new (_("A copy holds the contents; a link keeps pointing at the original."));
	gtk_widget_set_halign (note, GTK_ALIGN_START);
	gtk_box_pack_start (GTK_BOX (box), note, FALSE, FALSE, 0);

	grid = gtk_grid_new ();
	gtk_widget_set_halign (grid, GTK_ALIGN_START);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	for (i = 0; i < used; i++) {
		add_row (GTK_GRID (grid), i, &rows[i], supported, with_junctions,
			 nemo_link_choice_for (choice, rows[i].found));
	}
	gtk_box_pack_start (GTK_BOX (box), grid, FALSE, FALSE, 0);

	area = gtk_message_dialog_get_message_area (GTK_MESSAGE_DIALOG (dialog));
	gtk_box_pack_start (GTK_BOX (area), box, FALSE, FALSE, 6);
	gtk_widget_show_all (box);

	response = gtk_dialog_run (GTK_DIALOG (dialog));

	if (response == GTK_RESPONSE_OK) {
		for (i = 0; i < used; i++) {
			NemoLinkKind picked = read_row (&rows[i]);

			switch (rows[i].found) {
			case NEMO_LINK_FILE_SYMLINK:
				choice->file_symlink_as = picked;
				break;
			case NEMO_LINK_DIR_SYMLINK:
				choice->dir_symlink_as = picked;
				break;
			default:
				choice->junction_as = picked;
				break;
			}
		}
	}

	gtk_widget_destroy (dialog);

	return response == GTK_RESPONSE_OK;
}
