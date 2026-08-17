/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-config.c - app-owned settings store.

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

#include "nemo-config.h"

#include <gio/gio.h>
#include <string.h>

#define SHCL_IMPLEMENTATION
#include "shcl.h"

#include "nemo-config-keys.h"
#include "nemo-file-utilities.h"

#define CONFIG_FILE_NAME      "settings.shcl"
#define SAVE_DEBOUNCE_SECONDS 2

struct _NemoConfigGroup {
	GObject  parent_instance;
	char    *name;
};

G_DEFINE_TYPE (NemoConfigGroup, nemo_config_group, G_TYPE_OBJECT)

enum {
	CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL];

/* One document for the whole app. The lock is not decoration: file
 * operations write favorites and view state from worker threads. */
static GMutex      config_lock;
static shcl_doc   *config_doc;
static char       *config_path;
static guint       save_timeout_id;
static GFileMonitor *config_monitor;
static char       *last_written;       /* what we last put on disk, to ignore our own event */
static gsize       last_written_len;   /* byte length - the file may legally hold a NUL */
static GHashTable *config_groups;      /* name -> NemoConfigGroup (owned) */
static gboolean    config_ready;

static void schedule_save (void);
static void emit_changed  (const char *group, const char *key);

/* ---- Key table ---- */

static const NemoConfigKey *
find_key (const char *group, const char *key)
{
	const NemoConfigKey *k;

	if (group == NULL)
		group = "";

	for (k = nemo_config_keys; k->key != NULL; k++) {
		if (g_strcmp0 (k->group, group) == 0 && g_strcmp0 (k->key, key) == 0)
			return k;
	}
	return NULL;
}

static const NemoConfigKey *
require_key (NemoConfigGroup *group, const char *key, NemoConfigType type)
{
	const NemoConfigKey *k;

	/* Almost always means nemo_global_preferences_init() has not run, so
	 * say that rather than blaming the key. */
	if (group == NULL) {
		g_critical ("nemo-config: '%s' read before the config store was opened", key);
		return NULL;
	}

	k = find_key (group->name, key);
	if (k == NULL) {
		g_critical ("nemo-config: no such key '%s' in group '%s'",
		            key, group->name);
		return NULL;
	}
	/* An enum is stored as its nick, so reading one as a string is fine. */
	if (k->type != type && !(k->type == NEMO_CONFIG_ENUM && type == NEMO_CONFIG_STRING)) {
		g_critical ("nemo-config: key '%s.%s' read as the wrong type",
		            k->group, k->key);
		return NULL;
	}
	return k;
}

/* "group.key", or bare "key" for the file root. Caller frees. */
static char *
key_path (const NemoConfigKey *k)
{
	if (k->group == NULL || *k->group == '\0')
		return g_strdup (k->key);
	return g_strdup_printf ("%s.%s", k->group, k->key);
}

/* ---- Load / save ---- */

static char *
build_config_path (void)
{
	char *dir  = nemo_get_user_directory ();
	char *path = g_build_filename (dir, CONFIG_FILE_NAME, NULL);

	g_free (dir);
	return path;
}

static void
load_locked (void)
{
	char   *text = NULL;
	gsize   len  = 0;
	GError *error = NULL;

	if (g_file_get_contents (config_path, &text, &len, &error)) {
		size_t i, n;

		shcl_free (config_doc);
		config_doc = shcl_parse (text, len);

		/* A broken line is skipped, not fatal - say which, once, so a
		 * hand-edit that lost a value is not a silent mystery. */
		n = shcl_diag_count (config_doc);
		for (i = 0; i < n && i < 10; i++) {
			shcl_str m = shcl_diag_message (config_doc, i);
			if (shcl_diag_severity (config_doc, i) != SHCL_SEV_ERROR)
				continue;
			g_warning ("nemo-config: %s line %zu [%s] %.*s",
			           config_path, shcl_diag_line (config_doc, i),
			           shcl_diag_code (config_doc, i), (int) m.n, m.p);
		}

		g_free (last_written);
		last_written = g_memdup2 (text, len);
		last_written_len = len;
		g_free (text);
	} else {
		gboolean gone = g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT);

		if (!gone)
			g_warning ("nemo-config: cannot read %s: %s",
			           config_path, error->message);
		g_clear_error (&error);

		/* A transient failure (AV/sync/editor lock, the delete half of a
		 * non-atomic external save) must not swap defaults into memory: a
		 * queued save would then write that near-empty doc over the real
		 * file. Keep what we have; only a genuinely absent file resets. */
		if (!gone && config_doc != NULL)
			return;

		shcl_free (config_doc);
		config_doc = shcl_parse ("", 0);
	}
}

