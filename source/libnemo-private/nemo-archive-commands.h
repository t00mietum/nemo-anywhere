/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* nemo-archive-commands.h - the editable command lines, and their defaults.

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

/* Kept apart from nemo-archive.h and nemo-extract.h, and free of anything but
 * the strings, so the settings table can include it too - the default a key
 * falls back to and the default the writer runs are then the same text rather
 * than two copies that can drift.
 *
 * The placeholders are documented for users in project/design.md; see
 * nemo-command-template.h for what the braces mean and why they are braces.
 * Every switch the Compress dialog can turn on has a token of its own, so an
 * edited line keeps the dialog working - and dropping one is noticed and
 * warned about rather than quietly ignored.
 */

#ifndef NEMO_ARCHIVE_COMMANDS_H
#define NEMO_ARCHIVE_COMMANDS_H

#define NEMO_ARCHIVE_COMMANDS_GROUP "archive"

#define NEMO_ARCHIVE_COMMAND_KEY_7Z  "create-with-7z"
#define NEMO_ARCHIVE_COMMAND_KEY_RAR "create-with-rar"
#define NEMO_EXTRACT_COMMAND_KEY_7Z  "extract-with-7z"
#define NEMO_EXTRACT_COMMAND_KEY_RAR "extract-with-rar"

/* Worth knowing before editing any of these four:
   -y answers the prompts a program with no console would otherwise wait on
   forever; -bsp1 is what puts 7-Zip's percentage on stdout for the progress
   bar; "--" stops switch parsing, so a file whose name starts with a dash is
   read as a file; and "x" rather than "e" on the unpack lines is what keeps
   the paths stored in the archive. */
#define NEMO_ARCHIVE_COMMAND_7Z_DEFAULT \
	"{{PROGRAM}} a {{FORMAT}} {{LEVEL}} -y -bsp1 " \
	"{{PASSWORD}} {{SPLIT}} {{SOLID}} {{LINKS}} " \
	"-- {{TARGET_ARCHIVE}} {{SOURCE_ITEMS}}"

/* -r is what makes a named folder mean its contents. */
#define NEMO_ARCHIVE_COMMAND_RAR_DEFAULT \
	"{{PROGRAM}} a {{LEVEL}} -r -y " \
	"{{PASSWORD}} {{SPLIT}} {{SOLID}} {{DEDUPE}} {{RECOVERY}} {{LOCK}} {{LINKS}} " \
	"-- {{TARGET_ARCHIVE}} {{SOURCE_ITEMS}}"

#define NEMO_EXTRACT_COMMAND_7Z_DEFAULT \
	"{{PROGRAM}} x -y -bsp1 {{PASSWORD}} -o{{TARGET_FOLDER}} -- {{SOURCE_ARCHIVE}}"

/* rar reads its last argument as a destination only when it ends in a path
   separator, hence the second folder token rather than one with a "/" typed
   after it - the separator differs by platform, and a backslash written here
   would be eaten as an escape before it ever reached the program. */
#define NEMO_EXTRACT_COMMAND_RAR_DEFAULT \
	"{{PROGRAM}} x -y {{PASSWORD}} -- {{SOURCE_ARCHIVE}} {{TARGET_FOLDER_WITH_SEPARATOR}}"

#endif /* NEMO_ARCHIVE_COMMANDS_H */
