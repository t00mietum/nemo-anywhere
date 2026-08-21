/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-archive-dialog.c - ask what archive to make, then make it.

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

/* Name, format and folder are the whole dialog until the options expander is
 * opened. What is inside it depends on the format: an option no installed
 * program can honour for that format is shown greyed rather than hidden, so a
 * format that gains an option once 7z or rar is installed does not look like a
 * different dialog.
 */

#include <config.h>
#include "nemo-archive-dialog.h"

#include <glib/gi18n.h>

#include <eel/eel-stock-dialogs.h>
#include <libnemo-private/nemo-archive.h>
#include <libnemo-private/nemo-global-preferences.h>

typedef struct {
	GtkWidget *dialog;
	GtkWidget *name_entry;
	GtkWidget *name_label;
	GtkWidget *format_combo;
	GtkWidget *folder_button;
	GtkWidget *each_check;

	GtkWidget *level_scale;
	GtkWidget *level_label;
	GtkWidget *password_entry;
	GtkWidget *password_label;
	GtkWidget *encrypt_names_check;
	GtkWidget *split_check;
	GtkWidget *split_combo;
	GtkWidget *solid_check;
	GtkWidget *dedupe_check;
	GtkWidget *store_links_check;
	GtkWidget *follow_links_check;
	GtkWidget *recovery_check;
	GtkWidget *lock_check;

	GtkWidget *compress_button;

	GList     *files;		/* GFile *, owned */
	gboolean   whole_folder;	/* the selection is all the folder shows */
	gboolean   name_edited;		/* the user typed, so stop rewriting it */
} ArchiveDialog;

/* Volume sizes people actually use, as the editable dropdown's starting list. */
static const char * const split_sizes[] = {
	"100 MB", "700 MB", "1 GB", "2 GB", "4480 MB", "25 GB", NULL
};

static NemoArchiveFormat
current_format (ArchiveDialog *self)
{
	const char *id = gtk_combo_box_get_active_id (GTK_COMBO_BOX (self->format_combo));
	NemoArchiveFormat format = NEMO_ARCHIVE_FORMAT_ZIP;

	nemo_archive_format_from_id (id, &format);

	return format;
}

static void
set_row_sensitive (GtkWidget *widget,
		   gboolean   sensitive)
{
	if (widget != NULL) {
		gtk_widget_set_sensitive (widget, sensitive);
	}
}

/* One archive per item, each named after its item. */
static gboolean
compressing_each (ArchiveDialog *self)
{
	return self->each_check != NULL &&
	       gtk_widget_get_sensitive (self->each_check) &&
	       gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->each_check));
}

/* Part of a folder gets no suggested name, so the field starts empty and there
   is nothing to compress into until it is filled in. Separate archives take
   their names from the items, so an empty field stops nothing. */
static void
update_name_validity (ArchiveDialog *self)
{
	gboolean each = compressing_each (self);

	gtk_widget_set_sensitive (self->name_entry, !each);
	set_row_sensitive (self->name_label, !each);

	if (self->compress_button != NULL) {
		gtk_widget_set_sensitive (self->compress_button,
					  each ||
					  gtk_entry_get_text_length (GTK_ENTRY (self->name_entry)) > 0);
	}
}

/* Everything that depends on the chosen format: which options that format can
   honour on this box, and the extension on the suggested name. */
