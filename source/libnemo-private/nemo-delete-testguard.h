/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-delete-testguard.h - stop on every trash and delete, for testing.

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

/* Scaffolding for finding a removal that nothing else accounts for. Armed, it
 * asks before every trash and delete, whatever started it, and names the C
 * call site plus a backtrace so an unexpected caller can be read off the
 * dialog. The normal confirmations are off while it is armed, so nothing is
 * asked about twice.
 *
 * It sits at two levels. A job asks once, up front, for everything it is about
 * to take. Below that, the two functions that actually remove a file ask for
 * anything that got there without a job having asked - that is the half which
 * catches a path nobody knew about. The two are kept from doubling up by a
 * thread-local mark, since a job and the removals it drives run on one thread.
 *
 * Three things can arm it, and the first one that speaks wins. The define
 * below at 1 arms every run of the build and nothing turns that off. At 0,
 * the NEMO_TESTGUARD_ALL_DELETES environment variable decides, either way.
 * With neither, the debug.testguard-all-deletes setting decides.
 *
 * It compiles in whichever way the define is set, so a build already in use
 * can be armed without replacing it. Quiet, all it costs a delete is one
 * boolean read.
 */

#ifndef NEMO_DELETE_TESTGUARD_H
#define NEMO_DELETE_TESTGUARD_H

#include <gio/gio.h>
#include <gtk/gtk.h>

/* 1 arms every run of this build, whatever the environment or the settings
   file say. Leave at 0 for a shipping build. */
#define NEMO_TESTGUARD_ALL_DELETES 1

/* 1/true/yes/on, or 0/false/no/off. Beats the setting in both directions, so
   a run can be quieted as well as armed. */
#define NEMO_TESTGUARD_ENV_VAR "NEMO_TESTGUARD_ALL_DELETES"

/* Whether this run asks. Cheap enough to call per file. */
gboolean nemo_delete_testguard_armed    (void);

/* TRUE to go ahead, FALSE if it was called off. Quiet, both say go ahead.
 * `op` is what to call the operation in the dialog, such as "Move to trash". */
gboolean nemo_delete_testguard_ask_at   (const char *op,
					 GList      *files,
					 const char *func,
					 const char *source_file,
					 int         line);

gboolean nemo_delete_testguard_ask_one_at (const char *op,
					   GFile      *file,
					   const char *func,
					   const char *source_file,
					   int         line);

/* Around the part of a job that removes what it already asked about. */
void     nemo_delete_testguard_begin    (void);
void     nemo_delete_testguard_end      (void);

#define nemo_delete_testguard_ask(op, files) \
	nemo_delete_testguard_ask_at ((op), (files), G_STRFUNC, __FILE__, __LINE__)

#define nemo_delete_testguard_ask_one(op, file) \
	nemo_delete_testguard_ask_one_at ((op), (file), G_STRFUNC, __FILE__, __LINE__)

#endif /* NEMO_DELETE_TESTGUARD_H */
