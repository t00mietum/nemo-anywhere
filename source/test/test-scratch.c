/* Scratch directories for the tests, removed when the test exits.
 *
 * Only a directory this process made through here is removed, and only while
 * it still sits directly under the temp dir as a real directory. The walk never
 * goes through a link, a junction or onto another filesystem. Home has been
 * lost twice on the dev box; a test helper is not going to be the third. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>

#ifndef G_OS_WIN32
#include <unistd.h>
#endif

#include <libnemo-private/nemo-dir-enum.h>

#include "test-scratch.h"

#define WALK_ATTRIBUTES \
	G_FILE_ATTRIBUTE_STANDARD_NAME "," \
	G_FILE_ATTRIBUTE_STANDARD_TYPE "," \
	G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK "," \
	G_FILE_ATTRIBUTE_UNIX_DEVICE "," \
	G_FILE_ATTRIBUTE_UNIX_UID "," \
	G_FILE_ATTRIBUTE_DOS_REPARSE_POINT_TAG

static GPtrArray *made = NULL;
static gboolean registered = FALSE;
#ifndef G_OS_WIN32
static pid_t made_by = 0;
#endif

static gboolean
is_real_dir (GFileInfo *info)
{
	return nemo_dir_enum_file_type (info) == G_FILE_TYPE_DIRECTORY &&
	       !g_file_info_get_attribute_boolean (info, G_FILE_ATTRIBUTE_STANDARD_IS_SYMLINK) &&
	       g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_DOS_REPARSE_POINT_TAG) == 0;
}

static guint32
device_of (GFileInfo *info)
{
	return g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_DEVICE);
}

static void
remove_tree (GFile *dir, guint32 device)
{
	GFileEnumerator *children;
	GFileInfo *info;

	children = nemo_enumerate_children (dir, WALK_ATTRIBUTES,
					    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, NULL);
	if (children != NULL) {
		while ((info = g_file_enumerator_next_file (children, NULL, NULL)) != NULL) {
			GFile *child = g_file_get_child (dir, g_file_info_get_name (info));

			if (!is_real_dir (info)) {
				g_file_delete (child, NULL, NULL);
			} else if (device_of (info) == device) {
				remove_tree (child, device);
			}
			/* A directory on another device is a mount, and is left. */

			g_object_unref (child);
			g_object_unref (info);
		}
		g_object_unref (children);
	}

	g_file_delete (dir, NULL, NULL);
}

static void
remove_made (const char *path)
{
	GFile *file, *tmp;
	GFileInfo *info, *tmp_info;
	GError *error = NULL;
	char *parent, *tmp_path;
	gboolean owned;

	file = g_file_new_for_path (path);
	info = g_file_query_info (file, WALK_ATTRIBUTES,
				  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, NULL, &error);
	if (info == NULL) {
		/* The test cleaned up after itself. */
		if (!g_error_matches (error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
			g_printerr ("scratch: left %s: %s\n", path, error->message);
		}
		g_error_free (error);
		g_object_unref (file);
		return;
	}

	tmp_path = g_canonicalize_filename (g_get_tmp_dir (), NULL);
	tmp = g_file_new_for_path (tmp_path);
	tmp_info = g_file_query_info (tmp, WALK_ATTRIBUTES, 0, NULL, NULL);
	parent = g_path_get_dirname (path);

	owned = g_path_is_absolute (path) &&
		strcmp (parent, tmp_path) == 0 &&
		is_real_dir (info) &&
		tmp_info != NULL &&
		device_of (info) == device_of (tmp_info);
#ifndef G_OS_WIN32
	owned = owned && g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_UID) == getuid ();
#endif

	if (owned) {
		remove_tree (file, device_of (info));
		if (g_file_query_exists (file, NULL)) {
			g_printerr ("scratch: could not remove all of %s\n", path);
		}
	} else {
		g_printerr ("scratch: left %s, it is no longer the directory made there\n", path);
	}

	g_free (parent);
	g_clear_object (&tmp_info);
	g_object_unref (tmp);
	g_free (tmp_path);
	g_object_unref (info);
	g_object_unref (file);
}

void
test_scratch_cleanup (void)
{
	guint i;

	if (made == NULL) {
		return;
	}

#ifndef G_OS_WIN32
	/* A forked child inherits the exit handler, and must not take the parent's
	   directories out from under it. */
	if (getpid () != made_by) {
		return;
	}
#endif

	for (i = 0; i < made->len; i++) {
		remove_made (g_ptr_array_index (made, i));
	}

	g_ptr_array_free (made, TRUE);
	made = NULL;
}

char *
test_scratch_dir (const char *tmpl, GError **error)
{
	char *path;

	path = g_dir_make_tmp (tmpl, error);
	if (path == NULL) {
		return NULL;
	}

	if (made == NULL) {
		made = g_ptr_array_new_with_free_func (g_free);
	}
	g_ptr_array_add (made, g_canonicalize_filename (path, NULL));

	if (!registered) {
#ifndef G_OS_WIN32
		made_by = getpid ();
#endif
		atexit (test_scratch_cleanup);
		registered = TRUE;
	}

	return path;
}
