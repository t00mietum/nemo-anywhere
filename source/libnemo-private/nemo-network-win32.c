/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-network-win32.c - network:/// over native Windows networking.

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
#include "nemo-network-win32.h"

#ifdef G_OS_WIN32

#include <string.h>
#include <gio/gio.h>
#include <glib/gi18n.h>

#include <windows.h>
#include <winnetwk.h>

#define ROOT_URI "network:///"

/* uri layout: network:/// lists servers, network:///SERVER lists that
 * server's shares. A share is a shortcut whose target is its UNC path,
 * which GIO on win32 treats as a plain native location. */

typedef struct {
	char *name;        /* display: server or share leaf name */
	char *unc;         /* \\server or \\server\share */
	gboolean is_share;
} NetItem;

static void
net_item_free (gpointer data)
{
	NetItem *item = data;

	g_free (item->name);
	g_free (item->unc);
	g_free (item);
}

/* ---- WNet enumeration ---- */

/* enumerate one container; item->unc NULL means the whole net root */
static GList *
enum_container (NETRESOURCEW *container, gboolean shares)
{
	HANDLE handle;
	GList *items = NULL;
	DWORD rc;

	rc = WNetOpenEnumW (RESOURCE_GLOBALNET, RESOURCETYPE_DISK, 0,
			    container, &handle);
	if (rc != NO_ERROR) {
		return NULL;
	}

	for (;;) {
		BYTE buffer[16384];
		DWORD count = (DWORD) -1, size = sizeof (buffer), i;
		NETRESOURCEW *resources = (NETRESOURCEW *) buffer;

		rc = WNetEnumResourceW (handle, &count, buffer, &size);
		if (rc != NO_ERROR) {
			break;
		}

		for (i = 0; i < count; i++) {
			NETRESOURCEW *res = &resources[i];
			char *remote;

			if (res->lpRemoteName == NULL) {
				continue;
			}
			remote = g_utf16_to_utf8 ((gunichar2 *) res->lpRemoteName,
						  -1, NULL, NULL, NULL);
			if (remote == NULL) {
				continue;
			}

			if (shares) {
				/* under a server: shares only */
				if (res->dwType == RESOURCETYPE_DISK &&
				    res->dwDisplayType == RESOURCEDISPLAYTYPE_SHARE) {
					NetItem *item = g_new0 (NetItem, 1);
					const char *leaf = strrchr (remote, '\\');

					item->name = g_strdup (leaf != NULL ? leaf + 1 : remote);
					item->unc = g_strdup (remote);
					item->is_share = TRUE;
					items = g_list_prepend (items, item);
				}
			} else if (res->dwDisplayType == RESOURCEDISPLAYTYPE_SERVER) {
				NetItem *item = g_new0 (NetItem, 1);
				const char *name = remote;

				while (*name == '\\') {
					name++;
				}
				item->name = g_strdup (name);
				item->unc = g_strdup (remote);
				items = g_list_prepend (items, item);
			} else if (res->dwUsage & RESOURCEUSAGE_CONTAINER) {
				/* descend through providers/domains to servers */
				GList *sub = enum_container (res, FALSE);

				items = g_list_concat (items, sub);
			}

			g_free (remote);
		}
	}

	WNetCloseEnum (handle);
	return items;
}

static GList *
enum_servers (void)
{
	return g_list_reverse (enum_container (NULL, FALSE));
}

static GList *
enum_shares (const char *server_name)
{
	NETRESOURCEW container;
	char *unc;
	gunichar2 *unc_w;
	GList *items;

	unc = g_strdup_printf ("\\\\%s", server_name);
	unc_w = g_utf8_to_utf16 (unc, -1, NULL, NULL, NULL);
	g_free (unc);
	if (unc_w == NULL) {
		return NULL;
	}

	memset (&container, 0, sizeof (container));
	container.dwUsage = RESOURCEUSAGE_CONTAINER;
	container.lpRemoteName = (LPWSTR) unc_w;

	items = g_list_reverse (enum_container (&container, TRUE));
	g_free (unc_w);
	return items;
}

