/* How the list view divides its width between columns. Pure arithmetic, so all
 * of it is checkable without a screen - which matters, because the failure this
 * guards against is a column pushed off the end of the window or a strip of dead
 * space after the last one, and neither shows up in any other test. */

#include <config.h>

#include <stdlib.h>
#include <glib.h>

#include <src/nemo-column-layout.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

enum { NAME, SIZE, TYPE, DATE, N_COLS };

/* A plausible row of columns: Name, Size, Type, Date Modified. Type is the one
   with no natural limit and the one nominated to give first, which is what the
   view does. */
static void
usual_columns (NemoColumnLayoutItem *items)
{
	items[NAME] = (NemoColumnLayoutItem) { 100, 300, FALSE, TRUE };
	items[SIZE] = (NemoColumnLayoutItem) {  30,  80, FALSE, FALSE };
	items[TYPE] = (NemoColumnLayoutItem) {  30, 120, TRUE,  FALSE };
	items[DATE] = (NemoColumnLayoutItem) {  30, 160, FALSE, FALSE };
}

static int
total (const int *widths, int n)
{
	int sum = 0, i;

	for (i = 0; i < n; i++) {
		sum += widths[i];
	}

	return sum;
}

/* Nothing is left over and nothing hangs off the end. */
static void
check_fills_the_width (void)
{
	NemoColumnLayoutItem items[N_COLS];
	int widths[N_COLS];
	int available;

	usual_columns (items);

	for (available = 220; available <= 2000; available += 7) {
		nemo_column_layout_distribute (items, N_COLS, TYPE, available, widths);
		check (total (widths, N_COLS) == available);
	}
}

/* Room to spare: every column shows its longest value and Name has the rest. */
static void
check_wide (void)
{
	NemoColumnLayoutItem items[N_COLS];
	int widths[N_COLS];

	usual_columns (items);
	nemo_column_layout_distribute (items, N_COLS, TYPE, 1400, widths);

	check (widths[SIZE] == 80);
	check (widths[DATE] == 160);
	/* Type has a limit here well under a third of Name, so it stops at its own
	   longest value and Name takes everything that is left. */
	check (widths[TYPE] == 120);
	check (widths[NAME] == 1400 - 80 - 120 - 160);
}

/* Just short: Type gives, and nothing else moves. */
static void
check_type_gives_first (void)
{
	NemoColumnLayoutItem items[N_COLS];
	int widths[N_COLS];
	int natural_total;

	usual_columns (items);
	natural_total = 300 + 80 + 120 + 160;

	nemo_column_layout_distribute (items, N_COLS, TYPE, natural_total - 40, widths);

	check (widths[NAME] == 300);
	check (widths[SIZE] == 80);
	check (widths[DATE] == 160);
	check (widths[TYPE] == 80);
}

/* Shorter still: Type is on its floor and everything else gives together, the
   widest giving the most. */
static void
check_then_everyone (void)
{
	NemoColumnLayoutItem items[N_COLS];
	int widths[N_COLS];

	usual_columns (items);
	/* natural is 660, and Type has 90 to give before its floor */
	nemo_column_layout_distribute (items, N_COLS, TYPE, 460, widths);

	check (widths[TYPE] == 30);
	check (widths[NAME] < 300);
	check (widths[SIZE] < 80);
	check (widths[DATE] < 160);
	/* Name started widest, so Name gave the most. */
	check (300 - widths[NAME] > 160 - widths[DATE]);
	check (160 - widths[DATE] > 80 - widths[SIZE]);
	check (total (widths, N_COLS) == 460);
}

/* With no column nominated to go first, everything gives from the start. */
static void
check_no_first_giver (void)
{
	NemoColumnLayoutItem items[N_COLS];
	int widths[N_COLS];

	usual_columns (items);
	nemo_column_layout_distribute (items, N_COLS, -1, 620, widths);

	check (widths[TYPE] < 120);
	check (widths[NAME] < 300);
	check (widths[DATE] < 160);
	check (total (widths, N_COLS) == 620);
}

/* A column with no natural limit does not get to take the window. */
static void
check_unbounded_is_capped (void)
{
	NemoColumnLayoutItem items[N_COLS];
	int widths[N_COLS];

	usual_columns (items);
	items[TYPE].natural_width = 5000;

	nemo_column_layout_distribute (items, N_COLS, TYPE, 1200, widths);

	/* Exactly a third of what Name ends up with, not a third of some earlier
	   guess at it - the cap and Name's width have to agree with each other. */
	check (widths[TYPE] == 240);
	check (widths[NAME] == 720);
	check (widths[TYPE] * 3 == widths[NAME]);
	check (total (widths, N_COLS) == 1200);

	/* Without the flag it would simply take what it says it needs. */
	usual_columns (items);
	items[TYPE].natural_width = 5000;
	items[TYPE].unbounded = FALSE;

	nemo_column_layout_distribute (items, N_COLS, -1, 1200, widths);
	check (widths[TYPE] > widths[NAME]);
}

/* Narrower than the floors add up to. Nothing goes below its floor and nothing
   comes back negative; the view scrolls sideways instead. */
static void
check_impossibly_narrow (void)
{
	NemoColumnLayoutItem items[N_COLS];
	int widths[N_COLS];
	int i;

	usual_columns (items);
	nemo_column_layout_distribute (items, N_COLS, TYPE, 40, widths);

	for (i = 0; i < N_COLS; i++) {
		check (widths[i] >= items[i].floor_width);
	}

	check (total (widths, N_COLS) == 100 + 30 + 30 + 30);
}

/* One column on its own is still the whole width. */
static void
check_single_column (void)
{
	NemoColumnLayoutItem only = { 100, 300, FALSE, TRUE };
	int width = 0;

	nemo_column_layout_distribute (&only, 1, -1, 900, &width);
	check (width == 900);

	nemo_column_layout_distribute (&only, 1, -1, 50, &width);
	check (width == 100);
}

/* A floor wider than the longest value in the column still wins. */
static void
check_floor_beats_natural (void)
{
	NemoColumnLayoutItem items[2] = {
		{ 100, 300, FALSE, TRUE },
		{ 120,  20, FALSE, FALSE }
	};
	int widths[2];

	nemo_column_layout_distribute (items, 2, -1, 1000, widths);
	check (widths[1] == 120);
	check (widths[0] == 880);
}

int
main (int argc, char *argv[])
{
	check_fills_the_width ();
	check_wide ();
	check_type_gives_first ();
	check_then_everyone ();
	check_no_first_giver ();
	check_unbounded_is_capped ();
	check_impossibly_narrow ();
	check_single_column ();
	check_floor_beats_natural ();

	if (failures > 0) {
		g_printerr ("%d check(s) failed\n", failures);
		return EXIT_FAILURE;
	}

	g_print ("OK\n");
	return EXIT_SUCCESS;
}
