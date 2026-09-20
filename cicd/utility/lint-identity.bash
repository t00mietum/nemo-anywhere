#!/usr/bin/env bash

##	- Purpose: keep the code free of a real person's paths and of the wrong
##	  copyright marker. Both got in by being typed once and never looked at
##	  again, and neither shows up in a build or a test run.
##	- Scope is the code: source/, cicd/ and utility/. project/backlog.md is
##	  prose written by hand and is not this check's business.
##	- Syntax: lint-identity.bash

##	Copyright (c) 2026 Bubbles
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

fEcho(){ echo "[ $* ]"; }

## This file is left out of its own scan: the patterns below name the things
## being looked for, so it matches itself on every run.
mapfile -t files < <(git ls-files source cicd utility ':!:cicd/utility/lint-identity.bash')

## The account names these boxes actually use. A placeholder such as somebody
## or someone is what a fixture is supposed to say instead.
bad="$(printf '%s\n' "${files[@]}" | xargs -d '\n' grep -niE 'collierjr|jimcollier|/home/jim\b|/home/jim/' 2>/dev/null || true)"
if [[ -n "$bad" ]]; then
	fEcho "FAIL: a real account name or home path is baked into the code"
	printf '%s\n' "$bad"
	exit 2
fi

## The Bubbles marker belongs to the shared helper scripts, which move between
## two accounts. Anything under source/ is this project's own.
bad="$(printf '%s\n' "${files[@]}" | grep '^source/' | xargs -d '\n' grep -ln 'Bubbles' 2>/dev/null || true)"
if [[ -n "$bad" ]]; then
	fEcho "FAIL: the shared-helper copyright marker is on an application source"
	printf '%s\n' "$bad"
	exit 2
fi

## Retired markers. Anything still carrying one was copied from an old file.
bad="$(printf '%s\n' "${files[@]}" | xargs -d '\n' grep -nE '\(ID: |\(CryptogID: [A-Za-z0-9+/=]+\)' 2>/dev/null || true)"
if [[ -n "$bad" ]]; then
	fEcho "FAIL: a retired copyright marker is still in the tree"
	printf '%s\n' "$bad"
	exit 2
fi

fEcho "OK: identity lint: no findings"
