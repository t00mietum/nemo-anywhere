/* nemo-shortcut-properties.h - editing a Windows shortcut from Properties
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#ifndef NEMO_SHORTCUT_PROPERTIES_H
#define NEMO_SHORTCUT_PROPERTIES_H

#include <gtk/gtk.h>

/* The .lnk counterpart of the .desktop launcher editor: a box of fields for
 * the target, its arguments, the folder it starts in and the comment. */
GtkWidget *nemo_shortcut_properties_make_box    (GtkSizeGroup *label_size_group,
						 GList        *files);
gboolean   nemo_shortcut_properties_should_show (GList        *files);

#endif /* NEMO_SHORTCUT_PROPERTIES_H */
