/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-column-layout.h - how the list view divides its width between columns.

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

/* The list view fills its width exactly: no column is pushed off the end of the
 * window and no strip of empty space is left after the last one. Nothing but
 * arithmetic lives here, so the rule can be checked without a screen.
 *
 * Widening, from narrow to wide: every column takes the new space equally until
 * it can show the longest value in it, and then that one stops while the rest
 * carry on. Name is the only column with no such stop, so once everything else
 * has what it needs the remainder is all Name's.
 *
 * Location, when it is on screen, grows alongside Name instead of stopping. The
 * two split whatever the other columns leave, Name taking no more than half, so
 * Location is never the narrower of the pair and everything Name does not need
 * for its longest name goes to Location.
 *
 * Narrowing, which is the same continuum read the other way: the growing pair
 * gives its surplus back first, since it had all of it. When every column is
 * down to the longest value it holds and it still does not fit, one nominated
 * column - Type, in the view - gives next, on its own, down to a floor of about
 * three characters. After that the elastic columns give together, each in
 * proportion to how much it has to give, and only when they are all on their
 * floors does anything else move. That last part is what keeps a date or a size
 * whole: a truncated date says nothing, where a truncated name still reads.
 *
 * A column whose values have no natural limit - Type, Owner - would otherwise
 * take half the window for one long value, so it stops at a third of whatever
 * is growing, unless its own floor is wider than that. That is Name on its own,
 * or Name and Location together, so turning Location on does not squeeze them.
 */

#ifndef NEMO_COLUMN_LAYOUT_H
#define NEMO_COLUMN_LAYOUT_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	int      floor_width;	/* never narrower than this */
	int      natural_width;	/* what it takes to show the longest value in it */
	gboolean unbounded;	/* values have no natural limit; capped against name */
	gboolean is_name;	/* the one column that keeps growing */
	gboolean shares_growth;	/* grows with name and takes what name does not need */
	gboolean elastic;	/* gives width back before the columns that stay whole */
} NemoColumnLayoutItem;

/* Writes n_items widths, summing to available wherever the floors allow it.
 * shrink_first is the index of the column that gives before the others, or -1
 * when that column is not on screen. */
void nemo_column_layout_distribute (const NemoColumnLayoutItem *items,
				    int                         n_items,
				    int                         shrink_first,
				    int                         available,
				    int                        *widths);

G_END_DECLS

#endif /* NEMO_COLUMN_LAYOUT_H */
