#!/usr/bin/env bash

## Run a command inside the reference build container, resilient to a down/absent
## docker daemon - so an unreachable daemon can't turn every gated push into a raw
## socket error. Flow: ensure docker is reachable (best-effort NON-sudo nudge),
## start the container, then exec the given shell command in it. An environmental
## miss (docker absent, daemon down, container gone) SKIPS with a warning and
## exits 0, so it never hard-blocks a push; a genuine command failure (build error,
## smoke crash) propagates its exit code and still blocks.
##
## Usage: docker-run.bash <label> <sh-command>
##   NEMO_CONTAINER      selects the container (default nemo-build).
##   DOCKER_GATE_STRICT=1 turns an environmental miss into a hard failure instead
##                       of a skip (for a run that must not silently no-op).
##
## Why not auto-start the daemon: here it's a rootful system service, so starting
## it needs `sudo systemctl start docker` - which an unattended hook must not run.
## We nudge only the rootless (per-user) service, then skip with that hint.

##	Copyright © 2026 t00mietum (ID: f⍒Ê🝅ĜᛎỹqFẅ▿⍢Ŷ‡ʬẼᛏ🜣)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT


set -Eeuo pipefail

label="${1:-container task}"
cmd="${2:-}"
container="${NEMO_CONTAINER:-nemo-build}"
strict="${DOCKER_GATE_STRICT:-0}"

fEcho(){ echo "[ $* ]"; }

## Environmental miss: skip (exit 0) so a push isn't blocked, unless strict.
skip_or_die(){
	if [[ "$strict" == "1" ]]; then
		fEcho "FAILED: ${label}: $1" >&2
		exit 1
	fi
	fEcho "WARNING: ${label} SKIPPED: $1 - not verified against the container" >&2
	exit 0
}

command -v docker >/dev/null 2>&1 || skip_or_die "docker not installed"

docker_up(){ timeout 10 docker info >/dev/null 2>&1; }

if ! docker_up; then
	## Nudge only what's startable without sudo (rootless service); harmless no-op
	## if it isn't that kind of setup. A rootful daemon stays the human's call.
	if command -v systemctl >/dev/null 2>&1; then
		systemctl --user start docker.socket >/dev/null 2>&1 || true
		systemctl --user start docker        >/dev/null 2>&1 || true
	fi
	docker_up || skip_or_die "docker daemon not reachable (try: sudo systemctl start docker)"
fi

## The built binary lives inside the persistent container; without it there's
## nothing to build in or smoke-test, so treat a missing box as an env miss too.
docker ps -a --format '{{.Names}}' 2>/dev/null | grep -qx "$container" \
	|| skip_or_die "build container '${container}' not found (create it per design.md)"
docker start "$container" >/dev/null 2>&1 || true

## Real work: its exit code is the genuine result and still gates the push.
exec docker exec "$container" sh -c "$cmd"
