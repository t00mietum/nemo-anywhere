/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-appearance.h - light/dark mode and theme selection.

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

/* Three settings drive everything here: appearance.mode (system/light/dark),
 * appearance.gtk-theme and appearance.icon-theme. "system" defers to whatever
 * the platform reports - the Windows AppsUseLightTheme registry value, and on
 * a desktop that already told GTK, GTK's own answer - so it is a no-op where
 * the desktop is already in charge.
 *
 * Themes come from three places, searched user-first: the user's own drop-in
 * folder, the folder installed beside the app, and whatever GTK already knows
 * about. Each is offered to the picker only for the backgrounds it was drawn
 * for; see nemo_appearance_list_themes.
 */

#ifndef NEMO_APPEARANCE_H
#define NEMO_APPEARANCE_H

#include <glib.h>

G_BEGIN_DECLS

typedef enum {
	NEMO_APPEARANCE_MODE_SYSTEM = 0,
	NEMO_APPEARANCE_MODE_LIGHT  = 1,
	NEMO_APPEARANCE_MODE_DARK   = 2
} NemoAppearanceMode;

/* Which backgrounds a theme suits. A theme with no opinion claims both. */
typedef enum {
	NEMO_THEME_FITS_LIGHT = 1 << 0,
	NEMO_THEME_FITS_DARK  = 1 << 1,
	NEMO_THEME_FITS_BOTH  = (1 << 0) | (1 << 1)
} NemoThemeFit;

typedef enum {
	NEMO_THEME_KIND_WIDGET,
	NEMO_THEME_KIND_ICON
} NemoThemeKind;

typedef struct {
	char     *name;			/* directory name - what GTK is told */
	char     *display;		/* label for the picker */
	char     *style;		/* the look it imitates, e.g. "Windows 11" */
	char     *dir;			/* directory holding it, or NULL if GTK found it */
	char     *counterpart;		/* theme to swap to when the mode flips */
	guint     fits;			/* NemoThemeFit bits */
	gboolean  declared;		/* @fits is the theme's own answer, not a guess */
	/* Found in nemo's own folder beside the app rather than anywhere GTK
	 * looks. Note the bundled set installs into share/themes and share/icons
	 * so GTK can resolve it by name, so this is FALSE for most of it. */
	gboolean  bundled;
} NemoThemeInfo;

void     nemo_appearance_init      (void);
void     nemo_appearance_shutdown  (void);

/* The mode actually in force, with "system" already resolved. */
gboolean nemo_appearance_is_dark   (void);

/* Themes that suit @fits, newly allocated, sorted by display name. Pass
 * NEMO_THEME_FITS_BOTH to list everything. Free with
 * g_list_free_full (list, (GDestroyNotify) nemo_theme_info_free). */
GList   *nemo_appearance_list_themes (NemoThemeKind kind, guint fits);

void     nemo_theme_info_free      (NemoThemeInfo *info);

/* Whichever of @name's variants suits the mode in force, or @name itself.
 * Newly allocated; NULL in, NULL out. */
char    *nemo_appearance_theme_for_mode (NemoThemeKind kind, const char *name);

/* Where drop-in themes go, user first, NULL-terminated. Owned by us. */
const char * const *nemo_appearance_get_theme_roots (void);

G_END_DECLS

#endif /* NEMO_APPEARANCE_H */