/* ---- GFile implementation ---- */

#define NEMO_TYPE_NETWORK_WIN32_FILE (nemo_network_win32_file_get_type ())
#define NEMO_NETWORK_WIN32_FILE(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_NETWORK_WIN32_FILE, NemoNetworkWin32File))

typedef struct {
	GObject parent;
	char *uri;
} NemoNetworkWin32File;

typedef struct {
	GObjectClass parent_class;
} NemoNetworkWin32FileClass;

static GType nemo_network_win32_file_get_type (void);
static void nemo_network_win32_file_iface_init (GFileIface *iface);
static GFile *network_file_new_for_uri (const char *uri);

G_DEFINE_TYPE_WITH_CODE (NemoNetworkWin32File, nemo_network_win32_file, G_TYPE_OBJECT,
			 G_IMPLEMENT_INTERFACE (G_TYPE_FILE, nemo_network_win32_file_iface_init))

static gboolean
uri_is_root (const char *uri)
{
	return strcmp (uri, ROOT_URI) == 0 || strcmp (uri, "network://") == 0 ||
	       strcmp (uri, "network:") == 0;
}

/* server name from network:///SERVER, NULL for the root */
static char *
uri_to_server (const char *uri)
{
	if (uri_is_root (uri)) {
		return NULL;
	}
	return g_uri_unescape_string (uri + strlen (ROOT_URI), NULL);
}

static GFileInfo *
make_server_info (NetItem *item)
{
	GFileInfo *info;
	GIcon *icon;
	char *escaped;

	info = g_file_info_new ();
	g_file_info_set_file_type (info, G_FILE_TYPE_DIRECTORY);
	g_file_info_set_content_type (info, "inode/directory");

	escaped = g_uri_escape_string (item->name, NULL, TRUE);
	g_file_info_set_name (info, escaped);
	g_free (escaped);
	g_file_info_set_display_name (info, item->name);

	icon = g_themed_icon_new_with_default_fallbacks ("network-server");
	g_file_info_set_icon (info, icon);
	g_object_unref (icon);

	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, FALSE);
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE, FALSE);
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME, FALSE);
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH, FALSE);

	return info;
}

static GFileInfo *
make_share_info (NetItem *item)
{
	GFileInfo *info;
	GFile *target;
	GIcon *icon;
	char *escaped, *target_uri;

	info = g_file_info_new ();
	g_file_info_set_file_type (info, G_FILE_TYPE_SHORTCUT);

	escaped = g_uri_escape_string (item->name, NULL, TRUE);
	g_file_info_set_name (info, escaped);
	g_free (escaped);
	g_file_info_set_display_name (info, item->name);

	/* the share's UNC path is an ordinary native location for GIO */
	target = g_file_new_for_path (item->unc);
	target_uri = g_file_get_uri (target);
	g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_STANDARD_TARGET_URI,
					  target_uri);
	g_free (target_uri);
	g_object_unref (target);

	icon = g_themed_icon_new_with_default_fallbacks ("folder-remote");
	g_file_info_set_icon (info, icon);
	g_object_unref (icon);

	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, FALSE);
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE, FALSE);
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME, FALSE);
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH, FALSE);

	return info;
}

/* ---- GFile vtable ---- */

static GFile *
network_file_dup (GFile *file)
{
	return network_file_new_for_uri (NEMO_NETWORK_WIN32_FILE (file)->uri);
}

static guint
network_file_hash (GFile *file)
{
	return g_str_hash (NEMO_NETWORK_WIN32_FILE (file)->uri);
}

static gboolean
network_file_equal (GFile *a, GFile *b)
{
	return g_str_equal (NEMO_NETWORK_WIN32_FILE (a)->uri,
			    NEMO_NETWORK_WIN32_FILE (b)->uri);
}

