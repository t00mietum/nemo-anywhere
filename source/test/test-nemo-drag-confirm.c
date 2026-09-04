/* The rule behind the drop confirmation: which action, and which setting,
 * and the trash exception. The dialog itself is not driven here - only the
 * decision that puts it up. */

#include <config.h>

#include <gtk/gtk.h>
#include <glib/gstdio.h>

#include <libnemo-private/nemo-config.h>
#include <libnemo-private/nemo-dnd.h>
#include <libnemo-private/nemo-global-preferences.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

static void
set_prefs (gboolean move, gboolean copy)
{
	nemo_config_set_boolean (nemo_preferences, NEMO_PREFERENCES_CONFIRM_DRAG_MOVE, move);
	nemo_config_set_boolean (nemo_preferences, NEMO_PREFERENCES_CONFIRM_DRAG_COPY, copy);
}

int
main (int argc, char *argv[])
{
	char *tmp;

	tmp = g_dir_make_tmp ("nemo-drag-confirm-test-XXXXXX", NULL);
	g_setenv ("XDG_CONFIG_HOME", tmp, TRUE);
	g_setenv ("APPDATA", tmp, TRUE);
	g_setenv ("HOME", tmp, TRUE);

	gtk_init (&argc, &argv);
	nemo_global_preferences_init ();

	/* Defaults: a move asks, a copy does not. */
	check (nemo_drag_confirm_needed (GDK_ACTION_MOVE, "file:///tmp"));
	check (!nemo_drag_confirm_needed (GDK_ACTION_COPY, "file:///tmp"));
	check (!nemo_drag_confirm_needed (GDK_ACTION_LINK, "file:///tmp"));

	set_prefs (TRUE, TRUE);
	check (nemo_drag_confirm_needed (GDK_ACTION_MOVE, "file:///tmp"));
	check (nemo_drag_confirm_needed (GDK_ACTION_COPY, "file:///tmp"));
	check (nemo_drag_confirm_needed (GDK_ACTION_LINK, "file:///tmp"));
	/* A drop with no resolved action ends up as a link, so it follows copy. */
	check (nemo_drag_confirm_needed (GDK_ACTION_DEFAULT, "file:///tmp"));

	set_prefs (FALSE, FALSE);
	check (!nemo_drag_confirm_needed (GDK_ACTION_MOVE, "file:///tmp"));
	check (!nemo_drag_confirm_needed (GDK_ACTION_COPY, "file:///tmp"));

	/* Each setting answers only for its own action. */
	set_prefs (TRUE, FALSE);
	check (nemo_drag_confirm_needed (GDK_ACTION_MOVE, "file:///tmp"));
	check (!nemo_drag_confirm_needed (GDK_ACTION_COPY, "file:///tmp"));
	set_prefs (FALSE, TRUE);
	check (!nemo_drag_confirm_needed (GDK_ACTION_MOVE, "file:///tmp"));
	check (nemo_drag_confirm_needed (GDK_ACTION_COPY, "file:///tmp"));

	/* The trash asks for itself, whatever these say. */
	set_prefs (TRUE, TRUE);
	check (!nemo_drag_confirm_needed (GDK_ACTION_MOVE, "trash:///"));
	check (!nemo_drag_confirm_needed (GDK_ACTION_COPY, "trash:///"));

	/* Nothing to ask about for an action no drop performs. */
	check (!nemo_drag_confirm_needed (GDK_ACTION_PRIVATE, "file:///tmp"));
	check (!nemo_drag_confirm_needed (GDK_ACTION_ASK, "file:///tmp"));

	/* An empty drop goes through without a dialog, so this cannot block. */
	check (nemo_drag_confirm_drop (NULL, GDK_ACTION_MOVE, NULL, "file:///tmp"));
	/* Same for one the settings say not to ask about. */
	set_prefs (FALSE, FALSE);
	{
		GList *uris = g_list_append (NULL, (gpointer) "file:///tmp/a");
		check (nemo_drag_confirm_drop (NULL, GDK_ACTION_MOVE, uris, "file:///tmp"));
		g_list_free (uris);
	}

	nemo_config_shutdown ();
	g_free (tmp);

	if (failures == 0)
		g_print ("nemo-drag-confirm: all checks passed\n");

	return failures == 0 ? 0 : 1;
}
