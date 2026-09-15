/* A column header or a notebook tab never holds the keyboard focus. Focus left
 * on a header after a click, or on a tab after the path entry closed, sent the
 * arrow keys somewhere other than the file list. */

#include <config.h>

#include <gtk/gtk.h>
#include <eel/eel-gtk-extensions.h>

static int failures = 0;

static void
pump (void)
{
	while (gtk_events_pending ()) {
		gtk_main_iteration ();
	}
}

static void
expect_focus (GtkWidget *window, GtkWidget *expected, const char *what)
{
	GtkWidget *focus = gtk_window_get_focus (GTK_WINDOW (window));

	if (focus != expected) {
		g_print ("FAIL: %s: focus is on %s\n", what,
			 focus != NULL ? G_OBJECT_TYPE_NAME (focus) : "nothing");
		failures++;
	}
}

static GtkWidget *
list_with_columns (GtkWidget **buttons)
{
	GtkListStore *store = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_STRING);
	GtkTreeIter   iter;
	GtkWidget    *tree_view;

	gtk_list_store_append (store, &iter);
	gtk_list_store_set (store, &iter, 0, "a", 1, "b", -1);
	tree_view = gtk_tree_view_new_with_model (GTK_TREE_MODEL (store));
	g_object_unref (store);

	for (int i = 0; i < 2; i++) {
		GtkTreeViewColumn *column;

		column = gtk_tree_view_column_new_with_attributes ("Column", gtk_cell_renderer_text_new (),
								  "text", i, NULL);
		gtk_tree_view_append_column (GTK_TREE_VIEW (tree_view), column);
		gtk_tree_view_column_set_sort_column_id (column, i);
		gtk_tree_view_column_set_reorderable (column, TRUE);
		buttons[i] = gtk_tree_view_column_get_button (column);
		eel_gtk_widget_refuse_focus (buttons[i]);

		/* Each of these turns can-focus back on inside GTK. */
		gtk_tree_view_column_set_sort_indicator (column, TRUE);
		gtk_tree_view_column_set_sort_order (column, GTK_SORT_DESCENDING);
		gtk_tree_view_column_set_title (column, "Renamed");
	}

	return tree_view;
}

int
main (int argc, char *argv[])
{
	GtkWidget *window, *box, *entry, *notebook, *tree_view, *page_button;
	GtkWidget *buttons[2];

	if (!gtk_init_check (&argc, &argv)) {
		g_print ("no display; skipping\n");
		return 77;
	}

	tree_view = list_with_columns (buttons);

	notebook = gtk_notebook_new ();
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), tree_view, gtk_label_new ("one"));
	page_button = gtk_button_new_with_label ("page");
	gtk_notebook_append_page (GTK_NOTEBOOK (notebook), page_button, gtk_label_new ("two"));
	eel_gtk_notebook_keep_focus_off_tabs (GTK_NOTEBOOK (notebook));

	entry = gtk_entry_new ();
	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (box), entry, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), notebook, TRUE, TRUE, 0);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
	gtk_container_add (GTK_CONTAINER (window), box);
	gtk_widget_show_all (window);
	pump ();

	/* Without the window's own focus a focused widget grabs again rather than
	   pass Shift+Tab on, so nothing would ever leave it. */
	gdk_window_focus (gtk_widget_get_window (window), GDK_CURRENT_TIME);
	for (int i = 0; i < 50 && !gtk_window_has_toplevel_focus (GTK_WINDOW (window)); i++) {
		g_usleep (10000);
		pump ();
	}
	if (!gtk_window_has_toplevel_focus (GTK_WINDOW (window))) {
		g_print ("the window could not take the focus; skipping\n");
		return 77;
	}

	for (int i = 0; i < 2; i++) {
		if (gtk_widget_get_can_focus (buttons[i])) {
			g_print ("FAIL: header %d can take focus again after the column changed\n", i);
			failures++;
		}
	}

	/* What a click on a header does. */
	gtk_widget_grab_focus (tree_view);
	gtk_widget_grab_focus (buttons[0]);
	expect_focus (window, tree_view, "grab on a header");

	gtk_widget_grab_focus (entry);
	gtk_widget_child_focus (window, GTK_DIR_TAB_FORWARD);
	expect_focus (window, tree_view, "tab from the entry");

	/* What a click on a tab does. */
	gtk_widget_grab_focus (entry);
	gtk_widget_grab_focus (notebook);
	expect_focus (window, tree_view, "grab on the notebook");

	gtk_widget_grab_focus (tree_view);
	gtk_widget_grab_focus (notebook);
	expect_focus (window, tree_view, "grab on the notebook from its own page");

	/* A tree view keeps Shift+Tab to itself, so leave from a plain page. */
	gtk_notebook_set_current_page (GTK_NOTEBOOK (notebook), 1);
	pump ();
	gtk_widget_grab_focus (page_button);
	gtk_widget_child_focus (window, GTK_DIR_TAB_BACKWARD);
	expect_focus (window, entry, "shift+tab out of the page");

	gtk_widget_destroy (window);

	if (failures == 0) {
		g_print ("focus guard: all checks passed\n");
	}

	return failures == 0 ? 0 : 1;
}