static void
update_for_format (ArchiveDialog *self)
{
	NemoArchiveFormat format = current_format (self);
	NemoArchiveCaps caps = nemo_archive_format_caps (format);
	gboolean has_password;
	gboolean splitting;
	char *name;

	set_row_sensitive (self->level_scale, (caps & NEMO_ARCHIVE_CAP_LEVEL) != 0);
	set_row_sensitive (self->level_label, (caps & NEMO_ARCHIVE_CAP_LEVEL) != 0);
	set_row_sensitive (self->password_entry, (caps & NEMO_ARCHIVE_CAP_PASSWORD) != 0);
	set_row_sensitive (self->password_label, (caps & NEMO_ARCHIVE_CAP_PASSWORD) != 0);
	set_row_sensitive (self->split_check, (caps & NEMO_ARCHIVE_CAP_SPLIT) != 0);
	set_row_sensitive (self->solid_check, (caps & NEMO_ARCHIVE_CAP_SOLID) != 0);
	set_row_sensitive (self->dedupe_check, (caps & NEMO_ARCHIVE_CAP_DEDUPE) != 0);
	set_row_sensitive (self->store_links_check, (caps & NEMO_ARCHIVE_CAP_STORE_LINKS) != 0);
	set_row_sensitive (self->recovery_check, (caps & NEMO_ARCHIVE_CAP_RECOVERY) != 0);
	set_row_sensitive (self->lock_check, (caps & NEMO_ARCHIVE_CAP_LOCK) != 0);

	/* Encrypting the names is only meaningful once there is a password, and
	   the volume size only once splitting is asked for. */
	has_password = gtk_entry_get_text_length (GTK_ENTRY (self->password_entry)) > 0 &&
		       (caps & NEMO_ARCHIVE_CAP_PASSWORD) != 0;
	set_row_sensitive (self->encrypt_names_check,
			   has_password && (caps & NEMO_ARCHIVE_CAP_ENCRYPT_NAMES) != 0);

	splitting = (caps & NEMO_ARCHIVE_CAP_SPLIT) != 0 &&
		    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->split_check));
	set_row_sensitive (self->split_combo, splitting);

	/* Following linked folders is ours, not the format's - except that a
	   backend storing the links is not descending into them either. */
	set_row_sensitive (self->follow_links_check,
			   !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->store_links_check)) ||
			   (caps & NEMO_ARCHIVE_CAP_STORE_LINKS) == 0);

	if (!self->name_edited) {
		name = nemo_archive_suggest_name (self->files, self->whole_folder, format);
	} else {
		const char *typed = gtk_entry_get_text (GTK_ENTRY (self->name_entry));

		/* An emptied field stays empty rather than becoming a bare
		   extension the user would have to delete again. */
		name = typed[0] != '\0' ? nemo_archive_apply_extension (typed, format) : NULL;
	}

	/* Setting the text counts as a change, so the guard has to be up first. */
	g_object_set_data (G_OBJECT (self->name_entry), "nemo-archive-programmatic",
			   GINT_TO_POINTER (1));
	gtk_entry_set_text (GTK_ENTRY (self->name_entry), name != NULL ? name : "");
	g_object_set_data (G_OBJECT (self->name_entry), "nemo-archive-programmatic", NULL);

	g_free (name);

	update_name_validity (self);
}

static void
format_changed (GtkComboBox *combo,
		gpointer     user_data)
{
	(void) combo;

	update_for_format (user_data);
}

static void
option_toggled (GtkToggleButton *button,
		gpointer         user_data)
{
	(void) button;

	update_for_format (user_data);
}

static void
password_changed (GtkEditable *editable,
		  gpointer     user_data)
{
	(void) editable;

	update_for_format (user_data);
}

static void
each_toggled (GtkToggleButton *button,
	      gpointer         user_data)
{
	(void) button;

	update_name_validity (user_data);
}

static void
name_changed (GtkEditable *editable,
	      gpointer     user_data)
{
	ArchiveDialog *self = user_data;

	if (g_object_get_data (G_OBJECT (editable), "nemo-archive-programmatic") == NULL) {
		self->name_edited = TRUE;
	}

	update_name_validity (self);
}

/* One labelled row in the options grid. */
static GtkWidget *
add_row (GtkWidget  *grid,
	 int         row,
	 const char *label_text,
	 GtkWidget  *widget)
{
	GtkWidget *label;

	label = gtk_label_new_with_mnemonic (label_text);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), widget);
	gtk_grid_attach (GTK_GRID (grid), label, 0, row, 1, 1);

	gtk_widget_set_hexpand (widget, TRUE);
	gtk_grid_attach (GTK_GRID (grid), widget, 1, row, 1, 1);

	return label;
}

static GtkWidget *
add_check (GtkWidget  *grid,
	   int         row,
	   const char *label_text,
	   gboolean    active)
{
	GtkWidget *check;

	check = gtk_check_button_new_with_mnemonic (label_text);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (check), active);
	gtk_grid_attach (GTK_GRID (grid), check, 0, row, 2, 1);

	return check;
}

