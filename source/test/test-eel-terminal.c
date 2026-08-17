/* With no terminal emulator anywhere on PATH, opening a terminal must
 * decline cleanly instead of crashing on a NULL prefix. */

#include <config.h>

#include <gtk/gtk.h>
#include <eel/eel-gnome-extensions.h>

int
main (int argc, char *argv[])
{
	g_setenv ("PATH", "/nonexistent", TRUE);
	/* also hide any desktop schema that names a terminal - the box this
	 * runs on may have one installed */
	g_setenv ("XDG_DATA_DIRS", "/nonexistent", TRUE);
	g_setenv ("GSETTINGS_SCHEMA_DIR", "/nonexistent", TRUE);

	gtk_init_check (&argc, &argv);

	eel_gnome_open_terminal_on_screen ("true", NULL);

	g_print ("eel-terminal: all checks passed\n");
	return 0;
}
