#!/usr/bin/env bash

##	- Purpose: Run the regression suite and the launch smoke inside the reference
##	  build container. Handed to docker-run.bash by cicd's test stage; not meant to
##	  be run on the host.
##	- Builds first, since the gate has no build stage of its own. What gets built
##	  is the working tree, not the commit being pushed, so an unfinished edit
##	  sitting there stops the gate here. Building the pushed sha in a detached
##	  worktree instead would be correct and would also be a cold build every time,
##	  which is why it is not done.
##	- NEMO_TEST_JOBS caps both the build and the number of tests at once, so a run
##	  leaves the box usable. BUILD_DIR overrides the build directory.
##	- Syntax: run-tests.bash          (no arguments)

##	Copyright (c) 2026 Bubbles
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

case "${1:-}" in
	-h|--help) sed -n '/^##\t- Purpose:/,/^##\tCopyright/p' "${BASH_SOURCE[0]}" | sed '$d; s/^##\t\{0,1\}//'; exit 0 ;;
esac

build="${BUILD_DIR:-/build}"
jobs="${NEMO_TEST_JOBS:-2}"

## Arithmetic comparison here would evaluate whatever it was handed, and 0 means
## "no limit" to both tools below - the opposite of the point.
case "${jobs}" in
	''|*[!0-9]*|0*) jobs=2 ;;
esac

## A container recreated from the image has no build directory yet.
if [[ -f "${build}/build.ninja" ]]; then
	meson setup --reconfigure "${build}" /src/source
else
	meson setup "${build}" /src/source
fi

if ! ninja -C "${build}" -j "${jobs}"; then
	echo "[ the build here is the working tree, unfinished edits included ]" >&2
	exit 1
fi

## About a third of the suite fails on "cannot open display" with no server, and
## several of those read like real assertion failures. The bus is disabled so a
## test that sends --quit cannot reach a copy someone is actually using.
export DBUS_SESSION_BUS_ADDRESS='disabled:'

xvfb-run -a meson test -C "${build}" --no-rebuild --num-processes "${jobs}" --print-errorlogs

## The suite covers the program's insides; this covers the argument that has to
## answer before any of them are reached.
xvfb-run -a "${build}/src/nemo-anywhere" --version
