#ifndef TEST_SCRATCH_H
#define TEST_SCRATCH_H

#include <glib.h>

G_BEGIN_DECLS

/* Same contract as g_dir_make_tmp. The directory is removed when the test
   exits, however it exits short of a crash. */
char *test_scratch_dir     (const char  *tmpl,
			    GError     **error);

/* The same, made in base instead of the temp dir, for a test that needs a
   second file system. Removed only while it still sits directly in base. */
char *test_scratch_dir_in  (const char  *base,
			    const char  *tmpl,
			    GError     **error);

/* Removes every directory made so far. Runs on its own at exit. */
void  test_scratch_cleanup (void);

G_END_DECLS

#endif /* TEST_SCRATCH_H */