static void
build_options (ArchiveDialog *self,
	       GtkWidget     *box)
{
	GtkWidget *expander;
	GtkWidget *grid;
	int row = 0;
	int i;

	expander = gtk_expander_new_with_mnemonic (_("_Options"));
	gtk_box_pack_start (GTK_BOX (box), expander, FALSE, FALSE, 0);

	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_widget_set_margin_top (grid, 6);
	gtk_widget_set_margin_start (grid, 12);
	gtk_container_add (GTK_CONTAINER (expander), grid);

	self->level_scale = gtk_scale_new_with_range (GTK_ORIENTATION_HORIZONTAL,
						      NEMO_ARCHIVE_LEVEL_STORE,
						      NEMO_ARCHIVE_LEVEL_MAX, 1);
	gtk_scale_set_draw_value (GTK_SCALE (self->level_scale), FALSE);
	gtk_range_set_value (GTK_RANGE (self->level_scale), NEMO_ARCHIVE_LEVEL_DEFAULT);
	gtk_scale_add_mark (GTK_SCALE (self->level_scale), NEMO_ARCHIVE_LEVEL_STORE,
			    GTK_POS_BOTTOM, _("Store"));
	gtk_scale_add_mark (GTK_SCALE (self->level_scale), NEMO_ARCHIVE_LEVEL_DEFAULT,
			    GTK_POS_BOTTOM, _("Normal"));
	gtk_scale_add_mark (GTK_SCALE (self->level_scale), NEMO_ARCHIVE_LEVEL_MAX,
			    GTK_POS_BOTTOM, _("Smallest"));
	self->level_label = add_row (grid, row++, _("Co_mpression"), self->level_scale);

	self->password_entry = gtk_entry_new ();
	gtk_entry_set_visibility (GTK_ENTRY (self->password_entry), FALSE);
	gtk_entry_set_input_purpose (GTK_ENTRY (self->password_entry), GTK_INPUT_PURPOSE_PASSWORD);
	gtk_entry_set_placeholder_text (GTK_ENTRY (self->password_entry), _("None"));
	self->password_label = add_row (grid, row++, _("Pass_word"), self->password_entry);
	g_signal_connect (self->password_entry, "changed", G_CALLBACK (password_changed), self);

	self->encrypt_names_check = add_check (grid, row++, _("Encrypt the _file names too"), FALSE);

	self->split_check = add_check (grid, row++, _("Split into _volumes"), FALSE);
	g_signal_connect (self->split_check, "toggled", G_CALLBACK (option_toggled), self);

	self->split_combo = gtk_combo_box_text_new_with_entry ();
	for (i = 0; split_sizes[i] != NULL; i++) {
		gtk_combo_box_text_append_text (GTK_COMBO_BOX_TEXT (self->split_combo), split_sizes[i]);
	}
	gtk_combo_box_set_active (GTK_COMBO_BOX (self->split_combo), 1);
	add_row (grid, row++, _("Volume si_ze"), self->split_combo);

	self->solid_check = add_check (grid, row++, _("_Solid archive"), FALSE);
	self->dedupe_check = add_check (grid, row++, _("Store _duplicate files once"), FALSE);

	self->store_links_check = add_check (grid, row++,
					     _("Store s_ymlinks and junctions as links"), TRUE);
	g_signal_connect (self->store_links_check, "toggled", G_CALLBACK (option_toggled), self);

	self->follow_links_check = add_check (grid, row++,
					      _("Follow _linked folders into the archive"), FALSE);

	self->recovery_check = add_check (grid, row++, _("Add a _recovery record"), TRUE);
	self->lock_check = add_check (grid, row++, _("Loc_k the archive against changes"), FALSE);
}

static void
collect_options (ArchiveDialog      *self,
		 NemoArchiveOptions *options)
{
	NemoArchiveCaps caps;
	const char *password;

	nemo_archive_options_init (options);

	options->format = current_format (self);
	caps = nemo_archive_format_caps (options->format);

	options->level = (int) gtk_range_get_value (GTK_RANGE (self->level_scale));

	password = gtk_entry_get_text (GTK_ENTRY (self->password_entry));
	if ((caps & NEMO_ARCHIVE_CAP_PASSWORD) != 0 && password != NULL && password[0] != '\0') {
		options->password = g_strdup (password);
		options->encrypt_names = (caps & NEMO_ARCHIVE_CAP_ENCRYPT_NAMES) != 0 &&
			gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->encrypt_names_check));
	}

	if ((caps & NEMO_ARCHIVE_CAP_SPLIT) != 0 &&
	    gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->split_check))) {
		GtkWidget *entry = gtk_bin_get_child (GTK_BIN (self->split_combo));
		guint64 bytes = 0;

		if (nemo_archive_parse_size (gtk_entry_get_text (GTK_ENTRY (entry)), &bytes)) {
			options->split_size = bytes;
		}
	}

	options->solid = (caps & NEMO_ARCHIVE_CAP_SOLID) != 0 &&
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->solid_check));
	options->dedupe = (caps & NEMO_ARCHIVE_CAP_DEDUPE) != 0 &&
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->dedupe_check));
	options->store_links = (caps & NEMO_ARCHIVE_CAP_STORE_LINKS) != 0 &&
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->store_links_check));
	options->follow_link_dirs =
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->follow_links_check));
	options->recovery_record = (caps & NEMO_ARCHIVE_CAP_RECOVERY) != 0 &&
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->recovery_check));
	options->lock = (caps & NEMO_ARCHIVE_CAP_LOCK) != 0 &&
		gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (self->lock_check));
}

