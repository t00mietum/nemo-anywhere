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

/* A column with no natural limit stops here, as a fraction of the growing part
   of the row - Name, or Name and Location together. One third leaves the growing
   part comfortably the widest thing on the row while still letting a long type
   or owner say something useful. Measuring it against the pair rather than
   against Name alone is what keeps turning Location on from squeezing them. */
#define UNBOUNDED_SHARE 3

/* Every column can show the longest value in it, and the growing column has
   the rest. */
static void
give_everyone_their_target (const int *targets,
			    int        n_items,
			    int        grower_index,
			    int        surplus,
			    int       *widths)
{
	memcpy (widths, targets, n_items * sizeof (int));

	if (surplus > 0 && grower_index >= 0) {
		widths[grower_index] += surplus;
	}
}

/* Take `deficit` back off everything that still has room above its floor, each
   in proportion to how wide it is - so the widest column, which has the most to
   spare, gives the most. Repeats, because a column that reaches its floor part
   way through leaves its share for the others to pick up. `skip` is the column
   already reduced on its own, or -1. With `elastic_only`, the columns meant to
   stay whole are left out of it. Returns what could not be taken. */
static int
take_proportionally (const NemoColumnLayoutItem *items,
		     int                        *widths,
		     const int                  *floors,
		     int                         n_items,
		     int                         skip,
		     gboolean                    elastic_only,
		     int                         deficit)
{
	while (deficit > 0) {
		gint64 eligible_width = 0;
		int given = 0;
		int widest = -1;
		int i;

		for (i = 0; i < n_items; i++) {
			if (i == skip || (elastic_only && !items[i].elastic)) {
				continue;
			}
			if (widths[i] > floors[i]) {
				eligible_width += widths[i];
			}
		}

		if (eligible_width <= 0) {
			/* Everything eligible is on its floor. Where that is every
			   column, what is left cannot be given back and the view
			   scrolls sideways instead - the honest answer to a window
			   narrower than its own contents. */
			return deficit;
		}

		for (i = 0; i < n_items; i++) {
			int room, share;

			if (i == skip || (elastic_only && !items[i].elastic)) {
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
				return deficit;
			}

			widths[widest] -= 1;
			deficit -= 1;
		}
	}

	return 0;
}

/* What the growing columns would end up with between them if the columns with no
   natural limit stopped at `cap`, divided by the share they are allowed. Falls as
   `cap` rises - every pixel a capped column takes is one the growing part does
   not get - so there is exactly one width where a cap equals the share it
   implies, and that is the one wanted. */
static int
share_implied_by (const NemoColumnLayoutItem *items,
		  const int                  *targets,
		  const int                  *floors,
		  int                         n_items,
		  int                         name_index,
		  int                         growth_index,
		  int                         available,
		  int                         cap)
{
	int others = 0;
	int wanted;
	int share;
	int room;
	int i;

	for (i = 0; i < n_items; i++) {
		if (i == name_index || i == growth_index) {
			continue;
		}

		others += items[i].unbounded
			? MAX (floors[i], MIN (targets[i], cap))
			: targets[i];
	}

	room = available - others;

	/* What the growing columns are left with. The pair divides `room` between
	   them whatever their longest values are, so its floors are the only lower
	   bound there; Name on its own keeps its longest name instead. */
	wanted = growth_index >= 0
		? floors[name_index] + floors[growth_index]
		: targets[name_index];

	share = MAX (wanted, room) / UNBOUNDED_SHARE;

	/* A capped column never outgrows the columns that say which row this is.
	   Name on its own always has the surplus and so is wider than its own third
	   already; the pair has to be checked, since a folder of short names leaves
	   Name narrow while Location takes the rest. */
	if (growth_index >= 0) {
		int name_width = MAX (floors[name_index], MIN (targets[name_index], room / 2));
		int growth_width = MAX (floors[growth_index], room - name_width);

		share = MIN (share, MIN (name_width, growth_width));
	}

	return share;
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
	int growth_index = -1;
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
		} else if (items[i].shares_growth && growth_index < 0) {
			growth_index = i;
		}
	}

	/* Nothing to grow alongside. */
	if (name_index < 0) {
		growth_index = -1;
	}

	if (shrink_first < 0 || shrink_first >= n_items ||
	    shrink_first == name_index || shrink_first == growth_index) {
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
						     name_index, growth_index,
						     available, mid)) {
				lo = mid;
			} else {
				hi = mid - 1;
			}
		}

		for (i = 0; i < n_items; i++) {
			if (i != name_index && i != growth_index && items[i].unbounded) {
				targets[i] = MAX (floors[i], MIN (targets[i], lo));
			}
		}
	}

	/* Name and Location divide what the rest leave. Name takes no more than
	   half, so Location always has at least as much, and a Name column that can
	   already show its longest name hands the difference straight over. */
	if (growth_index >= 0) {
		int others = 0;
		int room;

		for (i = 0; i < n_items; i++) {
			if (i != name_index && i != growth_index) {
				others += targets[i];
			}
		}

		room = available - others;

		if (room >= floors[name_index] + floors[growth_index]) {
			targets[name_index] = MAX (floors[name_index],
						   MIN (targets[name_index], room / 2));
			targets[growth_index] = MAX (floors[growth_index],
						     room - targets[name_index]);
		}
	}

	for (i = 0; i < n_items; i++) {
		sum_targets += targets[i];
	}

	if (available >= sum_targets) {
		give_everyone_their_target (targets, n_items,
					    growth_index >= 0 ? growth_index : name_index,
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

	/* Then the columns that still read when they are cut short, and only once
	   those are spent, everything else. */
	if (deficit > 0) {
		deficit = take_proportionally (items, widths, floors, n_items,
					       shrink_first, TRUE, deficit);
	}

	if (deficit > 0) {
		take_proportionally (items, widths, floors, n_items,
				     shrink_first, FALSE, deficit);
	}

	g_free (targets);
	g_free (floors);
}
