#!/usr/bin/env bash

##	Purpose:
##		- The commit message an unattended run commits under, in one place so the
##		  pipeline and the publisher cannot disagree about it.
##		- As a GIT_EDITOR: git invokes it as `git-auto-msg.bash <msgfile>`. Fill an
##		  empty message; leave one git already pre-filled (e.g. a merge message).
##		  Either way, never block.
##		- As `--suggest`: print the message and exit, for a caller that wants it up
##		  front rather than at editor time.
##		- Nothing here can know what a change MEANS, so the wording stays at the
##		  shape a person types in a hurry - the area that moved, or nothing useful
##		  at all. No timestamps, no describing the run that made it.

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -euo pipefail

## --no-optional-locks: as a GIT_EDITOR this runs while the commit holds index.lock,
## and a git that decides to refresh the index would fail there.
fSuggest(){
	local areas area=""
	areas="$(git --no-optional-locks status --porcelain 2>/dev/null | sed 's/^...//; s/.* -> //; s|/.*||' | sort -u)"
	if [[ "$(printf '%s' "${areas}" | grep -c .)" == 1 ]]; then
		case "${areas}" in
			source)  area="source"    ;;
			cicd)    area="cicd"      ;;
			project) area="docs"      ;;
			assets)  area="assets"    ;;
			vendor)  area="vendor"    ;;
			.github) area="workflows" ;;
		esac
	fi
	if [[ -n "${area}" ]]; then printf '%s\n' "${area} tweaks"
	else                        printf '%s\n' "Updated"
	fi
}

if [[ "${1:-}" == "--suggest" ]]; then fSuggest; exit 0; fi

file="${1:?usage: git-auto-msg.bash <msgfile> | --suggest}"

## Anything left after dropping comment + blank lines means git pre-filled a message.
if [[ -z "$(grep -vE '^[[:space:]]*#' "$file" | tr -d '[:space:]')" ]]; then
	printf '%s\n' "${GIT_AUTO_MESSAGE:-$(fSuggest)}" >"$file"
fi
