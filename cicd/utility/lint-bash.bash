#!/usr/bin/env bash

##	- Purpose: shellcheck over the project's own shell scripts. Whole-tree,
##	  unlike the C check: every script here was written for this project, so
##	  there is no inherited noise to drown in.
##	- Anything shellcheck reports fails the stage, notes included. A script that
##	  needs a rule off carries a `shellcheck disable=` line with its reason.
##	  Four scripts turn a dozen rules off in a header block; new ones go at the
##	  site instead, so the rest of the file stays covered.
##	- A missing shellcheck skips with a warning, so an unprovisioned box can't
##	  hard-block a push; SHELLCHECK_STRICT=1 turns that miss into a failure.
##	- Syntax: lint-bash.bash

##	Copyright (c) 2026 Bubbles
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

strict="${SHELLCHECK_STRICT:-0}"
cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

fEcho(){ echo "[ $* ]"; }

if ! command -v shellcheck >/dev/null 2>&1; then
	if [[ "$strict" == "1" ]]; then
		fEcho "FAILED: Bash lint: shellcheck not installed" >&2
		exit 1
	fi
	fEcho "WARNING: Bash lint SKIPPED: shellcheck not installed" >&2
	exit 0
fi

## Everything under source/ is upstream's, and n8git_backup-and-publish is a
## shared copy that lives outside this project, so neither is ours to restyle.
## The two extensionless ones are named by what runs them - git wants the hook
## called pre-push, and runfm is a command name - so they can't be found by the
## .bash suffix the rest of the tree uses.
mapfile -t files < <(git ls-files '*.bash' ':!:cicd/utility/n8git_backup-and-publish')
files+=(cicd/hooks/pre-push utility/runfm)

## -x follows sourced files, and without -P shellcheck resolves their relative
## paths against the working directory rather than the sourcing script.
fEcho "Bash lint (shellcheck) over ${#files[@]} script(s)..."
shellcheck -x -P SCRIPTDIR -S style "${files[@]}"
fEcho "OK: Bash lint: no findings"
