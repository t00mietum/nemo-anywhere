/* nemo-favorites.h - favorite-files store, backed by GSettings.
 *
 * Adapted from libxapp 2.8.8 (xapp-favorites.h, LGPL-2.1-or-later,
 * © Linux Mint team), relicensed under GPL-2.0 per LGPL-2.1 section 3.
 * Launch/menu helpers dropped; storage moved to the org.nemo-anywhere schema.
 */

#ifndef __NEMO_FAVORITES_H__
#define __NEMO_FAVORITES_H__

#include <glib-object.h>
#include <gio/gio.h>

G_BEGIN_DECLS

#define NEMO_TYPE_FAVORITE_INFO (nemo_favorite_info_get_type ())
typedef struct _NemoFavoriteInfo NemoFavoriteInfo;

#define NEMO_TYPE_FAVORITES           (nemo_favorites_get_type ())

G_DECLARE_FINAL_TYPE (NemoFavorites, nemo_favorites, NEMO, FAVORITES, GObject)

NemoFavorites        *nemo_favorites_get_default            (void);
GList                *nemo_favorites_get_favorites          (NemoFavorites       *favorites,
                                                             const gchar * const *mimetypes);
gint                  nemo_favorites_get_n_favorites        (NemoFavorites *favorites);
NemoFavoriteInfo     *nemo_favorites_find_by_display_name   (NemoFavorites *favorites,
                                                             const gchar   *display_name);
NemoFavoriteInfo     *nemo_favorites_find_by_uri            (NemoFavorites *favorites,
                                                             const gchar   *uri);
void                  nemo_favorites_add                    (NemoFavorites *favorites,
                                                             const gchar   *uri);
void                  nemo_favorites_remove                 (NemoFavorites *favorites,
                                                             const gchar   *uri);
void                  nemo_favorites_rename                 (NemoFavorites *favorites,
                                                             const gchar   *old_uri,
                                                             const gchar   *new_uri);

/**
 * NemoFavoriteInfo:
 * @uri: The uri to the favorite file.
 * @display_name: The name for use when displaying the item. This may not exactly match
 * the filename if there are files with the same name but in different folders.
 * @cached_mimetype: The mimetype calculated for the uri when it was added to favorites.
 *
 * Information related to a single favorite file.
 */
struct _NemoFavoriteInfo
{
    gchar *uri;
    gchar *display_name;
    gchar *cached_mimetype;
};

GType             nemo_favorite_info_get_type (void) G_GNUC_CONST;
NemoFavoriteInfo *nemo_favorite_info_copy     (const NemoFavoriteInfo *info);
void              nemo_favorite_info_free     (NemoFavoriteInfo *info);

G_END_DECLS

#endif  /* __NEMO_FAVORITES_H__ */