static gboolean
save_now (gpointer data)
{
	char     *text = NULL;
	char     *dir;
	gsize     text_len;
	GError   *error = NULL;
	shcl_str  canon;

	g_mutex_lock (&config_lock);
	save_timeout_id = 0;

	canon = shcl_to_canonical (config_doc);
	/* By length, and g_memdup2 rather than g_strndup (which stops at a NUL
	 * and pads): SHCL is NUL-transparent, so a NUL that came in from the
	 * file must not truncate the write or the own-write check. */
	text = g_memdup2 (canon.p, canon.n);
	text_len = canon.n;

	g_free (last_written);
	last_written = g_memdup2 (text, text_len);
	last_written_len = text_len;
	g_mutex_unlock (&config_lock);

	dir = g_path_get_dirname (config_path);
	g_mkdir_with_parents (dir, 0700);
	g_free (dir);

	if (!g_file_set_contents (config_path, text, text_len, &error)) {
		g_warning ("nemo-config: cannot write %s: %s",
		           config_path, error->message);
		g_clear_error (&error);
	}
	g_free (text);
	return G_SOURCE_REMOVE;
}

/* Called with the lock held. */
static void
schedule_save (void)
{
	if (save_timeout_id != 0)
		return;
	save_timeout_id = g_timeout_add_seconds (SAVE_DEBOUNCE_SECONDS, save_now, NULL);
}

/* ---- Reload on external edit ---- */

/* Snapshot every declared key as text, so an external edit can be turned
 * into the same per-key change signals a set() would have produced. */
static GHashTable *
snapshot_locked (void)
{
	GHashTable          *out = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                                  g_free, g_free);
	const NemoConfigKey *k;

	for (k = nemo_config_keys; k->key != NULL; k++) {
		char          *path = key_path (k);
		/* read_string, not read_raw: raw only answers for fenced blocks and
		 * reports every ordinary value as empty, which made the whole diff
		 * blind to a changed value and only able to see a key appear or go. */
		shcl_read_str  r    = shcl_read_string (config_doc, path, strlen (path));

		if (r.status == SHCL_NOT_FOUND) {
			g_free (path);
			continue;
		}
		g_hash_table_insert (out, path, g_strndup (r.value.p, r.value.n));
	}
	return out;
}

static void
config_file_changed (GFileMonitor      *monitor,
                     GFile             *file,
                     GFile             *other,
                     GFileMonitorEvent  event,
                     gpointer           data)
{
	GHashTable          *before, *after;
	const NemoConfigKey *k;
	char                *text = NULL;
	gsize                len = 0;

	if (event != G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT &&
	    event != G_FILE_MONITOR_EVENT_CREATED &&
	    event != G_FILE_MONITOR_EVENT_DELETED)
		return;

	/* Our own write comes back as an event; ignore it. */
	if (g_file_get_contents (config_path, &text, &len, NULL)) {
		gboolean ours;
		g_mutex_lock (&config_lock);
		ours = (last_written != NULL && last_written_len == len &&
		        memcmp (last_written, text, len) == 0);
		g_mutex_unlock (&config_lock);
		g_free (text);
		if (ours)
			return;
	}

	g_mutex_lock (&config_lock);
	before = snapshot_locked ();
	load_locked ();
	after = snapshot_locked ();
	g_mutex_unlock (&config_lock);

	for (k = nemo_config_keys; k->key != NULL; k++) {
		char       *path = key_path (k);
		const char *a = g_hash_table_lookup (before, path);
		const char *b = g_hash_table_lookup (after, path);

		if (g_strcmp0 (a, b) != 0)
			emit_changed (k->group, k->key);
		g_free (path);
	}

	g_hash_table_destroy (before);
	g_hash_table_destroy (after);
}

/* ---- Lifecycle ---- */

