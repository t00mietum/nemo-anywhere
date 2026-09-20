/* The one assertion every test uses. Prints where it failed and keeps going,
 * so one run reports every problem rather than only the first.
 *
 * The counter is static on purpose: each test is its own program with one
 * translation unit of its own, and main() reads `failures` directly. */

#ifndef TEST_CHECK_H
#define TEST_CHECK_H

#include <glib.h>

static int failures = 0;

#define check(expr) \
	G_STMT_START { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} G_STMT_END

#endif /* TEST_CHECK_H */
