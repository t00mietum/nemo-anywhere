<!-- markdownlint-disable MD007 -- Unordered list indentation -->
<!-- markdownlint-disable MD010 -- No hard tabs -->
<!-- markdownlint-disable MD033 -- No inline html -->
<!-- markdownlint-disable MD041 -- First line in a file should be a top-level heading -->

<!-- TOC ignore:true -->
# nemo-anywhere code style

How the C in this repo is written, and why. Companion to [design.md](design.md) and [../contributing.md](../contributing.md).

<!-- TOC ignore:true -->
## Table of contents

<!-- TOC -->

- [The one rule that beats the rest](#the-one-rule-that-beats-the-rest)

- [Why there is no formatter config](#why-there-is-no-formatter-config)

- [Layout](#layout)

- [Naming](#naming)

- [Files](#files)

- [Platform-specific code](#platform-specific-code)

- [Comments](#comments)

- [Memory and errors](#memory-and-errors)

- [What the lint gate rejects](#what-the-lint-gate-rejects)

- [Tests](#tests)

- [Other languages in the tree](#other-languages-in-the-tree)

<!-- /TOC -->

## The one rule that beats the rest

This is a fork of a twenty-year-old GNOME codebase. New code looks like the code beside it, even where that is not what a fresh project would choose. A patch that reformats a function it did not need to touch is noise in every future diff against upstream, and it will be asked to go back.

Vendored code under `source/vendor/` and anything still carrying an upstream copyright header is left alone entirely. Do not restyle it, do not rename in it, do not "modernize" it.

## Why there is no formatter config

There is no `.clang-format`. Two reasons.

Most of the tree is inherited. A config aimed at the whole tree would rewrite hundreds of files nobody here wrote and make comparison against upstream Nemo useless. A config aimed only at the roughly eighty first-party files would need tuning until it stopped disagreeing with code that is already correct, and would then catch almost nothing, because the first-party code already matches the inherited style.

So the rules are written down here instead, and the review reads for them.

## Layout

- Tabs to indent, spaces to align. Every C file opens with the Emacs mode line that says so: `/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */`

- A tab is eight columns. Deep nesting gets uncomfortable fast, which is the point.

- Function definitions put the return type on its own line, the name at column zero, and the opening brace on its own line. Parameters that wrap are aligned with spaces under the first one.

- Inner blocks are K&R: brace on the same line, closing brace on its own.

- A space between a function name and its opening paren, in calls and in definitions both. `g_free (path)`, not `g_free(path)`. This one is near-universal in the tree and is the fastest way to spot a patch written somewhere else.

- No hard line-length limit. Long is fine when breaking it would hurt.

A sample, from `source/src/nemo-column-layout.c`:

```c
static int
move_proportionally (const NemoColumnLayoutItem *items,
                     int                        *widths,
                     const int                  *limits,
                     int                         n_items,
                     int                         direction,
                     int                         amount)
{
	while (amount > 0) {
		gint64 weight = 0;
		int moved = 0;
```

## Naming

- Public functions are `nemo_<file>_<verb>`, matching the file they live in.

- Static helpers are plain lowercase with underscores, no prefix.

- Types are `NemoThing`, macros and enum members are `NEMO_THING`.

- Names are searchable. `upper_bound` beats `ub`. A single letter is fine for a loop index and nowhere else.

- GObject boilerplate follows whatever GObject wants. It is generated shape, not a style choice.

## Files

- One pair per subject: `nemo-thing.c` and `nemo-thing.h`, lowercase with hyphens.

- Header guards are `#ifndef NEMO_THING_H`, not `#pragma once`.

- Every file opens with the mode line, then a one-line description, then the copyright and the GPL-2.0-only notice. Copy the block from a neighboring first-party file rather than retyping it.

- A file that keeps an upstream copyright keeps it. Add a line, do not replace one.

## Platform-specific code

Anything more than a few lines of Windows-only work gets its own file, named `*-win32.c`, with a header that declares a small portable-looking API. `nemo-clipboard-win32.c` and `nemo-trash-win32.c` are the pattern.

Short branches stay inline under `#ifdef G_OS_WIN32`. A branch that grows past a screen is a sign it wants its own file.

Reach for GLib before writing platform code at all. `GSubprocess` already terminates a process on both sides, so no `windows.h` and no branch.

## Comments

Terse, and about why. The code says what.

Comment the things that cost somebody a day: a workaround for a toolkit bug, an ordering that looks arbitrary but is not, a decision that a later reader would otherwise undo. Do not comment a line that reads fine on its own, and do not open a file with a summary of its own contents.

No banner dividers. ASCII only, except the copyright symbol.

Where a piece of reasoning is longer than a few lines, it goes at the top of the file it belongs to, or into `design.md` with the code pointing at it. `source/src/nemo-column-layout.h` is an example of the first.

## Memory and errors

- Explicit `g_free` and `g_object_unref` are the norm, because that is what the surrounding code does. `g_autofree` and `g_autoptr` appear in newer files and are fine there, but do not go converting old ones.

- Failures come back as `GError`. Check them or pass them up. Do not swallow one silently.

- GLib allocators abort rather than returning NULL, so a NULL check on `g_new` is dead code.

- Anything asked once per file on every pass of the async file engine must not allocate. `g_file_peek_path` over `g_file_get_path`.

## What the lint gate rejects

`cicd/utility/lint-c.bash` runs cppcheck and a check on user-facing strings, and both fail the build.

- `alloca`, and therefore `g_newa`. Use `g_new0` and `g_free` even for three ints.

- Any cppcheck finding at all, including informational ones. A known false positive gets an inline suppression with the reason written next to it.

- Title case in a user-facing string. Menu items, labels and dialog text are sentence case. A proper noun that trips the check goes in the checker's own list, with its reason.

The gate is scoped to the files a change touched, but it lints the whole of each of those files. Touching a large old file can therefore surface findings that were already there.

## Tests

New behavior arrives with a test. A fix arrives with a test that fails without the fix, and proving that is part of the work, not a formality; it has repeatedly caught a test that passed for the wrong reason.

Tests live in `source/test/`. A POSIX-only test is registered under `if not is_windows` in `source/test/meson.build`, or it breaks the Windows cross build.

A test that reads real user configuration is a test that fails on somebody else's machine. Point `HOME`, `APPDATA` and `XDG_CONFIG_HOME` at a throwaway directory first.

The suite needs a display. `Xvfb :95 -screen 0 1280x900x24 &` then `DISPLAY=:95 meson test`. Without one, a third of it fails in ways that read like real assertion failures.

## Other languages in the tree

- Bash files are named `*.bash` and must pass shellcheck. No formatter. Tabs.

- PowerShell is four spaces, `Set-StrictMode -Version Latest`, and no BOM on anything carrying a shebang.

- Python appears only as build and lint helpers. Four spaces.
