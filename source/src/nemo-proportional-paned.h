/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-proportional-paned.h - a divider that keeps its share of the width.

   Copyright © 2026 t00mietum (CryptogID: ปʬϝღถɔ4რఠΔթะ9ƾǝu).

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

/* A GtkPaned whose divider keeps its share of the width when the paned is
 * resized, using nemo_pane_layout_scale_position.
 *
 * It has to be a subclass. A handler on size-allocate runs after GtkPaned has
 * already placed its children, and a position set from there only queues a
 * resize that GTK drops, so the divider never moved. Setting it before chaining
 * up lets the one allocation use it.
 *
 * GtkPaned's own scaling, which it does when both children are resizable,
 * works from the last position each time and truncates, so a slow drag of a
 * pixel per step never moves the divider at all. This one measures every
 * resize against where the divider was last placed, so small steps add up.
 *
 * Pack child1 with resize off. With it on as well as child2's, GTK scales the
 * position again on top of this.
 *
 * "placed" (int position) fires when the divider moves at an unchanged width,
 * which is a drag. Save a remembered width from it, not from size-allocate. */

#ifndef NEMO_PROPORTIONAL_PANED_H
#define NEMO_PROPORTIONAL_PANED_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define NEMO_TYPE_PROPORTIONAL_PANED (nemo_proportional_paned_get_type ())

G_DECLARE_FINAL_TYPE (NemoProportionalPaned, nemo_proportional_paned, NEMO, PROPORTIONAL_PANED, GtkPaned)

GtkWidget *nemo_proportional_paned_new (void);

G_END_DECLS

#endif /* NEMO_PROPORTIONAL_PANED_H */