void
nemo_config_init (void)
{
	GFile *file;

	if (config_ready)
		return;

	g_mutex_init (&config_lock);
	config_path   = build_config_path ();
	config_groups = g_hash_table_new_full (g_str_hash, g_str_equal,
	                                       g_free, g_object_unref);

	g_mutex_lock (&config_lock);
	load_locked ();
	g_mutex_unlock (&config_lock);

	/* Hand-editing the file is the point, so pick edits up while we run. */
	file = g_file_new_for_path (config_path);
	config_monitor = g_file_monitor_file (file, G_FILE_MONITOR_NONE, NULL, NULL);
	if (config_monitor != NULL)
		g_signal_connect (config_monitor, "changed",
		                  G_CALLBACK (config_file_changed), NULL);
	g_object_unref (file);

	config_ready = TRUE;
}

void
nemo_config_flush (void)
{
	if (!config_ready || save_timeout_id == 0)
		return;

	g_source_remove (save_timeout_id);
	save_timeout_id = 0;
	save_now (NULL);
}

void
nemo_config_shutdown (void)
{
	if (!config_ready)
		return;

	nemo_config_flush ();
	g_clear_object (&config_monitor);
}

char *
nemo_config_get_path (void)
{
	return g_strdup (config_path);
}

NemoConfigGroup *
nemo_config_get_group (const char *group)
{
	NemoConfigGroup *g;

	if (group == NULL)
		group = "";

	g_return_val_if_fail (config_ready, NULL);

	g = g_hash_table_lookup (config_groups, group);
	if (g == NULL) {
		g = g_object_new (NEMO_TYPE_CONFIG_GROUP, NULL);
		g->name = g_strdup (group);
		g_hash_table_insert (config_groups, g_strdup (group), g);
	}
	return g;
}

static void
emit_changed (const char *group, const char *key)
{
	NemoConfigGroup *g;

	if (group == NULL)
		group = "";

	g = g_hash_table_lookup (config_groups, group);
	if (g != NULL)
		g_signal_emit (g, signals[CHANGED], g_quark_from_string (key), key);
}

static void
nemo_config_group_finalize (GObject *object)
{
	NemoConfigGroup *self = NEMO_CONFIG_GROUP (object);

	g_free (self->name);
	G_OBJECT_CLASS (nemo_config_group_parent_class)->finalize (object);
}

