/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-archive-dialog.h - ask what archive to make, then make it.

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

#ifndef NEMO_ARCHIVE_DIALOG_H
#define NEMO_ARCHIVE_DIALOG_H

#include <gtk/gtk.h>
#include <gio/gio.h>

/* files are GFile *; default_dir is where the archive is offered to be put,
   normally the folder being viewed. Runs the job itself when accepted. */
void nemo_archive_dialog_show (GtkWindow *parent_window,
			       GList     *files,
			       GFile     *default_dir);

#endif /* NEMO_ARCHIVE_DIALOG_H */
