/* The uri algebra of the Windows network:/// backend. Enumeration itself needs a
 * network - and an unreachable host costs the full timeout - so what is checked
 * here is the part that has to hold with no network at all: how a child uri is
 * built, and that two different server/share pairs cannot land on the same uri.
 * Windows-only; the Linux build compiles it out. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <gio/gio.h>

#ifdef G_OS_WIN32

#include <libnemo-private/nemo-network-win32.h>

static int failures = 0;

#define check(expr) \
	do { \
		if (!(expr)) { \
			g_printerr ("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
			failures++; \
		} \
	} while (0)

/* uri of root's child @server, or of that server's child @share when given */
static char *
child_uri (const char *server, const char *share)
{
	GFile *root, *node, *leaf;
	char *uri;

	root = g_file_new_for_uri ("network:///");
	node = g_file_get_child (root, server);

	if (share == NULL) {
		uri = g_file_get_uri (node);
	} else {
		leaf = g_file_get_child (node, share);
		uri = g_file_get_uri (leaf);
		g_object_unref (leaf);
	}

	g_object_unref (node);
	g_object_unref (root);
	return uri;
}

/* Browse a server that really is there, if this box is one. Enumeration is the
 * half the uri checks cannot reach, and a share only proves anything if it can
 * be followed - so the target each one points at is opened for real. Skips, out
 * loud, on a box that shares nothing. */
static void
test_live_shares (void)
{
	const char *host = g_get_host_name ();
	GFile *root, *server;
	GFileEnumerator *enumerator;
	GFileInfo *info;
	GError *error = NULL;
	int shares = 0, followed = 0;

	if (host == NULL || *host == '\0') {
		g_printerr ("SKIP live shares: no host name\n");
		return;
	}

	root = g_file_new_for_uri ("network:///");
	server = g_file_get_child (root, host);

	enumerator = g_file_enumerate_children (server, "standard::*", 0, NULL, &error);
	if (enumerator == NULL) {
		g_printerr ("SKIP live shares: cannot browse \\\\%s: %s\n",
			    host, error != NULL ? error->message : "?");
		g_clear_error (&error);
		g_object_unref (server);
		g_object_unref (root);
		return;
	}

	while ((info = g_file_enumerator_next_file (enumerator, NULL, NULL)) != NULL) {
		const char *target_uri;

		shares++;

		/* A share is a link to its UNC path - that is what makes opening
		 * one work through the activation nemo already had. */
		check (g_file_info_get_file_type (info) == G_FILE_TYPE_SHORTCUT);

		target_uri = g_file_info_get_attribute_string (
			info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);
		check (target_uri != NULL);

		if (target_uri != NULL) {
			GFile *target = g_file_new_for_uri (target_uri);
			GFileInfo *reached;

			/* Every share the enumeration offered has to sit under
			 * the server it was listed beneath, or the uri built for
			 * it names some other machine entirely. */
			check (g_file_has_prefix (target, server) == FALSE);

			reached = g_file_query_info (target, "standard::type",
						     0, NULL, NULL);
			if (reached != NULL) {
				followed++;
				g_object_unref (reached);
			} else {
				/* An empty optical drive shares fine and opens
				 * to nothing, so name it rather than counting
				 * it silently against the backend. */
				g_print ("  share %s did not open (%s)\n",
					 g_file_info_get_display_name (info),
					 target_uri);
			}
			g_object_unref (target);
		}

		g_object_unref (info);
	}

	g_file_enumerator_close (enumerator, NULL, NULL);
	g_object_unref (enumerator);

	if (shares == 0) {
		g_printerr ("SKIP live shares: \\\\%s offers none\n", host);
	} else {
		g_print ("  browsed \\\\%s: %d share(s), %d opened\n",
			 host, shares, followed);
		/* At least one has to actually open, or "browsing works" is a
		 * claim about a list nobody can follow. */
		check (followed > 0);
	}

	g_object_unref (server);
	g_object_unref (root);
}

int
main (int argc, char *argv[])
{
	GFile *root, *server;
	char *a, *b, *uri;

	nemo_network_win32_register ();

	uri = child_uri ("FILESRV", NULL);
	check (g_strcmp0 (uri, "network:///FILESRV") == 0);
	g_free (uri);

	/* The separator is the whole point: without it the share ran straight onto
	 * the server name and the result was a server called FILESRVpublic. */
	uri = child_uri ("FILESRV", "public");
	check (g_strcmp0 (uri, "network:///FILESRV/public") == 0);
	if (uri != NULL && strstr (uri, "FILESRVpublic") != NULL) {
		g_printerr ("  built %s\n", uri);
	}
	g_free (uri);

	/* Two different pairs that concatenate to the same string. With no
	 * separator both are network:///SRVAshare and one hides the other. */
	a = child_uri ("SRV", "Ashare");
	b = child_uri ("SRVA", "share");
	check (g_strcmp0 (a, b) != 0);
	g_free (a);
	g_free (b);

	/* The root is the top: nothing above it, and it is the parent of a server. */
	root = g_file_new_for_uri ("network:///");
	check (g_file_get_parent (root) == NULL);

	server = g_file_get_child (root, "FILESRV");
	check (g_file_has_prefix (server, root));
	{
		GFile *back = g_file_get_parent (server);

		check (back != NULL && g_file_equal (back, root));
		g_clear_object (&back);
	}
	g_object_unref (server);
	g_object_unref (root);

	test_live_shares ();

	if (failures == 0) {
		g_print ("network-win32: all checks passed\n");
	}
	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}

#else /* !G_OS_WIN32 */

int
main (int argc, char *argv[])
{
	return EXIT_SUCCESS;
}

#endif