static void
nemo_config_group_class_init (NemoConfigGroupClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = nemo_config_group_finalize;

	/* Detailed, so "changed::some-key" works exactly as it used to. */
	signals[CHANGED] =
		g_signal_new ("changed", NEMO_TYPE_CONFIG_GROUP,
		              G_SIGNAL_RUN_LAST | G_SIGNAL_DETAILED,
		              0, NULL, NULL, NULL,
		              G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void
nemo_config_group_init (NemoConfigGroup *self)
{
}

/* ---- Reads ---- */

/* A missing key falls back to the declared default. An empty one does not:
 * "set to nothing" is a real value, and conflating the two would make an
 * emptied list spring back to its default. */
gboolean
nemo_config_get_boolean (NemoConfigGroup *group, const char *key)
{
	const NemoConfigKey *k = require_key (group, key, NEMO_CONFIG_BOOL);
	char                *path;
	gboolean             fallback, out;

	if (k == NULL)
		return FALSE;

	fallback = (g_strcmp0 (k->def, "true") == 0);
	path = key_path (k);

	g_mutex_lock (&config_lock);
	out = shcl_get_bool (config_doc, path, strlen (path), fallback) ? TRUE : FALSE;
	g_mutex_unlock (&config_lock);

	g_free (path);
	return out;
}

gint
nemo_config_get_int (NemoConfigGroup *group, const char *key)
{
	const NemoConfigKey *k = require_key (group, key, NEMO_CONFIG_INT);
	char                *path;
	gint64               out;

	if (k == NULL)
		return 0;

	path = key_path (k);
	g_mutex_lock (&config_lock);
	out = shcl_get_int (config_doc, path, strlen (path),
	                    k->def ? g_ascii_strtoll (k->def, NULL, 10) : 0);
	g_mutex_unlock (&config_lock);

	g_free (path);
	return (gint) out;
}

gdouble
nemo_config_get_double (NemoConfigGroup *group, const char *key)
{
	const NemoConfigKey *k = require_key (group, key, NEMO_CONFIG_FLOAT);
	char                *path;
	gdouble              out;

	if (k == NULL)
		return 0.0;

	path = key_path (k);
	g_mutex_lock (&config_lock);
	out = shcl_get_float (config_doc, path, strlen (path),
	                      k->def ? g_ascii_strtod (k->def, NULL) : 0.0);
	g_mutex_unlock (&config_lock);

	g_free (path);
	return out;
}

char *
nemo_config_get_string (NemoConfigGroup *group, const char *key)
{
	const NemoConfigKey *k = require_key (group, key, NEMO_CONFIG_STRING);
	char                *path, *out;
	shcl_read_str        r;

	if (k == NULL)
		return g_strdup ("");

	path = key_path (k);
	g_mutex_lock (&config_lock);
	r = shcl_read_string (config_doc, path, strlen (path));
	if (r.status == SHCL_GOOD)
		out = g_strndup (r.value.p, r.value.n);
	else if (r.status == SHCL_EMPTY)
		out = g_strdup ("");
	else
		out = g_strdup (k->def ? k->def : "");
	g_mutex_unlock (&config_lock);

	g_free (path);
	return out;
}

char **
nemo_config_get_strv (NemoConfigGroup *group, const char *key)
{
	const NemoConfigKey *k = require_key (group, key, NEMO_CONFIG_STRING_LIST);
	char                *path;
	char               **out;
	shcl_read_str_arr    r;

	if (k == NULL)
		return g_new0 (char *, 1);

	path = key_path (k);
	g_mutex_lock (&config_lock);
	r = shcl_read_string_array (config_doc, path, strlen (path));

	if (r.status == SHCL_NOT_FOUND) {
		out = k->def_list ? g_strdupv ((char **) k->def_list)
		                  : g_new0 (char *, 1);
	} else {
		size_t i;
		out = g_new0 (char *, r.n + 1);
		for (i = 0; i < r.n; i++)
			out[i] = g_strndup (r.values[i].p, r.values[i].n);
	}
	g_mutex_unlock (&config_lock);

	g_free (path);
	return out;
}

gint
nemo_config_get_enum (NemoConfigGroup *group, const char *key)
{
	const NemoConfigKey       *k = require_key (group, key, NEMO_CONFIG_ENUM);
	const NemoConfigEnumValue *v;
	char                      *path, *nick = NULL;
	shcl_read_str              r;
	gint                       out = 0;

	if (k == NULL)
		return 0;

	path = key_path (k);
	g_mutex_lock (&config_lock);
	r = shcl_read_string (config_doc, path, strlen (path));
	if (r.status == SHCL_GOOD)
		nick = g_strndup (r.value.p, r.value.n);
	g_mutex_unlock (&config_lock);
	g_free (path);

	if (nick == NULL)
		nick = g_strdup (k->def);

	for (v = k->enum_values; v != NULL && v->nick != NULL; v++) {
		if (g_strcmp0 (v->nick, nick) == 0) {
			g_free (nick);
			return v->value;
		}
	}

	/* An unrecognized nick is a hand-edit typo - fall back to the default
	 * rather than to whatever 0 happens to mean. */
	g_warning ("nemo-config: '%s' is not a valid value for '%s.%s'",
	           nick, k->group, k->key);
	g_free (nick);
	for (v = k->enum_values; v != NULL && v->nick != NULL; v++) {
		if (g_strcmp0 (v->nick, k->def) == 0)
			out = v->value;
	}
	return out;
}

/* ---- Writes ---- */

/* Storing a value that equals the default would pin the key: a later change
 * to that default could no longer reach the user. Drop it instead, which
 * also keeps the file down to what was actually chosen. */
static gboolean
drop_if_default (const NemoConfigKey *k, const char *as_text, char *path)
{
	if (k->def == NULL || g_strcmp0 (k->def, as_text) != 0)
		return FALSE;

	shcl_remove (config_doc, path, strlen (path));
	return TRUE;
}

/* Only for a key that wasn't in the file yet: set_comment pushes another
 * leading line each time it's called, so re-commenting on every write would
 * grow the same line forever. An existing key already carries its comment. */
static void
apply_comment_if_new (const NemoConfigKey *k, const char *path, gboolean existed)
{
	if (k->summary == NULL || existed)
		return;
	shcl_set_comment (config_doc, path, strlen (path),
	                  k->summary, strlen (k->summary));
}

void
nemo_config_set_boolean (NemoConfigGroup *group, const char *key, gboolean value)
{
	const NemoConfigKey *k = require_key (group, key, NEMO_CONFIG_BOOL);
	char                *path;
	gboolean             existed;

	if (k == NULL)
		return;

	path = key_path (k);
	g_mutex_lock (&config_lock);
	existed = shcl_exists (config_doc, path, strlen (path)) != 0;
	if (!drop_if_default (k, value ? "true" : "false", path)) {
		shcl_set_bool (config_doc, path, strlen (path), value ? 1 : 0);
		apply_comment_if_new (k, path, existed);
	}
	schedule_save ();
	g_mutex_unlock (&config_lock);
	g_free (path);

	emit_changed (k->group, k->key);
}

void
nemo_config_set_int (NemoConfigGroup *group, const char *key, gint value)
{
	const NemoConfigKey *k = require_key (group, key, NEMO_CONFIG_INT);
	char                *path, *text;
	gboolean             existed;

	if (k == NULL)
		return;

	path = key_path (k);
	text = g_strdup_printf ("%d", value);
	g_mutex_lock (&config_lock);
	existed = shcl_exists (config_doc, path, strlen (path)) != 0;
	if (!drop_if_default (k, text, path)) {
		shcl_set_int (config_doc, path, strlen (path), value);
		apply_comment_if_new (k, path, existed);
	}
	schedule_save ();
	g_mutex_unlock (&config_lock);
	g_free (text);
	g_free (path);

	emit_changed (k->group, k->key);
}

void
nemo_config_set_double (NemoConfigGroup *group, const char *key, gdouble value)
{
	const NemoConfigKey *k = require_key (group, key, NEMO_CONFIG_FLOAT);
	char                *path;
	gboolean             existed;

	if (k == NULL)
		return;

	path = key_path (k);
	g_mutex_lock (&config_lock);
	existed = shcl_exists (config_doc, path, strlen (path)) != 0;
	shcl_set_float (config_doc, path, strlen (path), value);
	apply_comment_if_new (k, path, existed);
	schedule_save ();
	g_mutex_unlock (&config_lock);
	g_free (path);

	emit_changed (k->group, k->key);
}

void
nemo_config_set_string (NemoConfigGroup *group, const char *key, const char *value)
{
	const NemoConfigKey *k = require_key (group, key, NEMO_CONFIG_STRING);
	char                *path;
	gboolean             existed;

	if (k == NULL)
		return;
	if (value == NULL)
		value = "";

	path = key_path (k);
	g_mutex_lock (&config_lock);
	existed = shcl_exists (config_doc, path, strlen (path)) != 0;
	if (!drop_if_default (k, value, path)) {
		shcl_set_string (config_doc, path, strlen (path), value, strlen (value));
		apply_comment_if_new (k, path, existed);
	}
	schedule_save ();
	g_mutex_unlock (&config_lock);
	g_free (path);

	emit_changed (k->group, k->key);
}

void
nemo_config_set_strv (NemoConfigGroup *group, const char *key, const char *const *value)
{
	const NemoConfigKey *k = require_key (group, key, NEMO_CONFIG_STRING_LIST);
	char                *path;
	gsize                n = 0, i;
	size_t              *lens;
	gboolean             existed;

	if (k == NULL)
		return;

	while (value != NULL && value[n] != NULL)
		n++;

	/* Same as the scalars: a list matching the default is not stored. */
	if (k->def_list != NULL) {
		gsize dn = 0;
		while (k->def_list[dn] != NULL)
			dn++;
		if (dn == n) {
			gboolean same = TRUE;
			for (i = 0; i < n; i++) {
				if (g_strcmp0 (k->def_list[i], value[i]) != 0) {
					same = FALSE;
					break;
				}
			}
			if (same) {
				path = key_path (k);
				g_mutex_lock (&config_lock);
				shcl_remove (config_doc, path, strlen (path));
				schedule_save ();
				g_mutex_unlock (&config_lock);
				g_free (path);
				emit_changed (k->group, k->key);
				return;
			}
		}
	}

	lens = g_new0 (size_t, n + 1);
	for (i = 0; i < n; i++)
		lens[i] = strlen (value[i]);

	path = key_path (k);
	g_mutex_lock (&config_lock);
	existed = shcl_exists (config_doc, path, strlen (path)) != 0;
	shcl_set_string_array (config_doc, path, strlen (path),
	                       (const char *const *) value, lens, n);
	apply_comment_if_new (k, path, existed);
	schedule_save ();
	g_mutex_unlock (&config_lock);
	g_free (lens);
	g_free (path);

	emit_changed (k->group, k->key);
}

/* The file stores the nick, so both entry points below end up here. */
static void
store_enum_nick (const NemoConfigKey *k, const char *nick)
{
	char     *path = key_path (k);
	gboolean  existed;

	g_mutex_lock (&config_lock);
	existed = shcl_exists (config_doc, path, strlen (path)) != 0;
	if (!drop_if_default (k, nick, path)) {
		shcl_set_string (config_doc, path, strlen (path), nick, strlen (nick));
		apply_comment_if_new (k, path, existed);
	}
	schedule_save ();
	g_mutex_unlock (&config_lock);
	g_free (path);

	emit_changed (k->group, k->key);
}

/* Set by nick rather than by number, for callers that already have the nick
 * (the bind set-mappings all do). Unknown nicks are refused, not stored. */
static void
set_enum_by_nick (NemoConfigGroup *group, const char *key, const char *nick)
{
	const NemoConfigKey       *k = require_key (group, key, NEMO_CONFIG_ENUM);
	const NemoConfigEnumValue *v;

	if (k == NULL)
		return;

	for (v = k->enum_values; v != NULL && v->nick != NULL; v++) {
		if (g_strcmp0 (v->nick, nick) == 0) {
			store_enum_nick (k, v->nick);
			return;
		}
	}

	g_critical ("nemo-config: '%s' is not a valid value for '%s.%s'",
	            nick != NULL ? nick : "(null)", k->group, k->key);
}

void
nemo_config_set_enum (NemoConfigGroup *group, const char *key, gint value)
{
	const NemoConfigKey       *k = require_key (group, key, NEMO_CONFIG_ENUM);
	const NemoConfigEnumValue *v;

	if (k == NULL)
		return;

	for (v = k->enum_values; v != NULL && v->nick != NULL; v++) {
		if (v->value == value) {
			store_enum_nick (k, v->nick);
			return;
		}
	}

	g_critical ("nemo-config: %d is not a valid value for '%s.%s'",
	            value, k->group, k->key);
}

void
nemo_config_reset (NemoConfigGroup *group, const char *key)
{
	const NemoConfigKey *k;
	char                *path;

	if (group == NULL) {
		g_critical ("nemo-config: '%s' reset before the config store was opened", key);
		return;
	}

	k = find_key (group->name, key);
	if (k == NULL) {
		g_critical ("nemo-config: no such key '%s' in group '%s'",
		            key, group->name);
		return;
	}

	path = key_path (k);
	g_mutex_lock (&config_lock);
	shcl_remove (config_doc, path, strlen (path));
	schedule_save ();
	g_mutex_unlock (&config_lock);
	g_free (path);

	emit_changed (k->group, k->key);
}

char **
nemo_config_list_keys (NemoConfigGroup *group)
{
	const NemoConfigKey *k;
	GPtrArray           *out = g_ptr_array_new ();
	const char          *name = group ? group->name : "";

	for (k = nemo_config_keys; k->key != NULL; k++) {
		if (g_strcmp0 (k->group, name) == 0)
			g_ptr_array_add (out, g_strdup (k->key));
	}
	g_ptr_array_add (out, NULL);
	return (char **) g_ptr_array_free (out, FALSE);
}

/* ---- Property binding ---- */

typedef struct {
	NemoConfigGroup      *group;
	char                 *key;
	GObject              *object;
	char                 *property;
	NemoConfigBindFlags   flags;
	NemoConfigGetMapping  get_mapping;
	NemoConfigSetMapping  set_mapping;
	gpointer              user_data;
	GDestroyNotify        destroy;
	gulong                config_handler;
	gulong                notify_handler;
	gboolean              syncing;
} ConfigBinding;

static void
config_value_clear (NemoConfigValue *cv)
{
	g_clear_pointer (&cv->s, g_free);
	g_clear_pointer (&cv->sv, g_strfreev);
}

static void
read_config_value (ConfigBinding *b, NemoConfigValue *cv)
{
	const NemoConfigKey *k = find_key (b->group->name, b->key);

	memset (cv, 0, sizeof (*cv));
	if (k == NULL)
		return;

	cv->type = k->type;
	switch (k->type) {
	case NEMO_CONFIG_BOOL:
		cv->b = nemo_config_get_boolean (b->group, b->key);
		break;
	case NEMO_CONFIG_INT:
		cv->i = nemo_config_get_int (b->group, b->key);
		break;
	case NEMO_CONFIG_FLOAT:
		cv->d = nemo_config_get_double (b->group, b->key);
		break;
	case NEMO_CONFIG_STRING:
		cv->s = nemo_config_get_string (b->group, b->key);
		break;
	case NEMO_CONFIG_STRING_LIST:
		cv->sv = nemo_config_get_strv (b->group, b->key);
		break;
	case NEMO_CONFIG_ENUM:
		cv->i = nemo_config_get_enum (b->group, b->key);
		cv->s = nemo_config_get_string (b->group, b->key);
		break;
	}
}

static void
write_config_value (ConfigBinding *b, const NemoConfigValue *cv)
{
	switch (cv->type) {
	case NEMO_CONFIG_BOOL:
		nemo_config_set_boolean (b->group, b->key, cv->b);
		break;
	case NEMO_CONFIG_INT:
		nemo_config_set_int (b->group, b->key, (gint) cv->i);
		break;
	case NEMO_CONFIG_FLOAT:
		nemo_config_set_double (b->group, b->key, cv->d);
		break;
	case NEMO_CONFIG_STRING:
		nemo_config_set_string (b->group, b->key, cv->s);
		break;
	case NEMO_CONFIG_STRING_LIST:
		nemo_config_set_strv (b->group, b->key, (const char *const *) cv->sv);
		break;
	case NEMO_CONFIG_ENUM:
		/* A set-mapping hands us the nick and leaves the number at 0, so
		 * taking the number here wrote the zero-valued nick whatever the
		 * user picked. Only the unmapped path fills the number. */
		if (cv->s != NULL)
			set_enum_by_nick (b->group, b->key, cv->s);
		else
			nemo_config_set_enum (b->group, b->key, (gint) cv->i);
		break;
	}
}

/* config -> property */
static void
binding_sync_to_object (ConfigBinding *b)
{
	NemoConfigValue cv;
	GParamSpec     *pspec;
	GValue          value = G_VALUE_INIT;

	if (b->syncing || b->object == NULL)
		return;

	pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (b->object),
	                                      b->property);
	if (pspec == NULL)
		return;

	read_config_value (b, &cv);
	g_value_init (&value, pspec->value_type);

	if (b->get_mapping != NULL) {
		if (!b->get_mapping (&value, &cv, b->user_data)) {
			g_value_unset (&value);
			config_value_clear (&cv);
			return;
		}
	} else {
		switch (cv.type) {
		case NEMO_CONFIG_BOOL: {
			gboolean v = cv.b;
			if (b->flags & NEMO_CONFIG_BIND_INVERT_BOOLEAN)
				v = !v;
			g_value_set_boolean (&value, v);
			break;
		}
		case NEMO_CONFIG_INT:
		case NEMO_CONFIG_ENUM:
			if (pspec->value_type == G_TYPE_UINT)
				g_value_set_uint (&value, (guint) cv.i);
			else
				g_value_set_int (&value, (gint) cv.i);
			break;
		case NEMO_CONFIG_FLOAT:
			g_value_set_double (&value, cv.d);
			break;
		case NEMO_CONFIG_STRING:
			g_value_set_string (&value, cv.s);
			break;
		case NEMO_CONFIG_STRING_LIST:
			g_value_unset (&value);
			config_value_clear (&cv);
			return;
		}
	}

	b->syncing = TRUE;
	g_object_set_property (b->object, b->property, &value);
	b->syncing = FALSE;

	g_value_unset (&value);
	config_value_clear (&cv);
}

/* property -> config */
static void
binding_sync_to_config (ConfigBinding *b)
{
	const NemoConfigKey *k;
	NemoConfigValue      cv;
	GParamSpec          *pspec;
	GValue               value = G_VALUE_INIT;

	if (b->syncing || b->object == NULL)
		return;

	k = find_key (b->group->name, b->key);
	if (k == NULL)
		return;

	pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (b->object),
	                                      b->property);
	if (pspec == NULL)
		return;

	g_value_init (&value, pspec->value_type);
	g_object_get_property (b->object, b->property, &value);

	memset (&cv, 0, sizeof (cv));
	cv.type = k->type;

	if (b->set_mapping != NULL) {
		if (!b->set_mapping (&value, &cv, b->user_data)) {
			g_value_unset (&value);
			config_value_clear (&cv);
			return;
		}
	} else {
		switch (k->type) {
		case NEMO_CONFIG_BOOL: {
			gboolean v = g_value_get_boolean (&value);
			if (b->flags & NEMO_CONFIG_BIND_INVERT_BOOLEAN)
				v = !v;
			cv.b = v;
			break;
		}
		case NEMO_CONFIG_INT:
		case NEMO_CONFIG_ENUM:
			cv.i = (pspec->value_type == G_TYPE_UINT)
			     ? (gint64) g_value_get_uint (&value)
			     : (gint64) g_value_get_int (&value);
			break;
		case NEMO_CONFIG_FLOAT:
			cv.d = g_value_get_double (&value);
			break;
		case NEMO_CONFIG_STRING:
			cv.s = g_value_dup_string (&value);
			break;
		case NEMO_CONFIG_STRING_LIST:
			g_value_unset (&value);
			return;
		}
	}

	b->syncing = TRUE;
	write_config_value (b, &cv);
	b->syncing = FALSE;

	g_value_unset (&value);
	config_value_clear (&cv);
}

