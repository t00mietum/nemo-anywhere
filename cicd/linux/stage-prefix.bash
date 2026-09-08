#!/usr/bin/env bash

##	- Purpose: Turn a finished Linux build into the relocatable prefix released
##	  as the release tarball - the unix half of the asset contract in design.md
##	  (one top-level folder, entry point bin/nemo-anywhere).
##	- Thin prefix by design: the GTK3 runtime is NOT bundled on Linux, it comes
##	  from the distro. Only nemo's own binaries, libs and data are here (~5MB).
##	- Runs INSIDE the build container (meson, glib-compile-schemas and
##	  gtk-update-icon-cache all have to be the ones the build used).
##	- One executable, at bin/nemo-anywhere, with no wrapper script in front of it.
##	  It points XDG_DATA_DIRS and PATH at its own folder on startup and finds the
##	  extension library through an $ORIGIN rpath, so the prefix runs from anywhere.
##	- Syntax: stage-prefix.bash <build-dir> <dest-dir>   (dest is wiped and rebuilt)

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

BUILD="${1:?usage: stage-prefix.bash <build-dir> <dest-dir>}"
DEST="${2:?usage: stage-prefix.bash <build-dir> <dest-dir>}"
SLUG="nemo-anywhere"

fEcho(){ echo "[ $* ]"; }

[[ -x "${BUILD}/src/${SLUG}" ]] || { fEcho "FAILED: no binary at ${BUILD}/src/${SLUG}"; exit 1; }


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Install into a scratch DESTDIR, then lift the prefix out of it

fEcho "Staging Linux prefix -> ${DEST}"
staging="$(mktemp -d)"
trap 'rm -rf "${staging}"' EXIT
DESTDIR="${staging}" meson install -C "${BUILD}" >/dev/null

## The configured prefix is wherever bin/<slug> landed under the scratch root; take
## it from the tree rather than assuming, so a re-prefixed build still stages.
installed="$(find "${staging}" -type f -path "*/bin/${SLUG}" -print -quit)"
[[ -n "${installed}" ]] || { fEcho "FAILED: meson install produced no bin/${SLUG}"; exit 1; }
prefix="$(cd "$(dirname "${installed}")/.." && pwd)"

## Same guards install.bash puts on its own removals: absolute, and named after
## the app. ${2:?} alone would happily accept "/" or "$HOME".
[[ "${DEST:0:1}" == "/" ]] || { fEcho "FAILED: dest must be an absolute path: ${DEST}"; exit 1; }
[[ "$(basename "${DEST}")" == "${SLUG}" ]] || { fEcho "FAILED: dest must be named ${SLUG}: ${DEST}"; exit 1; }

rm -rf "${DEST}"
mkdir -p "$(dirname "${DEST}")"
mv "${prefix}" "${DEST}"


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Post-install steps meson skips under DESTDIR

## Settings moved off GSettings, so there is usually nothing to compile - but keep
## handling a schema if one ever ships again.
fEcho "Compiling schemas and icon cache"
if [[ -d "${DEST}/share/glib-2.0/schemas" ]]; then
	glib-compile-schemas "${DEST}/share/glib-2.0/schemas"
fi
gtk-update-icon-cache -qtf "${DEST}/share/icons/hicolor" 2>/dev/null || true

## Nothing here is meant to be built against - the SDK headers, pkg-config files and
## the .so devel symlink only matter to someone compiling an extension.
rm -rf "${DEST}/include" "${DEST}"/lib/*/pkgconfig
find "${DEST}/lib" -maxdepth 2 -type l -name "lib${SLUG}-extension.so" -delete


fEcho "Staged $(du -sh "${DEST}" | cut -f1) at ${DEST}"


##	History:
##		- 2026-08-04 JC: Created (Linux release prefix for the v1.0.0-beta1 assets).
