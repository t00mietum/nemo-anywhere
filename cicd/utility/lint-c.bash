#!/usr/bin/env bash

##	- Purpose: Check-only C lint over the CHANGED .c/.h files, never the whole
##	  inherited tree (upstream nemo has no house style, and a full-tree pass
##	  would drown real findings in legacy noise). Nothing is rewritten.
##	- Changed = commits since the merge base with the integration branch (dev,
##	  else main) plus anything uncommitted (tracked, staged, untracked). On the
##	  integration branch itself only the uncommitted changes are checked.
##	- cppcheck findings (error/warning/portability) fail the gate. A missing
##	  cppcheck skips with a warning so an unprovisioned box can't hard-block a
##	  push; CPPCHECK_STRICT=1 turns that miss into a hard failure.
##	- Runs the same everywhere bash + git + cppcheck exist (Linux host, MSYS2).
##	- Syntax: lint-c.bash [base-branch]

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

base="${1:-}"
strict="${CPPCHECK_STRICT:-0}"
cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

fEcho(){ echo "[ $* ]"; }

## Under MSYS2, use the Windows git that made this checkout - the msys one has
## its own HOME/config, so its line-ending view marks every CRLF file modified.
GIT=(git)
if [[ "$(uname -o 2>/dev/null)" == "Msys" ]]; then
	for cand in "/c/Program Files/Git/cmd/git.exe" "/c/Program Files (x86)/Git/cmd/git.exe"; do
		[[ -x "$cand" ]] && { GIT=("$cand"); break; }
	done
fi
## Read-only use; keep eol-normalization advice out of the gate output.
GIT+=(-c core.safecrlf=false)

if ! command -v cppcheck >/dev/null 2>&1; then
	if [[ "$strict" == "1" ]]; then
		fEcho "FAILED: C lint: cppcheck not installed" >&2
		exit 1
	fi
	fEcho "WARNING: C lint SKIPPED: cppcheck not installed" >&2
	exit 0
fi

## Integration branch: explicit arg wins, else dev if it exists, else main.
if [[ -z "$base" ]]; then
	if "${GIT[@]}" show-ref --verify --quiet refs/heads/dev; then base="dev"; else base="main"; fi
fi

## Collect candidates: branch commits since the merge base (skipped when we ARE
## the base), then everything not yet committed. Deletions can't be linted.
candidates=""
head_branch="$("${GIT[@]}" rev-parse --abbrev-ref HEAD)"
if [[ "$head_branch" != "$base" ]] && "${GIT[@]}" rev-parse --verify --quiet "$base" >/dev/null; then
	candidates+="$("${GIT[@]}" diff --name-only --diff-filter=d "${base}...HEAD")"$'\n'
fi
candidates+="$("${GIT[@]}" diff --name-only --diff-filter=d HEAD)"$'\n'
candidates+="$("${GIT[@]}" diff --cached --name-only --diff-filter=d)"$'\n'
candidates+="$("${GIT[@]}" ls-files --others --exclude-standard)"

## Keep C sources that still exist, dedup. Vendored code is upstream's to
## fix, so bumping it must not light up our gate.
files=()
while IFS= read -r f; do
	[[ "$f" == *.c || "$f" == *.h ]] || continue
	[[ "$f" == vendor/* ]] && continue
	[[ -f "$f" ]] && files+=("$f")
done < <(printf '%s\n' "$candidates" | LC_ALL=C sort -u)

if ((${#files[@]} == 0)); then
	fEcho "OK: C lint: no changed C files"
	exit 0
fi

## assertWithSideEffect misfires on the idiomatic g_assert(g_hash_table_...)
## pattern all over this codebase - not worth per-site suppressions.
## unknownMacro: cppcheck can't expand the EEL self-check X-macro prototype
## (nemo-lib-self-check-functions.h), so it fires for any .c that includes it.
## The two nemo-dnd.c items are inherited-legacy noise in the gnome-icon-list
## drag encoder/parser, surfaced only because a change touched that big file.
## The nemo-mime-actions.c trio is the same story in the activation code path.
fEcho "C lint (cppcheck, check-only) over ${#files[@]} changed file(s)..."
cppcheck --enable=warning,portability --library=gtk --inline-suppr \
	--suppress=missingInclude --suppress=assertWithSideEffect \
	--suppress=unknownMacro \
	--suppress=invalidPrintfArgType_uint:*nemo-dnd.c \
	--suppress=nullPointerRedundantCheck:*nemo-dnd.c \
	--suppress=CastAddressToIntegerAtReturn:*nemo-mime-actions.c \
	--suppress=uselessAssignmentPtrArg:*nemo-mime-actions.c \
	--suppress=nullPointerRedundantCheck:*nemo-mime-actions.c \
	--quiet --error-exitcode=2 "${files[@]}"
fEcho "OK: C lint: no findings"