static void
on_config_changed (NemoConfigGroup *group, const char *key, gpointer data)
{
	binding_sync_to_object ((ConfigBinding *) data);
}

static void
on_property_notify (GObject *object, GParamSpec *pspec, gpointer data)
{
	binding_sync_to_config ((ConfigBinding *) data);
}

static void
binding_free (ConfigBinding *b)
{
	if (b->config_handler != 0)
		g_signal_handler_disconnect (b->group, b->config_handler);
	if (b->destroy != NULL)
		b->destroy (b->user_data);
	g_free (b->key);
	g_free (b->property);
	g_free (b);
}

static void
binding_object_gone (gpointer data, GObject *where_the_object_was)
{
	ConfigBinding *b = data;

	b->object = NULL;
	binding_free (b);
}

void
nemo_config_bind_with_mapping (NemoConfigGroup      *group,
                               const char           *key,
                               gpointer              object,
                               const char           *property,
                               NemoConfigBindFlags   flags,
                               NemoConfigGetMapping  get_mapping,
                               NemoConfigSetMapping  set_mapping,
                               gpointer              user_data,
                               GDestroyNotify        destroy)
{
	ConfigBinding *b;
	gboolean       do_get, do_set;

	g_return_if_fail (NEMO_IS_CONFIG_GROUP (group));
	g_return_if_fail (G_IS_OBJECT (object));

	/* DEFAULT means both directions, as it did before. */
	do_get = (flags & NEMO_CONFIG_BIND_GET) || !(flags & (NEMO_CONFIG_BIND_GET | NEMO_CONFIG_BIND_SET));
	do_set = (flags & NEMO_CONFIG_BIND_SET) || !(flags & (NEMO_CONFIG_BIND_GET | NEMO_CONFIG_BIND_SET));

	b = g_new0 (ConfigBinding, 1);
	b->group       = group;
	b->key         = g_strdup (key);
	b->object      = object;
	b->property    = g_strdup (property);
	b->flags       = flags;
	b->get_mapping = get_mapping;
	b->set_mapping = set_mapping;
	b->user_data   = user_data;
	b->destroy     = destroy;

	if (do_get) {
		char *detailed = g_strdup_printf ("changed::%s", key);
		b->config_handler = g_signal_connect (group, detailed,
		                                      G_CALLBACK (on_config_changed), b);
		g_free (detailed);
	}
	if (do_set) {
		char *detailed = g_strdup_printf ("notify::%s", property);
		b->notify_handler = g_signal_connect (object, detailed,
		                                      G_CALLBACK (on_property_notify), b);
		g_free (detailed);
	}

	g_object_weak_ref (object, binding_object_gone, b);

	if (do_get)
		binding_sync_to_object (b);
	else if (do_set)
		binding_sync_to_config (b);
}

void
nemo_config_bind (NemoConfigGroup     *group,
                  const char          *key,
                  gpointer             object,
                  const char          *property,
                  NemoConfigBindFlags  flags)
{
	nemo_config_bind_with_mapping (group, key, object, property, flags,
	                               NULL, NULL, NULL, NULL);
}
