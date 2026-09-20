#!/usr/bin/env python3

# Connect and disconnect a settings handler on the same preference group.
#
# There are several groups - nemo_preferences, nemo_windows_preferences,
# nemo_desktop_preferences and the rest - and they are separate objects. A
# disconnect aimed at the wrong one removes nothing and reports nothing, so the
# handler outlives whatever it was connected for and is called with a pointer to
# freed memory the next time the setting changes. Live settings reload makes
# that reachable in normal use.
#
# Two spellings are checked. Disconnect by function pairs a callback name with
# the groups it was connected to. Disconnect by id pairs the variable holding
# the id with the group the connect that filled it used.
#
# Syntax: lint-pref-handlers.py [root]   (default: the repo's source/)

# Copyright (c) 2026 Bubbles
# Licensed under The MIT License (MIT). Full text at:
#     https://mit-license.org/
# SPDX-License-Identifier: MIT

import re
import sys
from pathlib import Path

# The group names come from the header that declares them, so a new group is
# covered the day it is added. Matching a pattern instead missed nemo_window_state,
# whose name does not end in "preferences".
GROUPS_HEADER = "libnemo-private/nemo-global-preferences.h"
DECLARED = re.compile(r"^extern\s+NemoConfigGroup\s*\*\s*(\w+)\s*;", re.MULTILINE)


def groups_pattern(root):
    header = root / GROUPS_HEADER
    names = DECLARED.findall(header.read_text(errors="replace")) if header.is_file() else []
    if not names:
        raise SystemExit(f"[ FAILED: no config groups declared in {GROUPS_HEADER} ]")
    return "(?:" + "|".join(sorted(names, key=len, reverse=True)) + ")"



def patterns(group):
    return {
        "connect_by_func": re.compile(
            r"g_signal_connect\w*\s*\(\s*(" + group + r")\s*,.*?G_CALLBACK\s*\(\s*(\w+)\s*\)",
            re.DOTALL),
        "disconnect_by_func": re.compile(
            r"g_signal_handlers_disconnect_by_func\s*\(\s*(" + group + r")\s*,\s*(\w+)\s*,",
            re.DOTALL),
        "connect_to_id": re.compile(
            r"(\w+)\s*=\s*g_signal_connect\w*\s*\(\s*(" + group + r")\s*,",
            re.DOTALL),
        "disconnect_by_id": re.compile(
            r"g_signal_handler_disconnect\s*\(\s*(" + group + r")\s*,\s*(\w+)\s*\)",
            re.DOTALL),
    }


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


def check_file(path, pats):
    text = path.read_text(errors="replace")
    problems = []

    by_func = {}
    for match in pats["connect_by_func"].finditer(text):
        by_func.setdefault(match.group(2), set()).add(match.group(1))

    by_id = {}
    for match in pats["connect_to_id"].finditer(text):
        by_id.setdefault(match.group(1), set()).add(match.group(2))

    for match in pats["disconnect_by_func"].finditer(text):
        group, callback = match.group(1), match.group(2)
        connected = by_func.get(callback)
        # No connect in this file means the pair is somewhere else and there is
        # nothing here to compare against.
        if connected and group not in connected:
            problems.append(
                (line_of(text, match.start()),
                 f"{callback} is disconnected from {group} but connected to "
                 + ", ".join(sorted(connected)))
            )

    for match in pats["disconnect_by_id"].finditer(text):
        group, variable = match.group(1), match.group(2)
        connected = by_id.get(variable)
        if connected and group not in connected:
            problems.append(
                (line_of(text, match.start()),
                 f"handler {variable} is disconnected from {group} but connected to "
                 + ", ".join(sorted(connected)))
            )

    return problems


def main():
    root = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / "source"
    if not root.is_dir():
        print(f"[ FAILED: {root} is not a directory ]")
        return 1

    pats = patterns(groups_pattern(root))
    failures = 0
    for path in sorted(root.rglob("*.c")):
        for line, message in check_file(path, pats):
            rel = path.relative_to(root.parent)
            print(f"[ FAIL: {rel}:{line}: {message} ]")
            failures += 1

    if failures:
        print(f"[ FAILED: settings handlers: {failures} mismatched group(s) ]")
        return 1
    print("[ OK: settings handlers: every disconnect matches its connect ]")
    return 0


if __name__ == "__main__":
    sys.exit(main())
