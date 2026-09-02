#!/usr/bin/env python3

##	- Purpose: Every piece of text the interface shows reads as a sentence, not
##	  as a headline: only the first word capitalised, and anything that is a
##	  name left alone. This is easy to undo by accident - a label copied from
##	  upstream nemo arrives in Title Case - so it is checked rather than trusted.
##	- Scope: text already marked translatable, and nothing else. That is the
##	  body of _() and N_() in C, the text of a translatable element in a
##	  .ui/.glade, and the Name/Comment of a bundled action.
##	- A word is left alone when it starts a sentence, is a name (NAMES below),
##	  is an acronym, or carries a capital of its own past the first letter.
##	  Whole strings that the rule reads wrong are listed in KEEP, each with why.
##	- Exit 1 on any finding, so it gates. Syntax: lint-ui-case.py <source-root>

##	Copyright © 2026 t00mietum
##	Licensed under the GNU General Public License, version 2 only.
##	SPDX-License-Identifier: GPL-2.0-only

import io
import os
import re
import sys

# Names that keep their capital wherever they stand.
NAMES = set("""
Windows Linux BSD FreeBSD OpenBSD NetBSD macOS Mac OS Unix POSIX Unicode ASCII
GNOME Cinnamon Expo GTK GLib GIO GVfs Adwaita Segoe GNU
Nemo Nemo's Anywhere Nautilus Explorer
Trash Home Desktop Documents Downloads Music Pictures Videos Public
Network Recent Favorites Favourites
MIME URI URL UTF SHA MD ID DPI CPU RAM USB DVD CD ISO PDF SVG PNG JPEG GIF EXIF
ZIP RAR TAR XZ GZ BZ LZMA SELinux SMB NFS FTP SFTP SSH WebDAV DAV AFP AFC
Ctrl Alt Shift Esc Enter
F1 F2 F3 F4 F5 F6 F7 F8 F9 F10 F11 F12
January February March April May June July August September October November
December Monday Tuesday Wednesday Thursday Friday Saturday Sunday
I A
""".split())

# Strings the rule reads wrong, and why each one is right as it stands.
KEEP = set([
	# Legal text, quoted verbatim.
	"Nemo is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.",
	"Nemo is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.",
	"You should have received a copy of the GNU General Public License along with Nemo; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA",
	# The name a new file is given on disk, not interface text.
	"Untitled Folder",
	"Untitled Document",
	# Property nicknames on a GObject; never shown to anyone.
	"Cursor Position",
	"Selection Bound",
	# Windows Search is the indexer's name.
	"Use the Windows Search index for folders it covers",
	# Disc formats are named things.
	"These files are on a Photo CD.",
	"These files are on a Picture CD.",
	"These files are on a Super Video CD.",
	"These files are on a Video CD.",
	"These files are on a Video DVD.",
	"These files are on an Audio CD.",
	"These files are on an Audio DVD.",
	# Sentences that name a menu item, a tab or a sidebar, which keeps its own
	# capital the same way a proper noun does.
	"Move or copy files previously selected by a Cut or Copy command",
	"Move or copy files previously selected by a Cut or Copy command into the selected folder",
	"Move or copy files previously selected by a Cut or Copy command into this folder",
	"Prepare the selected files to be copied with a Paste command",
	"Prepare the selected files to be moved with a Paste command",
	"Prepare this folder to be copied with a Paste command",
	"Prepare this folder to be moved with a Paste command",
	'\\"%s\\" will be copied if you select the Paste command',
	'\\"%s\\" will be moved if you select the Paste command',
	"The %'d selected item will be moved if you select the Paste command",
	"The %'d selected items%s will be moved if you select the Paste command",
	"The %'d selected item will be copied if you select the Paste command",
	"The %'d selected items%s will be copied if you select the Paste command",
	"No templates found. Click New or drag a file here to create one.",
	"Scripts: All executable files in this folder will appear in the Scripts menu.",
	"&lt;i&gt;Windows has a hidden attribute of its own, so the two kinds are separate here. The View menu switches both at once.&lt;/i&gt;",
	"Visible action and extension entries can be configured in the Plugins tab",
	"Select Places as the default sidebar",
	"Select Tree as the default sidebar",
	"Customize the layout and appearance of your actions in Nemo's menus",
	"Go to Computer",
	"Include a Delete command that bypasses Trash",
	"Bypass the Trash when the Delete key is pressed",
	"The file \\\"%s\\\" has no known programs associated with it.  If you trust the source of this file, and have sufficient permissions, you can mark it executable and launch it.  Or, you can use the Open with dialog to pick a program to associate it with.",
	"The file \\\"%s\\\" has no known programs associated with it.  Use the Open with dialog to pick a program to open it with.",
	"You have chosen to hide the main menu.  You can get it back temporarily by:\\n\\n- Tapping the <Alt> key\\n- Right-clicking an empty region of the main toolbar\\n- Right-clicking an empty region of the status bar.\\n\\nYou can restore it permanently by selecting this option again from the View menu.",
	# Windows names both of these itself, capitals and all.
	"Windows allows symlinks only with Developer Mode turned on, or when running as administrator.",
	"Only a file on this computer can go to the Recycle Bin.",
	"The Recycle Bin refused it (error %d).",
	# A window title: the file's name, then what the window is.
	"%s - File browser",
])