/* Asked once for the lot: with a selection compressed separately there can be
   any number of them, and answering one at a time before anything has started
   is worse than seeing the count. */
static gboolean
confirm_overwrite (ArchiveDialog *self,
		   GList         *destinations)
{
	GList *existing = NULL;
	GList *l;
	guint count;
	char *primary;
	int response;

	for (l = destinations; l != NULL; l = l->next) {
		if (g_file_query_exists (G_FILE (l->data), NULL)) {
			existing = g_list_prepend (existing, l->data);
		}
	}

	if (existing == NULL) {
		return TRUE;
	}

	count = g_list_length (existing);

	if (count == 1) {
		char *name = g_file_get_basename (G_FILE (existing->data));

		primary = g_strdup_printf (_("A file named \"%s\" already exists. Replace it?"), name);
		g_free (name);
	} else {
		primary = g_strdup_printf (_("%d of the archives already exist. Replace them?"), count);
	}

	response = eel_run_simple_dialog (self->dialog, TRUE, GTK_MESSAGE_QUESTION,
					  primary,
					  ngettext ("Replacing it overwrites its contents.",
						    "Replacing them overwrites their contents.",
						    count),
					  GTK_STOCK_CANCEL, _("_Replace"), NULL);

	g_free (primary);
	g_list_free (existing);

	return response == 1;
}

/* The archives separate mode would write. Worked out here as well as in the
   job so that what is already there can be asked about before it starts. */
static GList *
each_destinations (ArchiveDialog     *self,
		   GFile             *folder,
		   NemoArchiveFormat  format)
{
	GList *destinations = NULL;
	GList *l;

	for (l = self->files; l != NULL; l = l->next) {
		GFile *destination;
		char *basename;
		char *name;

		basename = g_file_get_basename (G_FILE (l->data));
		name = nemo_archive_each_name (basename, format);
		g_free (basename);

		if (name == NULL) {
			continue;
		}

		destination = g_file_get_child_for_display_name (folder, name, NULL);
		if (destination == NULL) {
			destination = g_file_get_child (folder, name);
		}
		g_free (name);

		destinations = g_list_prepend (destinations, destination);
	}

	return g_list_reverse (destinations);
}

static void
dialog_free (ArchiveDialog *self)
{
	g_list_free_full (self->files, g_object_unref);
	g_free (self);
}

