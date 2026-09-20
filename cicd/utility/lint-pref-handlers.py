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

GROUP = r"nemo_[a-z_]*preferences"

CONNECT_BY_FUNC = re.compile(
    r"g_signal_connect\w*\s*\(\s*(" + GROUP + r")\s*,.*?G_CALLBACK\s*\(\s*(\w+)\s*\)",
    re.DOTALL,
)
DISCONNECT_BY_FUNC = re.compile(
    r"g_signal_handlers_disconnect_by_func\s*\(\s*(" + GROUP + r")\s*,\s*(\w+)\s*,",
    re.DOTALL,
)
CONNECT_TO_ID = re.compile(
    r"(\w+)\s*=\s*g_signal_connect\w*\s*\(\s*(" + GROUP + r")\s*,",
    re.DOTALL,
)
DISCONNECT_BY_ID = re.compile(
    r"g_signal_handler_disconnect\s*\(\s*(" + GROUP + r")\s*,\s*(\w+)\s*\)",
    re.DOTALL,
)


def line_of(text, pos):
    return text.count("\n", 0, pos) + 1


def check_file(path):
    text = path.read_text(errors="replace")
    problems = []

    by_func = {}
    for match in CONNECT_BY_FUNC.finditer(text):
        by_func.setdefault(match.group(2), set()).add(match.group(1))

    by_id = {}
    for match in CONNECT_TO_ID.finditer(text):
        by_id.setdefault(match.group(1), set()).add(match.group(2))

    for match in DISCONNECT_BY_FUNC.finditer(text):
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

    for match in DISCONNECT_BY_ID.finditer(text):
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

    failures = 0
    for path in sorted(root.rglob("*.c")):
        for line, message in check_file(path):
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
