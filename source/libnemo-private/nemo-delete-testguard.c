/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-delete-testguard.c - stop on every trash and delete, for testing.

   Copyright © 2026 t00mietum (CryptogID: ปʬϝღถɔ4რఠΔթะ9ƾǝu).

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

#include "nemo-delete-testguard.h"

#include <glib/gi18n.h>

#include "nemo-delete-guard.h"
#include "nemo-global-preferences.h"

#if HAVE_BACKTRACE
#include <execinfo.h>
#define TESTGUARD_MAX_FRAMES 24
#endif

/* Long enough to read, short enough to fit a dialog. */
#define MAX_PATHS_SHOWN 40

/* Long enough for a person to read the thing, short enough that a run with
   nobody watching ends rather than wedges. */
#define ANSWER_TIMEOUT_USEC (300 * G_USEC_PER_SEC)

/* Set while a job works through what it already asked about, so the two
   removal functions below it stay quiet. A job and its removals run on one
   thread, which is what makes this enough. */
static GPrivate covered = G_PRIVATE_INIT (NULL);

/* The define is the loudest of the three ways in: on, nothing quiets it. */
static const gboolean forced_on = (NEMO_TESTGUARD_ALL_DELETES != 0);

/* 1 for on, 0 for off, -1 for "said nothing". */
static int
parse_choice (const char *text)
{
	static const char *const on[]  = { "1", "true", "yes", "on", NULL };
	static const char *const off[] = { "0", "false", "no", "off", NULL };
	int i;

	for (i = 0; on[i] != NULL; i++) {
		if (g_ascii_strcasecmp (text, on[i]) == 0) {
			return 1;
		}
	}

	for (i = 0; off[i] != NULL; i++) {
		if (g_ascii_strcasecmp (text, off[i]) == 0) {
			return 0;
		}
	}

	return -1;
}

/* Read once. The environment does not change under a running process, and a
   delete is no place to be parsing strings. */
static int
env_choice (void)
{
	static gsize read_once = 0;
	static int choice = -1;

	if (g_once_init_enter (&read_once)) {
		const char *text = g_getenv (NEMO_TESTGUARD_ENV_VAR);

		if (text != NULL && *text != '\0') {
			choice = parse_choice (text);
			if (choice < 0) {
				g_warning ("%s is set to \"%s\", which means neither on nor off - ignoring it",
					   NEMO_TESTGUARD_ENV_VAR, text);
			}
		}

		g_once_init_leave (&read_once, 1);
	}

	return choice;
}

gboolean
nemo_delete_testguard_armed (void)
{
	int choice;

	if (forced_on) {
		return TRUE;
	}

	choice = env_choice ();
	if (choice >= 0) {
		return choice == 1;
	}

	/* A delete can be reached from local_command_line, before the settings
	   file has been read. Nothing is stored yet, so the answer is no. */
	if (!nemo_config_is_ready ()) {
		return FALSE;
	}

	return nemo_config_get_boolean (nemo_config_get_group (NEMO_DEBUG_GROUP),
					NEMO_PREFERENCES_TESTGUARD_ALL_DELETES);
}

/* Heap held, since the waiting side can give up on the timeout below while the
   dialog is still up. Whoever lets go last frees it. */
typedef struct {
	const char *op;
	char *detail;
	guint count;
	gboolean go_ahead;

	GMutex lock;
	GCond done;
	gboolean answered;
	guint refs;
} AskData;

static void
ask_data_unref (AskData *data)
{
	gboolean last;

	g_mutex_lock (&data->lock);
	data->refs--;
	last = (data->refs == 0);
	g_mutex_unlock (&data->lock);

	if (!last) {
		return;
	}

	g_mutex_clear (&data->lock);
	g_cond_clear (&data->done);
	g_free (data->detail);
	g_free (data);
}

void
nemo_delete_testguard_begin (void)
{
	gsize depth = GPOINTER_TO_SIZE (g_private_get (&covered));

	g_private_set (&covered, GSIZE_TO_POINTER (depth + 1));
}

void
nemo_delete_testguard_end (void)
{
	gsize depth = GPOINTER_TO_SIZE (g_private_get (&covered));

	if (depth > 0) {
		g_private_set (&covered, GSIZE_TO_POINTER (depth - 1));
	}
}

static gboolean
already_asked (void)
{
	return GPOINTER_TO_SIZE (g_private_get (&covered)) > 0;
}

static char *
describe_stack (void)
{
#if HAVE_BACKTRACE
	void *frames[TESTGUARD_MAX_FRAMES];
	char **names;
	GString *out;
	int n_frames;
	int i;

	n_frames = backtrace (frames, TESTGUARD_MAX_FRAMES);
	if (n_frames <= 0) {
		return g_strdup ("  (no frames)");
	}

	names = backtrace_symbols (frames, n_frames);
	if (names == NULL) {
		return g_strdup ("  (no symbols)");
	}

	out = g_string_new (NULL);
	/* Frame 0 is this function, 1 is the text builder below it. */
	for (i = 2; i < n_frames; i++) {
		g_string_append_printf (out, "  %s\n", names[i]);
	}

	free (names);

	return g_string_free (out, FALSE);
#else
	return g_strdup ("  (not available in this build)");
#endif
}