static gboolean
network_file_is_native (GFile *file)
{
	return FALSE;
}

static gboolean
network_file_has_uri_scheme (GFile *file, const char *scheme)
{
	return g_ascii_strcasecmp (scheme, "network") == 0;
}

static char *
network_file_get_uri_scheme (GFile *file)
{
	return g_strdup ("network");
}

static char *
network_file_get_basename (GFile *file)
{
	NemoNetworkWin32File *self = NEMO_NETWORK_WIN32_FILE (file);

	if (uri_is_root (self->uri)) {
		return g_strdup ("/");
	}
	return g_strdup (self->uri + strlen (ROOT_URI));
}

static char *
network_file_get_path (GFile *file)
{
	return NULL;
}

static char *
network_file_get_uri (GFile *file)
{
	return g_strdup (NEMO_NETWORK_WIN32_FILE (file)->uri);
}

static char *
network_file_get_parse_name (GFile *file)
{
	return g_strdup (NEMO_NETWORK_WIN32_FILE (file)->uri);
}

static GFile *
network_file_get_parent (GFile *file)
{
	NemoNetworkWin32File *self = NEMO_NETWORK_WIN32_FILE (file);

	if (uri_is_root (self->uri)) {
		return NULL;
	}
	return network_file_new_for_uri (ROOT_URI);
}

static gboolean
network_file_prefix_matches (GFile *parent, GFile *descendant)
{
	NemoNetworkWin32File *p = NEMO_NETWORK_WIN32_FILE (parent);
	NemoNetworkWin32File *d = NEMO_NETWORK_WIN32_FILE (descendant);

	return uri_is_root (p->uri) && !uri_is_root (d->uri);
}

static char *
network_file_get_relative_path (GFile *parent, GFile *descendant)
{
	NemoNetworkWin32File *p = NEMO_NETWORK_WIN32_FILE (parent);
	NemoNetworkWin32File *d = NEMO_NETWORK_WIN32_FILE (descendant);

	if (!uri_is_root (p->uri) || uri_is_root (d->uri)) {
		return NULL;
	}
	return g_strdup (d->uri + strlen (ROOT_URI));
}

static GFile *
network_file_resolve_relative_path (GFile *file, const char *relative_path)
{
	NemoNetworkWin32File *self = NEMO_NETWORK_WIN32_FILE (file);
	char *uri;
	GFile *result;

	if (relative_path == NULL || relative_path[0] == '\0' ||
	    strcmp (relative_path, "/") == 0) {
		return network_file_dup (file);
	}

	uri = g_strconcat (uri_is_root (self->uri) ? ROOT_URI : self->uri,
			   relative_path, NULL);
	result = network_file_new_for_uri (uri);
	g_free (uri);
	return result;
}

static GFile *
network_file_get_child_for_display_name (GFile *file, const char *display_name,
					 GError **error)
{
	return network_file_resolve_relative_path (file, display_name);
}

static GFileInfo *
network_file_query_info (GFile *file, const char *attributes,
			 GFileQueryInfoFlags flags, GCancellable *cancellable,
			 GError **error)
{
	NemoNetworkWin32File *self = NEMO_NETWORK_WIN32_FILE (file);
	GFileInfo *info;
	GIcon *icon;
	char *server;

	info = g_file_info_new ();
	g_file_info_set_file_type (info, G_FILE_TYPE_DIRECTORY);
	g_file_info_set_content_type (info, "inode/directory");
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ, TRUE);
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, FALSE);
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_DELETE, FALSE);
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME, FALSE);
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH, FALSE);

	server = uri_to_server (self->uri);
	if (server == NULL) {
		g_file_info_set_name (info, "/");
		g_file_info_set_display_name (info, _("Network"));
		icon = g_themed_icon_new_with_default_fallbacks ("network-workgroup");
	} else {
		g_file_info_set_name (info, self->uri + strlen (ROOT_URI));
		g_file_info_set_display_name (info, server);
		icon = g_themed_icon_new_with_default_fallbacks ("network-server");
	}
	g_file_info_set_icon (info, icon);
	g_object_unref (icon);
	g_free (server);

	return info;
}

