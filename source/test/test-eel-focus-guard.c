/* A column header or a notebook tab never holds the keyboard focus. Focus left
 * on a header after a click, or on a tab after the path entry closed, sent the
 * arrow keys somewhere other than the file list.
 *
 * The sidebar is the other side of that: it keeps the focus it was given, so
 * connecting a content view behind it must not grab.
 *
 * Needs a display, and the keyboard along with it; it skips without either. */

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

/* Ask the display for the keyboard and wait for it, up to five seconds. A
 * display that is not busy hands it over on the first look.
 *
 * Every Tab check below needs it. With another window holding the keyboard,
 * GtkContainer's focus handler re-grabs the widget that is already the window's
 * focus widget, because it looks at has-focus, which an unfocused toplevel
 * clears. It then reports that as a move, the traversal stops there, and the
 * check reads as a failure no key press could ever produce - a real Shift+Tab
 * goes to whichever window does hold the keyboard. */
static gboolean
take_keyboard (GtkWidget *window)
{
	int i;

	for (i = 0; i < 500 && !gtk_window_has_toplevel_focus (GTK_WINDOW (window)); i++) {
		/* With no window manager the first ask lands before the window is on
		 * screen and X drops it, so keep asking. */
		if (i % 10 == 0 && gtk_widget_get_mapped (window)) {
			gdk_window_focus (gtk_widget_get_window (window), GDK_CURRENT_TIME);
		}
		g_usleep (10000);
		pump ();
	}

	return gtk_window_has_toplevel_focus (GTK_WINDOW (window));
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
	GtkWidget *sidebar, *sidebar_row, *content;
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

	/* Stands in for the places pane and the view beside it. */
	sidebar_row = gtk_button_new_with_label ("place");
	sidebar = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (sidebar), sidebar_row, FALSE, FALSE, 0);
	content = gtk_button_new_with_label ("content");

	box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start (GTK_BOX (box), entry, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), notebook, TRUE, TRUE, 0);

	/* Last, so the tab order the checks below rely on runs entry to notebook. */
	gtk_box_pack_start (GTK_BOX (box), sidebar, FALSE, FALSE, 0);
	gtk_box_pack_start (GTK_BOX (box), content, FALSE, FALSE, 0);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size (GTK_WINDOW (window), 400, 300);
	gtk_container_add (GTK_CONTAINER (window), box);
	gtk_widget_show_all (window);
	pump ();

	if (!take_keyboard (window)) {
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
	if (!take_keyboard (window)) {
		g_print ("another window holds the keyboard; the rest is not checked\n");
		return failures == 0 ? 77 : 1;
	}
	gtk_widget_grab_focus (page_button);
	gtk_widget_child_focus (window, GTK_DIR_TAB_BACKWARD);

	/* Losing it between those two lines is the same thing arriving late. */
	if (gtk_window_get_focus (GTK_WINDOW (window)) != entry
	    && !gtk_window_has_toplevel_focus (GTK_WINDOW (window))) {
		g_print ("another window took the keyboard mid-check; the rest is not checked\n");
		return failures == 0 ? 77 : 1;
	}
	expect_focus (window, entry, "shift+tab out of the page");

	/* What connecting a content view does. It grabs only when the sidebar is
	 * not holding the focus, or clicking a place would lose the keyboard. */
	gtk_widget_grab_focus (sidebar_row);
	if (!eel_gtk_focus_is_within (sidebar)) {
		gtk_widget_grab_focus (content);
	}
	expect_focus (window, sidebar_row, "view connected behind the sidebar");

	gtk_widget_grab_focus (entry);
	if (!eel_gtk_focus_is_within (sidebar)) {
		gtk_widget_grab_focus (content);
	}
	expect_focus (window, content, "view connected with the focus elsewhere");

	gtk_widget_destroy (window);

	if (failures == 0) {
		g_print ("focus guard: all checks passed\n");
	}

	return failures == 0 ? 0 : 1;
}
