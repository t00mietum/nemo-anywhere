/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-metadata-store.c - app-owned per-file metadata store.

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
#include "nemo-metadata-store.h"

#include <string.h>
#include <json-glib/json-glib.h>

#include "nemo-file-utilities.h"

#define STORE_FILE_NAME "metadata.json"
#define SAVE_DELAY_SECONDS 2

/* uri -> (key -> MetaValue). Mutations can come from file-operation
 * worker threads (via the move hook), so everything under the mutex. */
static GHashTable *store = NULL;
static GMutex store_mutex;
static guint save_timeout_id = 0;
static gboolean dirty = FALSE;

typedef struct {
	char  *value;   /* string entry */
	char **values;  /* list entry; exactly one of the two is set */
} MetaValue;

static void
meta_value_free (gpointer data)
{
	MetaValue *mv = data;

	g_free (mv->value);
	g_strfreev (mv->values);
	g_free (mv);
}

static MetaValue *
meta_value_new_string (const char *value)
{
	MetaValue *mv = g_new0 (MetaValue, 1);
	mv->value = g_strdup (value);
	return mv;
}

static MetaValue *
meta_value_new_stringv (char **values)
{
	MetaValue *mv = g_new0 (MetaValue, 1);
	mv->values = g_strdupv (values);
	return mv;
}

static GHashTable *
new_file_table (void)
{
	return g_hash_table_new_full (g_str_hash, g_str_equal,
				      g_free, meta_value_free);
}

static char *
store_path (void)
{
	char *dir, *path;

	dir = nemo_get_user_directory ();
	path = g_build_filename (dir, STORE_FILE_NAME, NULL);
	g_free (dir);

	return path;
}

/* caller holds the mutex */
static void
ensure_loaded (void)
{
	JsonParser *parser;
	JsonNode *root;
	char *path;

	if (store != NULL) {
		return;
	}

	store = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
				       (GDestroyNotify) g_hash_table_destroy);

	path = store_path ();
	parser = json_parser_new ();

	if (json_parser_load_from_file (parser, path, NULL) &&
	    (root = json_parser_get_root (parser)) != NULL &&
	    JSON_NODE_HOLDS_OBJECT (root)) {
		JsonObjectIter file_iter;
		const char *uri;
		JsonNode *file_node;

		json_object_iter_init (&file_iter, json_node_get_object (root));
		while (json_object_iter_next (&file_iter, &uri, &file_node)) {
			GHashTable *file_table;
			JsonObjectIter key_iter;
			const char *key;
			JsonNode *value_node;

			if (!JSON_NODE_HOLDS_OBJECT (file_node)) {
				continue;
			}

			file_table = new_file_table ();

			json_object_iter_init (&key_iter, json_node_get_object (file_node));
			while (json_object_iter_next (&key_iter, &key, &value_node)) {
				if (JSON_NODE_HOLDS_VALUE (value_node) &&
				    json_node_get_value_type (value_node) == G_TYPE_STRING) {
					g_hash_table_replace (file_table, g_strdup (key),
							      meta_value_new_string (json_node_get_string (value_node)));
				} else if (JSON_NODE_HOLDS_ARRAY (value_node)) {
					JsonArray *array = json_node_get_array (value_node);
					guint len = json_array_get_length (array);
					char **values = g_new0 (char *, len + 1);
					guint i, n = 0;
					MetaValue *mv;

					for (i = 0; i < len; i++) {
						JsonNode *el = json_array_get_element (array, i);
						if (JSON_NODE_HOLDS_VALUE (el) &&
						    json_node_get_value_type (el) == G_TYPE_STRING) {
							values[n++] = g_strdup (json_node_get_string (el));
						}
					}

					mv = g_new0 (MetaValue, 1);
					mv->values = values;
					g_hash_table_replace (file_table, g_strdup (key), mv);
				}
			}

			if (g_hash_table_size (file_table) > 0) {
				g_hash_table_replace (store, g_strdup (uri), file_table);
			} else {
				g_hash_table_destroy (file_table);
			}
		}
	}

	g_object_unref (parser);
	g_free (path);
}