static GFileInfo *
network_file_query_filesystem_info (GFile *file, const char *attributes,
				    GCancellable *cancellable, GError **error)
{
	GFileInfo *info;

	info = g_file_info_new ();
	g_file_info_set_attribute_string (info, G_FILE_ATTRIBUTE_FILESYSTEM_TYPE, "network");
	g_file_info_set_attribute_boolean (info, G_FILE_ATTRIBUTE_FILESYSTEM_READONLY, TRUE);
	return info;
}

/* ---- enumerator ---- */

#define NEMO_TYPE_NETWORK_WIN32_ENUMERATOR (nemo_network_win32_enumerator_get_type ())
#define NEMO_NETWORK_WIN32_ENUMERATOR(o) (G_TYPE_CHECK_INSTANCE_CAST ((o), NEMO_TYPE_NETWORK_WIN32_ENUMERATOR, NemoNetworkWin32Enumerator))

typedef struct {
	GFileEnumerator parent;
	GList *items;     /* NetItem */
	GList *position;
} NemoNetworkWin32Enumerator;

typedef struct {
	GFileEnumeratorClass parent_class;
} NemoNetworkWin32EnumeratorClass;

static GType nemo_network_win32_enumerator_get_type (void);

G_DEFINE_TYPE (NemoNetworkWin32Enumerator, nemo_network_win32_enumerator,
	       G_TYPE_FILE_ENUMERATOR)

static GFileInfo *
network_enumerator_next_file (GFileEnumerator *enumerator,
			      GCancellable *cancellable, GError **error)
{
	NemoNetworkWin32Enumerator *self = NEMO_NETWORK_WIN32_ENUMERATOR (enumerator);
	NetItem *item;
	GFileInfo *info;

	if (self->position == NULL) {
		return NULL;
	}

	item = self->position->data;
	info = item->is_share ? make_share_info (item) : make_server_info (item);
	self->position = self->position->next;
	return info;
}

static gboolean
network_enumerator_close (GFileEnumerator *enumerator,
			  GCancellable *cancellable, GError **error)
{
	return TRUE;
}

static void
network_enumerator_finalize (GObject *object)
{
	NemoNetworkWin32Enumerator *self = NEMO_NETWORK_WIN32_ENUMERATOR (object);

	g_list_free_full (self->items, net_item_free);

	G_OBJECT_CLASS (nemo_network_win32_enumerator_parent_class)->finalize (object);
}

static void
nemo_network_win32_enumerator_init (NemoNetworkWin32Enumerator *self)
{
}

static void
nemo_network_win32_enumerator_class_init (NemoNetworkWin32EnumeratorClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = network_enumerator_finalize;
	G_FILE_ENUMERATOR_CLASS (klass)->next_file = network_enumerator_next_file;
	G_FILE_ENUMERATOR_CLASS (klass)->close_fn = network_enumerator_close;
}

static GFileEnumerator *
network_file_enumerate_children (GFile *file, const char *attributes,
				 GFileQueryInfoFlags flags,
				 GCancellable *cancellable, GError **error)
{
	NemoNetworkWin32File *self = NEMO_NETWORK_WIN32_FILE (file);
	NemoNetworkWin32Enumerator *enumerator;
	char *server;

	enumerator = g_object_new (NEMO_TYPE_NETWORK_WIN32_ENUMERATOR,
				   "container", file, NULL);

	server = uri_to_server (self->uri);
	enumerator->items = server == NULL ? enum_servers () : enum_shares (server);
	enumerator->position = enumerator->items;
	g_free (server);

	return G_FILE_ENUMERATOR (enumerator);
}

/* ---- no-op monitor: the neighborhood refreshes on revisit ---- */

