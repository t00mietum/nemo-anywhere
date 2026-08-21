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

#include <string.h>

/* A column with no natural limit stops here, as a fraction of the Name column.
   One third leaves Name comfortably the widest thing on the row while still
   letting a long type or location say something useful. */
#define UNBOUNDED_SHARE 3

/* Every column can show the longest value in it, and Name has the rest. */
static void
give_everyone_their_target (const int *targets,
			    int        n_items,
			    int        name_index,
			    int        surplus,
			    int       *widths)
{
	memcpy (widths, targets, n_items * sizeof (int));

	if (surplus > 0 && name_index >= 0) {
		widths[name_index] += surplus;
	}
}

/* Take `deficit` back off everything that still has room above its floor, each
   in proportion to how wide it is - so the widest column, which has the most to
   spare, gives the most. Repeats, because a column that reaches its floor part
   way through leaves its share for the others to pick up. `skip` is the column
   already reduced on its own, or -1. */
static void
take_proportionally (int       *widths,
		     const int *floors,
		     int        n_items,
		     int        skip,
		     int        deficit)
{
	while (deficit > 0) {
		gint64 eligible_width = 0;
		int given = 0;
		int widest = -1;
		int i;

		for (i = 0; i < n_items; i++) {
			if (i != skip && widths[i] > floors[i]) {
				eligible_width += widths[i];
			}
		}

		if (eligible_width <= 0) {
			/* Everything is on its floor. What is left cannot be given
			   back, and the view scrolls sideways instead - the honest
			   answer to a window narrower than its own contents. */
			return;
		}

		for (i = 0; i < n_items; i++) {
			int room, share;

			if (i == skip) {
				continue;
			}

			room = widths[i] - floors[i];
			if (room <= 0) {
				continue;
			}

			/* Rounded down, so a pass never takes more than the deficit;
			   the remainder comes off the widest column below. */
			share = (int) (((gint64) deficit * widths[i]) / eligible_width);
			share = MIN (share, room);

			widths[i] -= share;
			given += share;

			if (widest < 0 || widths[i] > widths[widest]) {
				widest = i;
			}
		}

		deficit -= given;

		/* Rounding can leave a pixel or two that no proportional share
		   claims. The widest column notices it least. */
		if (given == 0) {
			if (widest < 0 || widths[widest] <= floors[widest]) {
				return;
			}

			widths[widest] -= 1;
			deficit -= 1;
		}
	}
}

/* What Name would end up with if the columns with no natural limit stopped at
   `cap`, divided by the share they are allowed. Falls as `cap` rises - every
   pixel a capped column takes is one Name does not get - so there is exactly one
   width where a cap equals the share it implies, and that is the one wanted. */
static int
share_implied_by (const NemoColumnLayoutItem *items,
		  const int                  *targets,
		  const int                  *floors,
		  int                         n_items,
		  int                         name_index,
		  int                         available,
		  int                         cap)
{
	int others = 0;
	int i;

	for (i = 0; i < n_items; i++) {
		if (i == name_index) {
			continue;
		}

		others += items[i].unbounded
			? MAX (floors[i], MIN (targets[i], cap))
			: targets[i];
	}

	return MAX (targets[name_index], available - others) / UNBOUNDED_SHARE;
}

void
nemo_column_layout_distribute (const NemoColumnLayoutItem *items,
			       int                         n_items,
			       int                         shrink_first,
			       int                         available,
			       int                        *widths)
{
	int *targets;
	int *floors;
	int name_index = -1;
	int sum_targets = 0;
	int deficit;
	int i;

	g_return_if_fail (items != NULL);
	g_return_if_fail (widths != NULL);

	if (n_items <= 0) {
		return;
	}

	targets = g_new0 (int, n_items);
	floors = g_new0 (int, n_items);

	for (i = 0; i < n_items; i++) {
		floors[i] = MAX (1, items[i].floor_width);
		/* A column that cannot show its longest value still cannot go
		   below its floor, so the floor wins where the two disagree. */
		targets[i] = MAX (floors[i], items[i].natural_width);

		if (items[i].is_name && name_index < 0) {
			name_index = i;
		}
	}

	if (shrink_first < 0 || shrink_first >= n_items || shrink_first == name_index) {
		shrink_first = -1;
	}

	/* The cap on a column with no natural limit is a share of Name - but Name's
	   own width depends on what the capped columns leave it, so the two have to
	   agree. Found by halving the range rather than guessed at, which is a dozen
	   passes over a handful of columns and settles at one answer whatever order
	   the columns are in. */
	if (name_index >= 0) {
		int lo = 0;
		int hi = MAX (0, available);

		while (lo < hi) {
			int mid = (lo + hi + 1) / 2;

			if (mid <= share_implied_by (items, targets, floors, n_items,
						     name_index, available, mid)) {
				lo = mid;
			} else {
				hi = mid - 1;
			}
		}

		for (i = 0; i < n_items; i++) {
			if (i != name_index && items[i].unbounded) {
				targets[i] = MAX (floors[i], MIN (targets[i], lo));
			}
		}
	}

	for (i = 0; i < n_items; i++) {
		sum_targets += targets[i];
	}

	if (available >= sum_targets) {
		give_everyone_their_target (targets, n_items, name_index,
					    available - sum_targets, widths);
		g_free (targets);
		g_free (floors);
		return;
	}

	memcpy (widths, targets, n_items * sizeof (int));
	deficit = sum_targets - available;

	/* The nominated column gives first, alone, down to its floor. */
	if (shrink_first >= 0) {
		int slack = widths[shrink_first] - floors[shrink_first];
		int take = MIN (MAX (slack, 0), deficit);

		widths[shrink_first] -= take;
		deficit -= take;
	}

	if (deficit > 0) {
		take_proportionally (widths, floors, n_items, shrink_first, deficit);
	}

	g_free (targets);
	g_free (floors);
}