static char *
describe_files (GList *files)
{
	GString *out;
	GList *l;
	guint shown = 0;

	out = g_string_new (NULL);

	for (l = files; l != NULL; l = l->next) {
		char *name;

		if (shown == MAX_PATHS_SHOWN) {
			g_string_append_printf (out, "  ... and %u more\n",
						g_list_length (l));
			break;
		}

		name = g_file_get_parse_name (l->data);
		g_string_append_printf (out, "  %s\n", name != NULL ? name : "?");
		g_free (name);
		shown++;
	}

	return g_string_free (out, FALSE);
}

/* Everything the dialog shows below the headline: where it came from in the C
   source, what it would take, and how it got there. */
static char *
build_detail (GList      *files,
	      const char *func,
	      const char *source_file,
	      int         line)
{
	char *paths;
	char *stack;
	char *detail;

	paths = describe_files (files);
	stack = describe_stack ();

	detail = g_strdup_printf ("Asked from %s at %s:%d\n\nItems:\n%s\nCall stack:\n%s",
				  func, source_file, line, paths, stack);

	g_free (paths);
	g_free (stack);

	return detail;
}

static gboolean
show_dialog (gpointer _data)
{
	AskData *data = _data;
	GtkWidget *dialog;
	char *primary;
	int response;

	primary = g_strdup_printf ("%s: %u item%s",
				   data->op, data->count,
				   data->count == 1 ? "" : "s");

	dialog = gtk_message_dialog_new (NULL,
					 0,
					 GTK_MESSAGE_WARNING,
					 GTK_BUTTONS_NONE,
					 "%s", primary);

	gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog),
						  "%s", data->detail);

	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
	gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_OK);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);

	gtk_window_set_title (GTK_WINDOW (dialog), "Delete test guard");
	gtk_window_set_urgency_hint (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_keep_above (GTK_WINDOW (dialog), TRUE);

	response = gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_free (primary);

	g_mutex_lock (&data->lock);
	data->go_ahead = (response == GTK_RESPONSE_OK);
	data->answered = TRUE;
	g_cond_signal (&data->done);
	g_mutex_unlock (&data->lock);

	ask_data_unref (data);

	return G_SOURCE_REMOVE;
}

/* A job runs off the main thread, so the dialog goes over there and this side
   waits for the answer. */
static gboolean
ask (const char *op, guint count, char *detail)
{
	AskData *data;
	gboolean go_ahead;

	/* A test binary has no display and often no loop to hand a dialog to, and
	   waiting on an answer that cannot come would just hang it. */
	if (gdk_display_get_default () == NULL) {
		nemo_delete_guard_log ("test guard: %s went ahead, nothing to ask on", op);
		g_free (detail);
		return TRUE;
	}

	data = g_new0 (AskData, 1);
	data->op = op;
	data->count = count;
	data->detail = detail;
	data->refs = 1;
	g_mutex_init (&data->lock);
	g_cond_init (&data->done);

	/* Owning the default context means this is the thread running the GTK
	   loop, so the dialog can just go up here. */
	if (g_main_context_is_owner (g_main_context_default ())) {
		data->refs++;
		show_dialog (data);

		g_mutex_lock (&data->lock);
		go_ahead = data->go_ahead;
		g_mutex_unlock (&data->lock);
	} else {
		gint64 deadline = g_get_monotonic_time () + ANSWER_TIMEOUT_USEC;

		data->refs++;
		g_main_context_invoke (NULL, show_dialog, data);

		g_mutex_lock (&data->lock);
		while (!data->answered) {
			if (!g_cond_wait_until (&data->done, &data->lock, deadline)) {
				/* Nobody is iterating the loop the dialog went to, or
				   nobody is there to answer. Cancel is the default, so
				   that is what running out of time means. */
				nemo_delete_guard_log ("test guard: %s called off, no answer in %d seconds",
						       op, (int) (ANSWER_TIMEOUT_USEC / G_USEC_PER_SEC));
				break;
			}
		}
		go_ahead = data->answered && data->go_ahead;
		g_mutex_unlock (&data->lock);
	}

	ask_data_unref (data);

	return go_ahead;
}

gboolean
nemo_delete_testguard_ask_at (const char *op,
			      GList      *files,
			      const char *func,
			      const char *source_file,
			      int         line)
{
	gboolean go_ahead;

	if (files == NULL || !nemo_delete_testguard_armed ()) {
		return TRUE;
	}

	go_ahead = ask (op, g_list_length (files),
			build_detail (files, func, source_file, line));

	nemo_delete_guard_log ("test guard: %s from %s at %s:%d, %s",
			       op, func, source_file, line,
			       go_ahead ? "went ahead" : "called off");

	return go_ahead;
}

gboolean
nemo_delete_testguard_ask_one_at (const char *op,
				  GFile      *file,
				  const char *func,
				  const char *source_file,
				  int         line)
{
	GList one = { file, NULL, NULL };

	if (already_asked ()) {
		return TRUE;
	}

	return nemo_delete_testguard_ask_at (op, &one, func, source_file, line);
}
