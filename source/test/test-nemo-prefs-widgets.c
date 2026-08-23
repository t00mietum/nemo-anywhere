/* The preferences dialog is built from a resource and wired by widget name, so
 * a renamed or mistyped id only shows up when someone opens the dialog. This
 * loads the same resource and checks the ids the code asks for by name. */

#include <config.h>

#include <stdlib.h>
#include <gtk/gtk.h>

static int failures = 0;

static const char * const widget_ids[] = {
	"path_separator_combobox",
	"allow_slash_input_checkbutton",
	"vbox_paths",
	"date_format_combobox",
	"show_full_path_in_title_bars_checkbutton",
	NULL
};

int
main (int argc, char *argv[])
{
	GtkBuilder *builder;
	GError *error = NULL;
	int i;

	gtk_init (&argc, &argv);

	builder = gtk_builder_new ();
	if (!gtk_builder_add_from_resource (builder,
					    "/org/nemo/nemo-file-management-properties.glade",
					    &error)) {
		g_printerr ("FAIL cannot load the dialog: %s\n", error->message);
		g_clear_error (&error);
		return EXIT_FAILURE;
	}

	for (i = 0; widget_ids[i] != NULL; i++) {
		if (gtk_builder_get_object (builder, widget_ids[i]) == NULL) {
			g_printerr ("FAIL no widget named %s\n", widget_ids[i]);
			failures++;
		}
	}

	g_object_unref (builder);

	if (failures > 0) {
		g_printerr ("%d failure(s)\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("preferences widgets: all checks passed\n");
	return EXIT_SUCCESS;
}
