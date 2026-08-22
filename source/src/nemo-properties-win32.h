/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-properties-win32.h - the property sheet Windows itself shows.

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

#ifndef NEMO_PROPERTIES_WIN32_H
#define NEMO_PROPERTIES_WIN32_H

/* No windows.h here on purpose - this header is included from the plain
 * properties sources, which have identifiers that collide with its macros. */

#include <glib.h>
#include <gtk/gtk.h>

/* Show the shell property sheet for @files, parented on @parent_widget's
 * toplevel. FALSE means the selection has no Windows path behind it (a virtual
 * location, or items from more than one folder) and the caller should fall back
 * to our own window. */
gboolean nemo_properties_win32_show (GList     *files,
				     GtkWidget *parent_widget);

/* The same answer without the sheet: TRUE when @files is something the shell
 * can show one for. Split out so the fallback rule can be tested unattended. */
gboolean nemo_properties_win32_can_show (GList *files);

#endif /* NEMO_PROPERTIES_WIN32_H */
