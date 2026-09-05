#!/usr/bin/env bash

##	- Purpose: Turn a finished Linux build into the relocatable prefix released
##	  as the release tarball - the unix half of the asset contract in design.md
##	  (one top-level folder, entry point bin/nemo-anywhere).
##	- Thin prefix by design: the GTK3 runtime is NOT bundled on Linux, it comes
##	  from the distro. Only nemo's own binaries, libs and data are here (~5MB).
##	- Runs INSIDE the build container (meson, glib-compile-schemas and
##	  gtk-update-icon-cache all have to be the ones the build used).
##	- The real binary moves to libexec/ so bin/nemo-anywhere can be the wrapper
##	  that wires the environment. Everything the app looks up through XDG_DATA_DIRS
##	  (actions, search helpers, icons, mime) then resolves wherever the folder ends up.
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


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Entry point

## The prefix has to run from anywhere - the installer drops it in ~/.local/share or
## /opt, and links ~/.local/bin/nemo-anywhere at this wrapper - so every path is
## derived from the wrapper's own resolved location, never baked in.
fEcho "Writing bin/${SLUG} wrapper"
mkdir -p "${DEST}/libexec"
mv "${DEST}/bin/${SLUG}" "${DEST}/libexec/${SLUG}"
cat > "${DEST}/bin/${SLUG}" <<-'WRAPPER'
	#!/bin/sh
	# Entry point for a relocatable nemo-anywhere prefix. Resolves its own location
	# (following the symlink the installer puts on PATH) and points the runtime at
	# this folder: the extension lib and the data dirs that carry nemo's actions,
	# search helpers, icons and mime info.

	self="$0"
	while [ -L "$self" ]; do
		link="$(readlink "$self")"
		case "$link" in
			/*) self="$link" ;;
			 *) self="$(dirname "$self")/$link" ;;
		esac
	done
	prefix="$(cd "$(dirname "$self")/.." && pwd)"

	# The extension library sits under the build machine's multiarch dir, which
	# is not x86_64 on an arm64 build - glob rather than bake one triplet in.
	for libdir in "${prefix}"/lib/*-linux-gnu*; do
		[ -d "$libdir" ] && LD_LIBRARY_PATH="${libdir}${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
	done
	LD_LIBRARY_PATH="${prefix}/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
	# A launcher may already have put this share dir first.
	case ":${XDG_DATA_DIRS:-}:" in
		*":${prefix}/share:"*) ;;
		*) XDG_DATA_DIRS="${prefix}/share:${XDG_DATA_DIRS:-/usr/local/share:/usr/share}" ;;
	esac
	PATH="${prefix}/bin:${PATH}"
	export LD_LIBRARY_PATH XDG_DATA_DIRS PATH

	# Settings no longer use GSettings, so there is normally no schema here.
	if [ -d "${prefix}/share/glib-2.0/schemas" ]; then
		GSETTINGS_SCHEMA_DIR="${prefix}/share/glib-2.0/schemas${GSETTINGS_SCHEMA_DIR:+:${GSETTINGS_SCHEMA_DIR}}"
		export GSETTINGS_SCHEMA_DIR
	fi

	exec "${prefix}/libexec/nemo-anywhere" "$@"
WRAPPER
chmod 755 "${DEST}/bin/${SLUG}"

fEcho "Staged $(du -sh "${DEST}" | cut -f1) at ${DEST}"


##	History:
##		- 2026-08-04 JC: Created (Linux release prefix for the v1.0.0-beta1 assets).