/* caller holds the mutex */
static void
save_now (void)
{
	JsonBuilder *builder;
	JsonGenerator *generator;
	JsonNode *root;
	GHashTableIter file_iter;
	gpointer uri, table;
	char *path, *data;
	gsize length;

	if (!dirty || store == NULL) {
		return;
	}

	builder = json_builder_new ();
	json_builder_begin_object (builder);

	g_hash_table_iter_init (&file_iter, store);
	while (g_hash_table_iter_next (&file_iter, &uri, &table)) {
		GHashTableIter key_iter;
		gpointer key, value;

		json_builder_set_member_name (builder, (const char *) uri);
		json_builder_begin_object (builder);

		g_hash_table_iter_init (&key_iter, (GHashTable *) table);
		while (g_hash_table_iter_next (&key_iter, &key, &value)) {
			MetaValue *mv = value;

			json_builder_set_member_name (builder, (const char *) key);
			if (mv->values != NULL) {
				int i;

				json_builder_begin_array (builder);
				for (i = 0; mv->values[i] != NULL; i++) {
					json_builder_add_string_value (builder, mv->values[i]);
				}
				json_builder_end_array (builder);
			} else {
				json_builder_add_string_value (builder, mv->value);
			}
		}

		json_builder_end_object (builder);
	}

	json_builder_end_object (builder);

	root = json_builder_get_root (builder);
	generator = json_generator_new ();
	json_generator_set_root (generator, root);
	data = json_generator_to_data (generator, &length);

	path = store_path ();
	g_file_set_contents (path, data, length, NULL);

	dirty = FALSE;

	g_free (path);
	g_free (data);
	json_node_unref (root);
	g_object_unref (generator);
	g_object_unref (builder);
}

static gboolean
save_timeout_callback (gpointer user_data)
{
	g_mutex_lock (&store_mutex);
	save_timeout_id = 0;
	save_now ();
	g_mutex_unlock (&store_mutex);

	return G_SOURCE_REMOVE;
}

/* caller holds the mutex */
static void
schedule_save (void)
{
	dirty = TRUE;
	if (save_timeout_id == 0) {
		save_timeout_id = g_timeout_add_seconds (SAVE_DELAY_SECONDS,
							 save_timeout_callback, NULL);
	}
}

/* caller holds the mutex; removes the uri entry when its table empties */
static void
remove_key (const char *uri, const char *key)
{
	GHashTable *file_table;

	file_table = g_hash_table_lookup (store, uri);
	if (file_table == NULL) {
		return;
	}

	if (g_hash_table_remove (file_table, key)) {
		if (g_hash_table_size (file_table) == 0) {
			g_hash_table_remove (store, uri);
		}
		schedule_save ();
	}
}

void
nemo_metadata_store_set_string (const char *uri,
				const char *key,
				const char *value)
{
	GHashTable *file_table;
	MetaValue *old;

	g_return_if_fail (uri != NULL && key != NULL);

	g_mutex_lock (&store_mutex);
	ensure_loaded ();

	if (value == NULL) {
		remove_key (uri, key);
		g_mutex_unlock (&store_mutex);
		return;
	}

	file_table = g_hash_table_lookup (store, uri);
	if (file_table == NULL) {
		file_table = new_file_table ();
		g_hash_table_replace (store, g_strdup (uri), file_table);
	}

	old = g_hash_table_lookup (file_table, key);
	if (old == NULL || old->value == NULL || strcmp (old->value, value) != 0) {
		g_hash_table_replace (file_table, g_strdup (key),
				      meta_value_new_string (value));
		schedule_save ();
	}

	g_mutex_unlock (&store_mutex);
}

