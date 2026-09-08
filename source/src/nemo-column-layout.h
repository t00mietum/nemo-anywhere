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

/* Every column has three widths: the least it will ever be, the width that
 * shows most of what is in it, and the width that shows all of it. For a date
 * or a size the three are the same number - a value like that says nothing cut
 * short, so it is never cut. Name and the columns with no natural length (a
 * type, an owner, a path) have a spread: they show every value when the row has
 * room, most of them when it does not, and Type alone goes a little further.
 *
 * Widening, from narrow to wide: once every column has the width that shows
 * most of its values, the columns that can still grow do so together, each in
 * proportion to its size, until each can show everything. What is left after
 * that is Name's - or Location's, when it is on the row and growing alongside.
 *
 * Narrowing: the same thing read the other way, until every column is at the
 * width that shows most of its values. Below that only the columns with a
 * smaller minimum give (Type), in proportion to their size, and when they are
 * spent the row is wider than the window and the view scrolls sideways. That is
 * deliberate: a Name column crushed to nothing tells the user less than a
 * scrollbar does.
 *
 * Search results are the one exception, and have their own rule at the bottom.
 * Nothing but arithmetic lives here, so the rule can be checked without a
 * screen.
 */

#ifndef NEMO_COLUMN_LAYOUT_H
#define NEMO_COLUMN_LAYOUT_H

#include <glib.h>

G_BEGIN_DECLS

typedef struct {
	int      min_width;	/* never narrower than this */
	int      fit_width;	/* shows most of its values; where it stops when the row runs short */
	int      max_width;	/* shows every value in it */
	gboolean is_name;	/* the column the surplus goes to */
	gboolean shares_growth;	/* takes the surplus instead of Name, when present */
} NemoColumnLayoutItem;

/* Writes n_items widths. They sum to available whenever the minimums allow it;
 * when they do not, each column is at its minimum and the row overflows. */
void nemo_column_layout_distribute (const NemoColumnLayoutItem *items,
				    int                         n_items,
				    int                         available,
				    int                        *widths);

/* The width that shows `percent` of the values given - the smallest value that
 * is at least as wide as that share of them. 100 is the widest; 0 values is 0. */
int nemo_column_layout_fit (const int *values,
			    int        n_values,
			    int        percent);

/* Search results divide the row differently, since the other columns there are
 * already at the width their contents ask for: Name and Location take what they
 * need out of `available` and no more, leaving the rest of the row empty. Only
 * when the two together do not fit does either give, and then both give in
 * proportion to what they asked for - except that neither ends up more than
 * twice the width of the other, unless the narrower one did not want the extra.
 * Neither goes under its floor, so the pair can still overflow the row. */
void nemo_column_layout_search_pair (int  name_floor,
				     int  name_natural,
				     int  where_floor,
				     int  where_natural,
				     int  available,
				     int *name_width,
				     int *where_width);

G_END_DECLS

#endif /* NEMO_COLUMN_LAYOUT_H */
