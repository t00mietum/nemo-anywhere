/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-pane-layout.h - where a divider lands when the window changes width.

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

/* GtkPaned gives new space to whichever children are marked resizable, and
 * splits it evenly between them. The window wants the other rule: a pane that
 * grows should keep the share of the width it already had, so a tree view
 * sitting at a third of the window is still at a third once the window is
 * wider. Only one divider moves at a time, so this is the arithmetic for one.
 *
 * The floor stops either side being squeezed out of existence on a narrow
 * window. A pane one pixel wide is worse than a slightly wrong ratio.
 *
 * Nothing here touches a widget, so the rule can be checked without a screen.
 */

#ifndef NEMO_PANE_LAYOUT_H
#define NEMO_PANE_LAYOUT_H

#include <glib.h>

G_BEGIN_DECLS

/* The divider position that keeps the share of the width it had before.
 * `position` is where it sits while the paned is `old_width` wide, and the
 * answer is where it belongs once that paned is `new_width` wide. Neither side
 * comes out narrower than `floor_width`, and a paned with no room to give both
 * that much is split down the middle. A width of zero or less has no proportion
 * to keep, so the position comes back untouched. */
int nemo_pane_layout_scale_position (int position,
				     int old_width,
				     int new_width,
				     int floor_width);

G_END_DECLS

#endif /* NEMO_PANE_LAYOUT_H */