#define NEMO_TYPE_NETWORK_WIN32_MONITOR (nemo_network_win32_monitor_get_type ())

typedef struct {
	GFileMonitor parent;
} NemoNetworkWin32Monitor;

typedef struct {
	GFileMonitorClass parent_class;
} NemoNetworkWin32MonitorClass;

static GType nemo_network_win32_monitor_get_type (void);

G_DEFINE_TYPE (NemoNetworkWin32Monitor, nemo_network_win32_monitor, G_TYPE_FILE_MONITOR)

static gboolean
network_win32_monitor_cancel (GFileMonitor *monitor)
{
	return TRUE;
}

static void
nemo_network_win32_monitor_init (NemoNetworkWin32Monitor *monitor)
{
}

static void
nemo_network_win32_monitor_class_init (NemoNetworkWin32MonitorClass *klass)
{
	G_FILE_MONITOR_CLASS (klass)->cancel = network_win32_monitor_cancel;
}

static GFileMonitor *
network_file_monitor (GFile *file, GFileMonitorFlags flags,
		      GCancellable *cancellable, GError **error)
{
	return g_object_new (NEMO_TYPE_NETWORK_WIN32_MONITOR, NULL);
}

/* ---- plumbing ---- */

static void
network_file_finalize (GObject *object)
{
	NemoNetworkWin32File *self = NEMO_NETWORK_WIN32_FILE (object);

	g_free (self->uri);

	G_OBJECT_CLASS (nemo_network_win32_file_parent_class)->finalize (object);
}

static void
nemo_network_win32_file_init (NemoNetworkWin32File *self)
{
}

static void
nemo_network_win32_file_class_init (NemoNetworkWin32FileClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = network_file_finalize;
}

static void
nemo_network_win32_file_iface_init (GFileIface *iface)
{
	iface->dup = network_file_dup;
	iface->hash = network_file_hash;
	iface->equal = network_file_equal;
	iface->is_native = network_file_is_native;
	iface->has_uri_scheme = network_file_has_uri_scheme;
	iface->get_uri_scheme = network_file_get_uri_scheme;
	iface->get_basename = network_file_get_basename;
	iface->get_path = network_file_get_path;
	iface->get_uri = network_file_get_uri;
	iface->get_parse_name = network_file_get_parse_name;
	iface->get_parent = network_file_get_parent;
	iface->prefix_matches = network_file_prefix_matches;
	iface->get_relative_path = network_file_get_relative_path;
	iface->resolve_relative_path = network_file_resolve_relative_path;
	iface->get_child_for_display_name = network_file_get_child_for_display_name;
	iface->enumerate_children = network_file_enumerate_children;
	iface->query_info = network_file_query_info;
	iface->query_filesystem_info = network_file_query_filesystem_info;
	iface->monitor_dir = network_file_monitor;
	iface->monitor_file = network_file_monitor;
}

static GFile *
network_file_new_for_uri (const char *uri)
{
	NemoNetworkWin32File *self;

	self = g_object_new (NEMO_TYPE_NETWORK_WIN32_FILE, NULL);
	self->uri = g_strdup (uri);
	return G_FILE (self);
}

static GFile *
network_vfs_lookup (GVfs *vfs, const char *identifier, gpointer user_data)
{
	if (g_str_has_prefix (identifier, "network:")) {
		return network_file_new_for_uri (identifier);
	}
	return NULL;
}

void
nemo_network_win32_register (void)
{
	static gsize once_init_value = 0;

	if (g_once_init_enter (&once_init_value)) {
		GVfs *vfs;

		vfs = g_vfs_get_default ();
		g_vfs_register_uri_scheme (vfs, "network",
					   network_vfs_lookup, NULL, NULL,
					   network_vfs_lookup, NULL, NULL);

		g_once_init_leave (&once_init_value, 1);
	}
}

#else /* !G_OS_WIN32 */

void
nemo_network_win32_register (void)
{
}

#endif /* G_OS_WIN32 */