void
nemo_archive_dialog_show (GtkWindow *parent_window,
			  GList     *files,
			  GFile     *default_dir,
			  gboolean   whole_folder)
{
	ArchiveDialog *self;
	GtkWidget *content;
	GtkWidget *box;
	GtkWidget *grid;
	GtkWidget *compress_button;
	GList *l;
	int i;
	int row = 0;
	gboolean any_format = FALSE;

	g_return_if_fail (files != NULL);

	self = g_new0 (ArchiveDialog, 1);

	for (l = files; l != NULL; l = l->next) {
		self->files = g_list_prepend (self->files, g_object_ref (G_FILE (l->data)));
	}
	self->files = g_list_reverse (self->files);
	self->whole_folder = whole_folder;

	self->dialog = gtk_dialog_new_with_buttons (_("Compress"), parent_window,
						    GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
						    GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
						    _("C_ompress"), GTK_RESPONSE_OK,
						    NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (self->dialog), GTK_RESPONSE_OK);
	compress_button = gtk_dialog_get_widget_for_response (GTK_DIALOG (self->dialog), GTK_RESPONSE_OK);
	gtk_widget_set_can_default (compress_button, TRUE);

	content = gtk_dialog_get_content_area (GTK_DIALOG (self->dialog));
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 12);
	gtk_widget_set_margin_start (box, 12);
	gtk_widget_set_margin_end (box, 12);
	gtk_widget_set_margin_top (box, 12);
	gtk_widget_set_margin_bottom (box, 12);
	gtk_box_pack_start (GTK_BOX (content), box, TRUE, TRUE, 0);

	grid = gtk_grid_new ();
	gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
	gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
	gtk_box_pack_start (GTK_BOX (box), grid, FALSE, FALSE, 0);

	self->name_entry = gtk_entry_new ();
	gtk_entry_set_activates_default (GTK_ENTRY (self->name_entry), TRUE);
	gtk_entry_set_width_chars (GTK_ENTRY (self->name_entry), 36);
	self->name_label = add_row (grid, row++, _("_Name"), self->name_entry);

	self->format_combo = gtk_combo_box_text_new ();
	for (i = 0; i < NEMO_ARCHIVE_N_FORMATS; i++) {
		/* A format nothing here can write is left out entirely - it
		   would only be a dead entry. */
		if (!nemo_archive_format_available ((NemoArchiveFormat) i)) {
			continue;
		}
		gtk_combo_box_text_append (GTK_COMBO_BOX_TEXT (self->format_combo),
					   nemo_archive_format_id ((NemoArchiveFormat) i),
					   nemo_archive_format_name ((NemoArchiveFormat) i));
		any_format = TRUE;
	}
	add_row (grid, row++, _("_Format"), self->format_combo);

	self->folder_button = gtk_file_chooser_button_new (_("Where to put it"),
							   GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
	if (default_dir != NULL) {
		gtk_file_chooser_set_current_folder_file (GTK_FILE_CHOOSER (self->folder_button),
							  default_dir, NULL);
	}
	add_row (grid, row++, _("_Where"), self->folder_button);

	/* Greyed rather than hidden with one item selected: it is the same
	   dialog either way, and there is nothing to explain about why it is
	   not offered. */
	self->each_check = add_check (grid, row++, _("Compress each item se_parately"), FALSE);
	gtk_widget_set_sensitive (self->each_check, files->next != NULL);
	gtk_widget_set_tooltip_text (self->each_check,
				     _("Each item becomes its own archive, named after it."));
	g_signal_connect (self->each_check, "toggled", G_CALLBACK (each_toggled), self);

	build_options (self, box);

	g_signal_connect (self->format_combo, "changed", G_CALLBACK (format_changed), self);
	g_signal_connect (self->name_entry, "changed", G_CALLBACK (name_changed), self);

	if (!any_format) {
		/* Cannot happen with libarchive linked in, but a combo with no
		   rows would silently make the button do nothing. */
		gtk_widget_set_sensitive (compress_button, FALSE);
	} else {
		self->compress_button = compress_button;
		gtk_combo_box_set_active (GTK_COMBO_BOX (self->format_combo), 0);
	}

	gtk_widget_show_all (self->dialog);
	gtk_widget_grab_focus (self->name_entry);

	/* Looping rather than a one-shot run: an empty name or a refused
	   overwrite should put the dialog back, not throw the settings away. */
	while (TRUE) {
		NemoArchiveOptions options;
		GFile *folder;
		GList *destinations;
		gboolean each;
		const char *name;

		if (gtk_dialog_run (GTK_DIALOG (self->dialog)) != GTK_RESPONSE_OK) {
			break;
		}

		each = compressing_each (self);

		name = gtk_entry_get_text (GTK_ENTRY (self->name_entry));
		if (!each && (name == NULL || name[0] == '\0')) {
			gtk_widget_grab_focus (self->name_entry);
			continue;
		}

		folder = gtk_file_chooser_get_file (GTK_FILE_CHOOSER (self->folder_button));
		if (folder == NULL) {
			continue;
		}

		if (each) {
			destinations = each_destinations (self, folder, current_format (self));
		} else {
			GFile *destination = g_file_get_child_for_display_name (folder, name, NULL);

			if (destination == NULL) {
				destination = g_file_get_child (folder, name);
			}
			destinations = g_list_append (NULL, destination);
		}

		if (destinations == NULL || !confirm_overwrite (self, destinations)) {
			g_list_free_full (destinations, g_object_unref);
			g_object_unref (folder);
			continue;
		}

		collect_options (self, &options);

		gtk_widget_hide (self->dialog);

		if (each) {
			nemo_archive_create_each (self->files, folder, &options,
						  parent_window, NULL, NULL);
		} else {
			nemo_archive_create (self->files, G_FILE (destinations->data), &options,
					     parent_window, NULL, NULL);
		}

		nemo_archive_options_clear (&options);
		g_list_free_full (destinations, g_object_unref);
		g_object_unref (folder);
		break;
	}

	gtk_widget_destroy (self->dialog);
	dialog_free (self);
}
