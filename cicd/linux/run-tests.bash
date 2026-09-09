#!/usr/bin/env bash

##	- Purpose: Run the regression suite and the launch smoke inside the reference
##	  build container. Handed to docker-run.bash by cicd's test stage; not meant to
##	  be run on the host.
##	- The gate has no build stage of its own, so the build here is what keeps the
##	  suite from running against a stale binary.
##	- NEMO_TEST_JOBS caps both the rebuild and the number of tests at once, so a
##	  run leaves the box usable. BUILD_DIR overrides the build directory.
##	- Syntax: run-tests.bash          (no arguments)

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

case "${1:-}" in
	-h|--help) sed -n '/^##\t- Purpose:/,/^##\tCopyright/p' "${BASH_SOURCE[0]}" | sed '$d; s/^##\t\{0,1\}//'; exit 0 ;;
esac

build="${BUILD_DIR:-/build}"
jobs="${NEMO_TEST_JOBS:-2}"

## Left alone, both ninja and meson take every core, and something else is usually
## running on this box.
ninja -C "${build}" -j "${jobs}"

## About a third of the suite fails on "cannot open display" with no server, and
## several of those read like real assertion failures. The bus is disabled so a
## test that sends --quit cannot reach a copy someone is actually using.
export DBUS_SESSION_BUS_ADDRESS='disabled:'

xvfb-run -a meson test -C "${build}" --no-rebuild --num-processes "${jobs}" --print-errorlogs

## The suite covers the program's insides; this covers the argument that has to
## answer before any of them are reached.
xvfb-run -a "${build}/src/nemo-anywhere" --version
