/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-column-layout.c - how the list view divides its width between columns.

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

#include <config.h>

#include "nemo-column-layout.h"

#include <stdlib.h>
#include <string.h>

/* Move `amount` pixels from the columns that have room to move, each in
   proportion to its fit width - a wide column moves the most, since it has the
   most to give or the most to show. `limits` is where each stops (max going
   up, min going down). Repeats, because a column that reaches its limit part
   way through leaves its share for the others. Returns what could not be
   moved. */
static int
move_proportionally (const NemoColumnLayoutItem *items,
		     int                        *widths,
		     const int                  *limits,
		     int                         n_items,
		     int                         direction,
		     int                         amount)
{
	while (amount > 0) {
		gint64 weight = 0;
		int moved = 0;
		int biggest = -1;
		int i;

		for (i = 0; i < n_items; i++) {
			if (direction * (limits[i] - widths[i]) > 0) {
				weight += MAX (1, items[i].fit_width);
			}
		}

		if (weight <= 0) {
			return amount;
		}

		for (i = 0; i < n_items; i++) {
			int room = direction * (limits[i] - widths[i]);
			int share;

			if (room <= 0) {
				continue;
			}

			/* Rounded down, so a pass never moves more than asked; what
			   rounding leaves goes to the biggest column below. */
			share = (int) (((gint64) amount * MAX (1, items[i].fit_width)) / weight);
			share = MIN (share, room);

			widths[i] += direction * share;
			moved += share;

			if (biggest < 0 || items[i].fit_width > items[biggest].fit_width) {
				biggest = i;
			}
		}

		amount -= moved;

		if (moved == 0) {
			if (biggest < 0 || direction * (limits[biggest] - widths[biggest]) <= 0) {
				return amount;
			}

			widths[biggest] += direction;
			amount -= 1;
		}
	}

	return 0;
}

void
nemo_column_layout_distribute (const NemoColumnLayoutItem *items,
			       int                         n_items,
			       int                         available,
			       int                        *widths)
{
	int *limits;
	int grower = -1;
	int sum_fit = 0;
	int i;

	g_return_if_fail (items != NULL);
	g_return_if_fail (widths != NULL);

	if (n_items <= 0) {
		return;
	}

	limits = g_new0 (int, n_items);

	for (i = 0; i < n_items; i++) {
		int min = MAX (1, items[i].min_width);

		/* The three have to be in order; where they are not, the larger
		   wins, since a column cannot be asked to show less than its
		   minimum. */
		widths[i] = MAX (min, items[i].fit_width);
		sum_fit += widths[i];

		if (items[i].shares_growth && grower < 0) {
			grower = i;
		}
	}

	if (grower < 0) {
		for (i = 0; i < n_items; i++) {
			if (items[i].is_name) {
				grower = i;
				break;
			}
		}
	}

	if (available >= sum_fit) {
		int surplus;

		for (i = 0; i < n_items; i++) {
			limits[i] = MAX (widths[i], items[i].max_width);
		}

		surplus = move_proportionally (items, widths, limits, n_items, 1,
					       available - sum_fit);

		if (surplus > 0 && grower >= 0) {
			widths[grower] += surplus;
		}
	} else {
		for (i = 0; i < n_items; i++) {
			limits[i] = MIN (widths[i], MAX (1, items[i].min_width));
		}

		/* Whatever cannot be given back is the overflow, and the view
		   scrolls sideways for it. */
		move_proportionally (items, widths, limits, n_items, -1,
				     sum_fit - available);
	}

	g_free (limits);
}

static int
compare_ints (const void *a,
	      const void *b)
{
	int x = *(const int *) a;
	int y = *(const int *) b;

	return (x > y) - (x < y);
}

int
nemo_column_layout_fit (const int *values,
			int        n_values,
			int        percent)
{
	int *sorted;
	int rank;
	int fit;

	if (values == NULL || n_values <= 0) {
		return 0;
	}

	percent = CLAMP (percent, 1, 100);

	sorted = g_new (int, n_values);
	memcpy (sorted, values, n_values * sizeof (int));
	qsort (sorted, n_values, sizeof (int), compare_ints);

	/* Rounded up, so a share of a handful of values is still a whole one:
	   90 percent of three values is all three. */
	rank = (n_values * percent + 99) / 100;
	rank = CLAMP (rank, 1, n_values);

	fit = sorted[rank - 1];
	g_free (sorted);

	return fit;
}

void
nemo_column_layout_search_pair (int  name_floor,
				int  name_natural,
				int  where_floor,
				int  where_natural,
				int  available,
				int *name_width,
				int *where_width)
{
	int wants_name;
	int wants_where;
	int total;
	int name;
	int where;

	g_return_if_fail (name_width != NULL);
	g_return_if_fail (where_width != NULL);

	name_floor = MAX (1, name_floor);
	where_floor = MAX (1, where_floor);
	wants_name = MAX (name_floor, name_natural);
	wants_where = MAX (where_floor, where_natural);
	total = wants_name + wants_where;

	if (available >= total) {
		*name_width = wants_name;
		*where_width = wants_where;
		return;
	}

	available = MAX (available, 0);
	name = (int) (((gint64) available * wants_name) / total);
	where = available - name;

	/* The narrower of the two stays readable: a third of the room, or all it
	   asked for if that is less, in which case the other keeps the difference. */
	if (name > 2 * where) {
		where = MIN (available / 3, wants_where);
		name = available - where;
	} else if (where > 2 * name) {
		name = MIN (available / 3, wants_name);
		where = available - name;
	}

	*name_width = MAX (name_floor, name);
	*where_width = MAX (where_floor, where);
}
