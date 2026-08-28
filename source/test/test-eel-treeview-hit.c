/* Dragging a file over the empty space below the last row crashed the app.
 * gtk_tree_view_is_blank_at_pos answers no when the position is past every row,
 * and leaves the path it was asked for untouched - so an uninitialized pointer
 * was read and then freed. */

#include <config.h>

#include <gtk/gtk.h>
#include <eel/eel-gtk-extensions.h>

int
main (int argc, char *argv[])
{
	GtkWidget    *window, *tree_view;
	GtkListStore *store;
	GtkTreeIter   iter;
	int           failures = 0;

	if (!gtk_init_check (&argc, &argv)) {
		g_print ("no display; skipping\n");
		return 77;
	}

	store = gtk_list_store_new (1, G_TYPE_STRING);

	for (int i = 0; i < 3; i++) {
		char *name = g_strdup_printf ("row %d", i);

		gtk_list_store_append (store, &iter);
		gtk_list_store_set (store, &iter, 0, name, -1);
		g_free (name);
	}

	tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view),
				     gtk_tree_view_column_new_with_attributes ("Name",
									       gtk_cell_renderer_text_new (),
									       "text", 0,
									       NULL));

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
	gtk_container_add (GTK_CONTAINER (window), tree_view);
	gtk_widget_show_all (window);

	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}

	/* Well below the third row, and inside the view. */
	if (eel_gtk_get_treeview_row_text_at_pos (GTK_TREE_VIEW (tree_view), 40, 250)) {
		g_printerr ("FAIL: empty space below the rows reads as a row's text\n");
		failures++;
	}

	/* Over the first row's text, which still has to answer yes. */
	if (!eel_gtk_get_treeview_row_text_at_pos (GTK_TREE_VIEW (tree_view), 10, 10)) {
		g_printerr ("FAIL: the first row's text does not read as a row's text\n");
		failures++;
	}

	gtk_widget_destroy (window);
	g_object_unref (store);

	if (failures > 0) {
		return 1;
	}

	g_print ("eel-treeview-hit: all checks passed\n");
	return 0;
}
