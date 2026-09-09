#!/usr/bin/env bash

##	- Purpose: Run the regression suite and the launch smoke inside the reference
##	  build container. Handed to docker-run.bash by cicd's test stage; not meant to
##	  be run on the host.
##	- BUILD_DIR overrides the in-container build directory (default /build).

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

build="${BUILD_DIR:-/build}"

## About a third of the suite fails on "cannot open display" with no server, and
## several of those read like real assertion failures. Off the session bus too, so
## nothing in here can reach a window on the real desktop.
export DBUS_SESSION_BUS_ADDRESS='disabled:'

xvfb-run -a meson test -C "${build}" --print-errorlogs

## Nothing in the suite starts the whole program, so this stays.
xvfb-run -a "${build}/src/nemo-anywhere" --version
