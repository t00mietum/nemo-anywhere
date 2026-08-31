/* nemo-launch-win32.h - starting a program that is not ours
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef NEMO_LAUNCH_WIN32_H
#define NEMO_LAUNCH_WIN32_H

#include <glib.h>

/* Default action on a path, the way a double-click in the shell would do it. */
gboolean nemo_launch_win32_open_path   (const gchar  *path,
					const gchar  *workdir,
					GError      **error);

/* A named program. @args is a command tail the program parses itself, or NULL. */
gboolean nemo_launch_win32_run         (const gchar  *exe,
					const gchar  *args,
					const gchar  *workdir,
					GError      **error);

/* Same, from a whole command line with the program on the front. */
gboolean nemo_launch_win32_run_command (const gchar  *command_line,
					const gchar  *workdir,
					GError      **error);

/* The program at the front of a command line; *@args gets the rest, or NULL. */
gchar   *nemo_launch_win32_split_command (const gchar  *command_line,
					  gchar       **args);

/* The two brokers on their own. Exposed so a probe can tell which of them the
 * box actually allows, rather than watching one cover for the other. */
gboolean nemo_launch_win32_via_shell   (const gchar *exe,
					const gchar *args,
					const gchar *workdir);
gboolean nemo_launch_win32_via_service (const gchar *command_line,
					const gchar *workdir);

#endif /* NEMO_LAUNCH_WIN32_H */
