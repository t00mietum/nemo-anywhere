/* The key table in the code and the schema shipped for `shcl check` are two
 * hand-kept copies of the same list. Miss the second and a hand-edited config
 * validates against a schema that has never heard of the key. This walks both
 * and fails on any name, type, allowed set or default that does not line up.
 *
 * Takes the schema path as its one argument. */

#include <config.h>

#include <stdlib.h>
#include <string.h>
#include <glib.h>

#include <libnemo-private/nemo-config.h>
#include <libnemo-private/nemo-config-keys.h>

static int failures = 0;

static void
fail (const char *fmt, ...) G_GNUC_PRINTF (1, 2);

static void
fail (const char *fmt, ...)
{
	va_list args;
	char *message;

	va_start (args, fmt);
	message = g_strdup_vprintf (fmt, args);
	va_end (args);

	g_printerr ("FAIL %s\n", message);
	g_free (message);
	failures++;
}

typedef struct {
	char *type;
	char *allowed;
	char *def;
	gboolean seen;
} SchemaField;

static void
schema_field_free (gpointer data)
{
	SchemaField *field = data;

	g_free (field->type);
	g_free (field->allowed);
	g_free (field->def);
	g_free (field);
}

/* name -> SchemaField, in file order so the report reads like the file. */
static GHashTable *
read_schema (const char *path)
{
	GHashTable *fields;
	char *text = NULL;
	char **lines;
	SchemaField *current = NULL;
	int i;

	if (!g_file_get_contents (path, &text, NULL, NULL)) {
		fail ("could not read %s", path);
		return NULL;
	}

	fields = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, schema_field_free);
	lines = g_strsplit (text, "\n", -1);

	for (i = 0; lines[i] != NULL; i++) {
		const char *line = lines[i];
		gsize line_len = strlen (line);

		/* Git checks the schema out with CRLF on Windows, and a carriage return
		   left on the end of a field name matches nothing. */
		if (line_len > 0 && line[line_len - 1] == '\r') {
			lines[i][line_len - 1] = '\0';
		}

		if (g_str_has_prefix (line, "field: ")) {
			current = g_new0 (SchemaField, 1);
			g_hash_table_insert (fields, g_strdup (line + strlen ("field: ")), current);
		} else if (current != NULL && g_str_has_prefix (line, "\ttype: ")) {
			current->type = g_strdup (line + strlen ("\ttype: "));
		} else if (current != NULL && g_str_has_prefix (line, "\tallowed: ")) {
			current->allowed = g_strdup (line + strlen ("\tallowed: "));
		} else if (current != NULL && g_str_has_prefix (line, "\tdefault:")) {
			/* An empty string default is written as a bare "default:", and a
			   string one is quoted where a list or a number is not. */
			const char *value = line + strlen ("\tdefault:");
			gsize len;

			if (value[0] == ' ') {
				value++;
			}
			len = strlen (value);
			if (len >= 2 && value[0] == '"' && value[len - 1] == '"') {
				current->def = g_strndup (value + 1, len - 2);
			} else {
				current->def = g_strdup (value);
			}
		}
	}

	g_strfreev (lines);
	g_free (text);

	return fields;
}

static const char *
schema_type_name (NemoConfigType type)
{
	switch (type) {
	case NEMO_CONFIG_BOOL:        return "bool";
	case NEMO_CONFIG_INT:         return "int";
	case NEMO_CONFIG_FLOAT:       return "float";
	case NEMO_CONFIG_STRING:      return "string";
	case NEMO_CONFIG_STRING_LIST: return "string-array";
	case NEMO_CONFIG_ENUM:        return "string";
	}

	return "?";
}

/* "a, b, c" - the spelling the schema uses for both an enum's nicks and a
   string list's default. */
static char *
join_nicks (const NemoConfigEnumValue *values)
{
	GString *text = g_string_new (NULL);
	int i;

	for (i = 0; values != NULL && values[i].nick != NULL; i++) {
		if (i > 0) {
			g_string_append (text, ", ");
		}
		g_string_append (text, values[i].nick);
	}

	return g_string_free (text, FALSE);
}

/* Only the default comparison uses this, and that is POSIX-only below. */
#ifndef G_OS_WIN32
static char *
join_list (const char *const *list)
{
	GString *text = g_string_new (NULL);
	int i;

	for (i = 0; list != NULL && list[i] != NULL; i++) {
		if (i > 0) {
			g_string_append (text, ", ");
		}
		g_string_append (text, list[i]);
	}

	return g_string_free (text, FALSE);
}
#endif

int
main (int argc, char **argv)
{
	GHashTable *schema;
	GHashTableIter iter;
	gpointer name, value;
	int i;

	if (argc < 2) {
		g_printerr ("usage: %s <schema.shcl>\n", argv[0]);
		return EXIT_FAILURE;
	}

	schema = read_schema (argv[1]);
	if (schema == NULL) {
		return EXIT_FAILURE;
	}

	for (i = 0; nemo_config_keys[i].key != NULL; i++) {
		const NemoConfigKey *key = &nemo_config_keys[i];
		char *full;
		SchemaField *field;

		full = key->group[0] == '\0'
			? g_strdup (key->key)
			: g_strdup_printf ("%s.%s", key->group, key->key);

		field = g_hash_table_lookup (schema, full);
		if (field == NULL) {
			fail ("%s is in the key table but not in the schema", full);
			g_free (full);
			continue;
		}
		field->seen = TRUE;

		if (g_strcmp0 (field->type, schema_type_name (key->type)) != 0) {
			fail ("%s is %s in the key table, %s in the schema",
			      full, schema_type_name (key->type), field->type);
		}

		if (key->type == NEMO_CONFIG_ENUM) {
			char *nicks = join_nicks (key->enum_values);

			if (g_strcmp0 (field->allowed, nicks) != 0) {
				fail ("%s allows \"%s\" in the key table, \"%s\" in the schema",
				      full, nicks, field->allowed);
			}
			g_free (nicks);
		} else if (field->allowed != NULL) {
			fail ("%s has an allowed list in the schema but is not an enum", full);
		}

		/* The two list-view column defaults differ by platform and the schema
		   carries the POSIX spelling, so a Windows run only checks the rest. */
#ifndef G_OS_WIN32
		{
			char *expected = key->def_list != NULL
				? join_list (key->def_list)
				: g_strdup (key->def != NULL ? key->def : "");

			if (g_strcmp0 (field->def != NULL ? field->def : "", expected) != 0) {
				fail ("%s defaults to \"%s\" in the key table, \"%s\" in the schema",
				      full, expected, field->def != NULL ? field->def : "");
			}
			g_free (expected);
		}
#endif

		g_free (full);
	}

	g_hash_table_iter_init (&iter, schema);
	while (g_hash_table_iter_next (&iter, &name, &value)) {
		if (!((SchemaField *) value)->seen) {
			fail ("%s is in the schema but not in the key table", (const char *) name);
		}
	}

	g_hash_table_destroy (schema);

	if (failures == 0)
		g_print ("nemo-config-schema: key table and schema agree\n");

	return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