void
nemo_metadata_store_set_stringv (const char *uri,
				 const char *key,
				 char      **values)
{
	GHashTable *file_table;
	MetaValue *old;

	g_return_if_fail (uri != NULL && key != NULL);

	g_mutex_lock (&store_mutex);
	ensure_loaded ();

	if (values == NULL) {
		remove_key (uri, key);
		g_mutex_unlock (&store_mutex);
		return;
	}

	file_table = g_hash_table_lookup (store, uri);
	if (file_table == NULL) {
		file_table = new_file_table ();
		g_hash_table_replace (store, g_strdup (uri), file_table);
	}

	old = g_hash_table_lookup (file_table, key);
	if (old == NULL || old->values == NULL ||
	    !g_strv_equal ((const char * const *) old->values,
			   (const char * const *) values)) {
		g_hash_table_replace (file_table, g_strdup (key),
				      meta_value_new_stringv (values));
		schedule_save ();
	}

	g_mutex_unlock (&store_mutex);
}

void
nemo_metadata_store_apply_to_info (const char *uri,
				   GFileInfo  *info)
{
	GHashTable *file_table;
	GHashTableIter iter;
	gpointer key, value;

	g_return_if_fail (uri != NULL && G_IS_FILE_INFO (info));

	g_mutex_lock (&store_mutex);
	ensure_loaded ();

	file_table = g_hash_table_lookup (store, uri);
	if (file_table != NULL) {
		g_hash_table_iter_init (&iter, file_table);
		while (g_hash_table_iter_next (&iter, &key, &value)) {
			MetaValue *mv = value;
			char *attr;

			attr = g_strconcat ("metadata::", (const char *) key, NULL);
			if (mv->values != NULL) {
				g_file_info_set_attribute_stringv (info, attr, mv->values);
			} else {
				g_file_info_set_attribute_string (info, attr, mv->value);
			}
			g_free (attr);
		}
	}

	g_mutex_unlock (&store_mutex);
}

void
nemo_metadata_store_rename (const char *old_uri,
			    const char *new_uri)
{
	GHashTableIter iter;
	gpointer key, value;
	gpointer orig_key, orig_value;
	char *old_prefix;
	GPtrArray *moved_uris;
	gsize prefix_len;
	guint i;
	gboolean changed = FALSE;

	g_return_if_fail (old_uri != NULL && new_uri != NULL);

	if (strcmp (old_uri, new_uri) == 0) {
		return;
	}

	g_mutex_lock (&store_mutex);
	ensure_loaded ();

	/* the entry itself */
	if (g_hash_table_steal_extended (store, old_uri, &orig_key, &orig_value)) {
		g_free (orig_key);
		g_hash_table_replace (store, g_strdup (new_uri), orig_value);
		changed = TRUE;
	}

	/* descendants, when a directory moved */
	old_prefix = g_strconcat (old_uri, "/", NULL);
	prefix_len = strlen (old_prefix);
	moved_uris = g_ptr_array_new_with_free_func (g_free);

	g_hash_table_iter_init (&iter, store);
	while (g_hash_table_iter_next (&iter, &key, &value)) {
		if (g_str_has_prefix ((const char *) key, old_prefix)) {
			g_ptr_array_add (moved_uris, g_strdup ((const char *) key));
		}
	}

	for (i = 0; i < moved_uris->len; i++) {
		const char *child_old = g_ptr_array_index (moved_uris, i);
		char *child_new;

		if (!g_hash_table_steal_extended (store, child_old, &orig_key, &orig_value)) {
			continue;
		}
		g_free (orig_key);

		child_new = g_strconcat (new_uri, "/", child_old + prefix_len, NULL);
		g_hash_table_replace (store, child_new, orig_value);
		changed = TRUE;
	}

	if (changed) {
		schedule_save ();
	}

	g_ptr_array_unref (moved_uris);
	g_free (old_prefix);
	g_mutex_unlock (&store_mutex);
}

void
nemo_metadata_store_flush (void)
{
	g_mutex_lock (&store_mutex);
	if (save_timeout_id != 0) {
		g_source_remove (save_timeout_id);
		save_timeout_id = 0;
	}
	save_now ();
	g_mutex_unlock (&store_mutex);
}
