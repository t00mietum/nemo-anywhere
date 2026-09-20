/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-column-layout.h - how the list view divides its width between columns.

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

/* Every column has three widths: the least it will ever be, the width it has
 * when the row has room, and the most it grows to. design.md's "List view
 * column widths" is the rule that sets them, and the only place it is written
 * down - restating it here would give two versions to keep in step. This file
 * only shares the row out between the widths it is given.
 *
 * Nothing but arithmetic lives here, so the rule can be checked without a
 * screen.
 */

#ifndef NEMO_COLUMN_LAYOUT_H
#define NEMO_COLUMN_LAYOUT_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	int      min_width;	/* never narrower than this */
	int      fit_width;	/* the default, when the row has room */
	int      max_width;	/* the most it grows to before the surplus is shared out */
	gboolean grows;		/* takes a share of what is left past every max */
} NemoColumnLayoutItem;

/* Writes n_items widths. They sum to available whenever the minimums allow it;
 * when they do not, each column is at its minimum and the row overflows. */
void nemo_column_layout_distribute (const NemoColumnLayoutItem *items,
				    int                         n_items,
				    int                         available,
				    int                        *widths);

/* The width that shows the narrowest `percent` of the values given: the
 * max (1, floor (count * percent)) th smallest. 100 is the widest; 0 values
 * is 0. */
int nemo_column_layout_fit (const int *values,
			    int        n_values,
			    int        percent);

G_END_DECLS

#endif /* NEMO_COLUMN_LAYOUT_H */