# A mnemonic underscore sits inside the word it marks, so it is part of the
# token and the letter after it does not start a word of its own.
WORD_RE = re.compile(r"\\n|<[^>]*>|[A-Za-z][A-Za-z'’_]*|[.?!:;\"'‘“]")

# A long message is often written as adjacent literals; they are one string to
# anyone reading it, so they are checked as one.
C_RE = re.compile(r'\b(?:N_|_)\(\s*("(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")*)')
UI_RE = re.compile(r'translatable="yes"[^>]*>([^<]*)</')
ACTION_RE = re.compile(r'^(?:Name|Comment)=(.*)$', re.M)
LITERAL_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')
# A plural call carries two labels of its own and is easy to overlook - it is
# what left "Copy Paths" and "Make Links" in Title Case after the first sweep.
NGETTEXT_RE = re.compile(r'\bngettext\s*\(')


def ngettext_strings(text):
	"""Every string literal inside an ngettext call, with its line."""
	for m in NGETTEXT_RE.finditer(text):
		depth = 1
		i = m.end()
		parts = []
		while i < len(text) and depth > 0:
			ch = text[i]
			if ch == '"':
				line = text.count("\n", 0, i) + 1
				joined = []
				# Adjacent literals are one string to whoever reads it.
				while True:
					lit = LITERAL_RE.match(text, i)
					if lit is None:
						break
					joined.append(lit.group(1))
					i = lit.end()
					while i < len(text) and text[i] in " \t\r\n":
						i += 1
				if not joined:
					break
				parts.append((line, "".join(joined)))
				continue
			if ch == "(":
				depth += 1
			elif ch == ")":
				depth -= 1
			i += 1

		for line, body in parts:
			yield line, body


def offenders(text):
	"""The words that should have been lower case, in order."""
	bad = []
	sentence_start = True

	for m in WORD_RE.finditer(text):
		word = m.group(0)

		if not word[0].isalpha():
			# Punctuation, a line break or markup: what follows starts a sentence.
			sentence_start = True
			continue

		first, sentence_start = sentence_start, False
		bare = word.replace("_", "")

		if (first or not bare[0].isupper() or bare in NAMES or bare.isupper() or
		    any(c.isupper() for c in bare[1:])):
			continue

		bad.append(word)

	return bad


def strings_in(path):
	text = io.open(path, encoding="utf-8", errors="replace").read()

	if path.endswith((".c", ".h")):
		pattern = C_RE
	elif path.endswith((".ui", ".glade", ".xml")):
		pattern = UI_RE
	else:
		pattern = ACTION_RE

	for m in pattern.finditer(text):
		body = m.group(1)
		if pattern is C_RE:
			body = "".join(LITERAL_RE.findall(body))
		line = text.count("\n", 0, m.start()) + 1
		yield line, body

	if pattern is C_RE:
		for line, body in ngettext_strings(text):
			yield line, body


def main():
	root = sys.argv[1] if len(sys.argv) > 1 else "source"
	findings = 0

	for base, _dirs, files in os.walk(root):
		if os.sep + "vendor" in base:
			continue
		for name in sorted(files):
			if not name.endswith((".c", ".h", ".ui", ".glade", ".xml",
					      ".nemo_action", ".nemo_action.in")):
				continue
			path = os.path.join(base, name)
			for line, body in strings_in(path):
				if not body.strip() or body.replace("_", "") in KEEP:
					continue
				bad = offenders(body)
				if not bad:
					continue
				findings += 1
				sys.stderr.write("%s:%d: not sentence case (%s): %s\n"
						 % (path.replace(os.sep, "/"), line,
						    ", ".join(bad), body))

	if findings:
		sys.stderr.write("[ FAILED: UI case: %d string(s) not in sentence case ]\n"
				 % findings)
		return 1

	print("[ OK: UI case: every translatable string is sentence case ]")

	return 0


sys.exit(main())
