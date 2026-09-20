#!/usr/bin/env bash

##	- Purpose: Keep the two Windows lanes asking for a release build. Every tag
##	  cut before 20260919 published a debug exe: neither lane passed a buildtype,
##	  and meson's default is debug, so the signed artifact carried full DWARF at
##	  15.7 MB against 3.0 MB for the Linux release binary beside it.
##	- Two halves. The text half reads the meson setup line out of each lane and
##	  fails on a missing flag; it is the only thing that can watch the hosted
##	  workflow, which runs on a release tag and nowhere else. The artifact half
##	  reads a built exe and fails on a size only a debug build reaches, so the
##	  check sits on the path that makes the file.
##	- --shipped adds the stricter half: no debug sections at all. It is for the
##	  staged bundle, not the build directory. meson's -Dstrip=true only runs on
##	  install and neither lane installs, so stage-native.bash does the stripping
##	  and calls this on what it wrote. mingw's own static objects carry a little
##	  debug information even in a release link, so the build directory copy will
##	  not pass --shipped and is not meant to.
##	- The artifact half is skipped when no exe is named and none is staged, and
##	  when objdump cannot be found, the same way lint-c.bash skips a missing
##	  cppcheck. STRICT=1 turns either skip into a failure.
##	- Syntax: check-win-build-flags.bash [--shipped] [path-to-exe]

##	Copyright (c) 2026 Bubbles
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

shipped=0
case "${1:-}" in
	--shipped) shipped=1; shift ;;
esac
exe="${1:-}"
strict="${STRICT:-0}"
## A debug link is 15.7 MB. Stripped release is 8.3 MB. Anything past this is
## debug information, whatever the section names say.
maxBytes=$(( 12 * 1024 * 1024 ))
failures=0

fEcho(){ echo "[ $* ]"; }
fFail(){ fEcho "FAIL: $*"; failures=$(( failures + 1 )); }
fSkip(){ if [[ "$strict" == 1 ]]; then fFail "$*"; else fEcho "SKIP: $*"; fi ;}

## Pull the meson setup line out of a lane and check it carries every flag. The
## lanes are written by hand in two different languages, so matching the command
## rather than the surrounding syntax is what keeps this working across both.
fCheckLane(){
	local file="$1"; shift
	local line flag

	[[ -f "$file" ]] || { fFail "${file}: not found"; return; }

	line="$(grep -n -F 'meson setup' "$file" | head -1 || true)"
	[[ -n "$line" ]] || { fFail "${file}: no meson setup line"; return; }

	for flag in "$@"; do
		case "$line" in
			*"$flag"*) ;;
			*) fFail "${file}:${line%%:*}: meson setup is missing ${flag}" ;;
		esac
	done
}

fCheckExe(){
	local path="$1"
	local bytes sections

	if ! command -v objdump >/dev/null 2>&1; then
		fSkip "objdump not found, so ${path} was not read"
		return
	fi

	bytes="$(stat -c %s "$path" 2>/dev/null || wc -c < "$path")"
	if (( bytes > maxBytes )); then
		fFail "${path}: ${bytes} bytes, over the ${maxBytes} ceiling - this is a debug link"
	fi

	(( shipped )) || return 0

	sections="$(objdump -h "$path" 2>/dev/null | grep -c '\.debug_' || true)"
	if [[ "$sections" != 0 ]]; then
		fFail "${path}: ${sections} debug sections, expected none in a shipped exe"
		objdump -h "$path" 2>/dev/null | grep '\.debug_' || true
	fi
}

fCheckLane 'cicd/win/build-cross.bash' '--buildtype=release' '-Dstrip=true'
fCheckLane '.github/workflows/release-win.yml' '--buildtype=release' '-Dstrip=true'
## The Linux lane has always passed both. Checked here so all three stay together.
fCheckLane 'cicd/linux/release.bash' '--buildtype=release' '-Dstrip=true'

if [[ -z "$exe" ]]; then
	for candidate in 'cicd/artifacts/cross/nemo-anywhere.exe' 'cicd/artifacts/win-run/nemo-anywhere.exe'; do
		[[ -f "$candidate" ]] && { exe="$candidate"; break; }
	done
fi

if [[ -n "$exe" && -f "$exe" ]]; then
	fCheckExe "$exe"
elif [[ -n "$exe" ]]; then
	fFail "${exe}: not found"
else
	fSkip "no built exe to read"
fi

if (( failures > 0 )); then
	fEcho "FAILED: Windows build flags: ${failures} problem(s)"
	exit 1
fi
fEcho "OK: Windows build flags"

##	History:
##		- 2026-09-19: Created.
