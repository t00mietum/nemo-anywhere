/* nemo-shell-icon-win32.c - the icon the Windows shell would draw for a file
 *
 * A shortcut's icon is its target's, which only the shell knows how to find:
 * the target may be a program with its own icon, a folder, or a shell item
 * with no path at all. SHGetFileInfo answers with an index into the system
 * image lists, and the list for the wanted size hands back a real icon at
 * that size rather than a scaled one. None of it needs Explorer running.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 */

#include <config.h>

#define COBJMACROS
#include <windows.h>
#include <shellapi.h>
#include <commctrl.h>
#include <commoncontrols.h>
#include <shlobj.h>

#include "nemo-shell-icon-win32.h"

/* Exported by GDK's win32 backend, declared only for GTK's own build. */
GdkPixbuf *gdk_win32_icon_to_pixbuf_libgtk_only (HICON    hicon,
						 gdouble *x_hot,
						 gdouble *y_hot);

#define CACHE_LIMIT 2000

static GHashTable *cache = NULL;

static const GUID iid_iimagelist = { 0x46EB5926, 0x582E, 0x4017, { 0x9F, 0xDF, 0xE8, 0x99, 0x8D, 0xAA, 0x09, 0x50 } };

static gint
list_for_size (gint pixel_size, gint *list_size)
{
	if (pixel_size <= 16) {
		*list_size = 16;
		return SHIL_SMALL;
	}
	if (pixel_size <= 32) {
		*list_size = 32;
		return SHIL_LARGE;
	}
	if (pixel_size <= 48) {
		*list_size = 48;
		return SHIL_EXTRALARGE;
	}
	*list_size = 256;
	return SHIL_JUMBO;
}

static GdkPixbuf *
icon_from_shell (const gchar *path, gint pixel_size)
{
	wchar_t *wide = g_utf8_to_utf16 (path, -1, NULL, NULL, NULL);
	SHFILEINFOW info;
	IImageList *list = NULL;
	HICON icon = NULL;
	GdkPixbuf *pixbuf = NULL;
	gint list_size, which;

	if (wide == NULL) {
		return NULL;
	}

	memset (&info, 0, sizeof info);

	if (SHGetFileInfoW (wide, 0, &info, sizeof info, SHGFI_SYSICONINDEX | SHGFI_SMALLICON) == 0) {
		g_free (wide);
		return NULL;
	}
	g_free (wide);

	which = list_for_size (pixel_size, &list_size);

	if (SUCCEEDED (SHGetImageList (which, &iid_iimagelist, (void **) &list)) && list != NULL) {
		if (SUCCEEDED (IImageList_GetIcon (list, info.iIcon, ILD_TRANSPARENT, &icon)) && icon != NULL) {
			pixbuf = gdk_win32_icon_to_pixbuf_libgtk_only (icon, NULL, NULL);
			DestroyIcon (icon);
		}
		IImageList_Release (list);
	}

	/* The jumbo list pads an icon it only has at 48 with blank space around
	 * it; a smaller size than asked for is better than a small picture in a
	 * large frame. */
	if (pixbuf != NULL && list_size == 256) {
		gint w = gdk_pixbuf_get_width (pixbuf);
		gint h = gdk_pixbuf_get_height (pixbuf);
		const guchar *pixels = gdk_pixbuf_get_pixels (pixbuf);
		gint stride = gdk_pixbuf_get_rowstride (pixbuf);
		gboolean blank_corner = gdk_pixbuf_get_has_alpha (pixbuf);
		gint x, y;

		/* anything drawn in the bottom-right quarter means the full size is real */
		for (y = h * 3 / 4; y < h && blank_corner; y++) {
			for (x = w * 3 / 4; x < w; x++) {
				if (pixels[y * stride + x * 4 + 3] != 0) {
					blank_corner = FALSE;
					break;
				}
			}
		}

		if (blank_corner) {
			GdkPixbuf *smaller = icon_from_shell (path, 48);

			if (smaller != NULL) {
				g_object_unref (pixbuf);
				pixbuf = smaller;
			}
		}
	}

	return pixbuf;
}

GdkPixbuf *
nemo_shell_icon_win32_for_path (const gchar *path,
				gint         pixel_size,
				gint64       mtime)
{
	gchar *key;
	GdkPixbuf *pixbuf;
	gint list_size;

	g_return_val_if_fail (path != NULL, NULL);
	g_return_val_if_fail (pixel_size > 0, NULL);

	list_for_size (pixel_size, &list_size);

	if (cache == NULL) {
		cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
	} else if (g_hash_table_size (cache) >= CACHE_LIMIT) {
		g_hash_table_remove_all (cache);
	}

	key = g_strdup_printf ("%s|%d|%lld", path, pixel_size, (long long) mtime);
	pixbuf = g_hash_table_lookup (cache, key);

	if (pixbuf != NULL) {
		g_free (key);
		return g_object_ref (pixbuf);
	}

	pixbuf = icon_from_shell (path, list_size);

	/* The view asked for a size the shell has no list at: scale down from the
	 * next one up rather than hand back a picture larger than the cell. */
	if (pixbuf != NULL && gdk_pixbuf_get_width (pixbuf) != pixel_size) {
		GdkPixbuf *scaled = gdk_pixbuf_scale_simple (pixbuf, pixel_size, pixel_size, GDK_INTERP_HYPER);

		g_object_unref (pixbuf);
		pixbuf = scaled;
	}

	if (pixbuf != NULL) {
		g_hash_table_insert (cache, key, g_object_ref (pixbuf));
	} else {
		g_free (key);
	}

	return pixbuf;
}

void
nemo_shell_icon_win32_clear_cache (void)
{
	if (cache != NULL) {
		g_hash_table_remove_all (cache);
	}
}
