#!/usr/bin/env bash

##	- Purpose: Build the Windows release .zip from the cross-compiled build, so the
##	  one-liner installer has something to fetch. install.ps1 only ever looks for
##	  the contract-named zip, and until now nothing produced one.
##	- Layout is the release contract from design.md, NOT the developer snapshot:
##	  one top-level folder, with nemo-anywhere.exe at its root beside its DLLs, so
##	  the normal Windows DLL search finds them and no launcher or environment
##	  wiring is needed. The dev snapshot's split app/ + mingw64/ tree is flattened.
##	- Runs from the cross-build container, which holds both the built exe and the
##	  mingw sysroot the runtime comes from.
##	- Syntax:
##	  cicd/win/pack-zip.bash [options]
##	  Options:
##	   --out DIR   write the zip here (default: cicd/artifacts/release)

##	History: At bottom of script.

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT


set -Eeuo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${HERE}/../.." && pwd)"
SLUG="nemo-anywhere"
CONTAINER="${NEMO_WIN_CONTAINER:-nemo-winbuild}"
BUILD="${NEMO_WIN_BUILD:-/build-win}"
SYSROOT="/opt/win-sysroot/mingw64"
OUT="${ROOT}/cicd/artifacts/release"

# shellcheck source=../utility/include/echo.bash
source "${ROOT}/cicd/utility/include/echo.bash"

while (($#)); do case "$1" in
	--out)     OUT="${2:?--out needs a path}"; shift 2 ;;
	--out=*)   OUT="${1#*=}"; shift ;;
	-h|--help) sed -n '/^##	- Purpose:/,/^##	History:/p' "${BASH_SOURCE[0]}" | sed '$d; s/^##	\{0,1\}//'; exit 0 ;;
	*)         fDie "unknown option: $1 (try --help)" ;;
esac; done

command -v zip >/dev/null 2>&1 || fDie "zip is not installed"
docker exec "$CONTAINER" true 2>/dev/null || fDie "cross-build container '${CONTAINER}' is not running"
docker exec "$CONTAINER" test -x "${BUILD}/src/${SLUG}.exe" 2>/dev/null \
	|| fDie "no built exe at ${BUILD}/src/${SLUG}.exe - build the Windows target first"

ver="$(grep -oP "(?<![_[:alnum:]])version\s*:\s*'\K[^']+" "${ROOT}/source/meson.build" | head -1)"
[[ -n "$ver" ]] || fDie "no version in source/meson.build"

name="${SLUG}-${ver}-windows-x86_64"

fEcho_Clean
fEcho "Packing ${name}.zip"


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Flatten the runtime into one folder inside the container

## Everything the app loads by a path relative to the exe (icons, themes, schemas,
## pixbuf loaders, thumbnailers) keeps its share/ and lib/ shape; only the split
## between the app and the sysroot goes away.
docker exec "$CONTAINER" sh -c "
	set -e
	rm -rf /tmp/${name}
	mkdir -p /tmp/${name}/lib /tmp/${name}/share
	cd ${SYSROOT}

	cp bin/*.dll /tmp/${name}/
	for h in gdbus.exe gspawn-win64-helper.exe gspawn-win64-helper-console.exe \
	         gdk-pixbuf-thumbnailer.exe gsf-office-thumbnailer.exe; do
		[ -f \"bin/\$h\" ] && cp \"bin/\$h\" /tmp/${name}/ || true
	done
	cp -r lib/gdk-pixbuf-2.0 /tmp/${name}/lib/
	cp -r share/icons share/themes /tmp/${name}/share/
	[ -d share/thumbnailers ] && cp -r share/thumbnailers /tmp/${name}/share/ || true
	[ -d share/glib-2.0/schemas ] && { mkdir -p /tmp/${name}/share/glib-2.0; cp -r share/glib-2.0/schemas /tmp/${name}/share/glib-2.0/; } || true
	[ -d etc ] && cp -r etc /tmp/${name}/ || true

	## The app's own binaries and data go on top, so a build always wins over
	## anything of the same name from the sysroot.
	cp ${BUILD}/src/${SLUG}.exe /tmp/${name}/
	find ${BUILD} -maxdepth 3 -name '*.dll' -exec cp {} /tmp/${name}/ \; 2>/dev/null || true
	if [ -d /src/source/data ]; then
		mkdir -p /tmp/${name}/share/${SLUG}
	fi
"

## GTK needs its schemas compiled; the output is architecture-independent, so the
## container's own tool produces a file the Windows build reads fine.
docker exec "$CONTAINER" sh -c "
	[ -d /tmp/${name}/share/glib-2.0/schemas ] &&
		glib-compile-schemas /tmp/${name}/share/glib-2.0/schemas 2>/dev/null || true
"


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Pull it out and zip it

work="$(mktemp -d)"
trap 'rm -rf "${work}"' EXIT

docker cp "${CONTAINER}:/tmp/${name}" "${work}/${name}" >/dev/null
docker exec "$CONTAINER" rm -rf "/tmp/${name}"

[[ -f "${work}/${name}/${SLUG}.exe" ]] || fDie "flattened tree has no ${SLUG}.exe"

mkdir -p "$OUT"
rm -f "${OUT}/${name}.zip"
( cd "$work" && zip -qr "${OUT}/${name}.zip" "${name}" )

fEcho "OK: ${name}.zip ($(du -h --apparent-size "${OUT}/${name}.zip" | cut -f1))"
fEcho_Clean "contents: $(unzip -l "${OUT}/${name}.zip" | tail -1 | awk '{print $2}') entries, exe at the folder root"
fEcho_Clean


##	History:
##		- 2026-08-04: Created, so the Windows one-liner installer has an asset.
