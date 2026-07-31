#!/usr/bin/env bash

##	- Purpose: Stage a self-contained Windows runtime bundle for the NATIVE
##	  (MSYS2/MinGW-w64) build of nemo-anywhere, so it runs on a box that has no
##	  MSYS2 - and can ride the dogfood sync. The native analog of the cross
##	  stager in utility/run-windows-build-via-wine.bash, but sourced from the
##	  host's /mingw64 tree instead of the container sysroot.
##	- Native /mingw64/bin holds EVERY installed package's DLLs, so a blind
##	  bin/*.dll copy would be enormous. We copy only the dependency closure:
##	  ldd is recursive on PE, so one pass per binary yields its full static
##	  import set; we union the closures of the app exes, the pixbuf loaders
##	  (dlopen'd, so not in the app's own ldd), and the runtime helper exes.
##	- Layout produced under DEST (matches the win-run snapshot n8runfm reads):
##	    app/       nemo-anywhere.exe + libnemo-anywhere-extension-1.dll (+ sibling exes)
##	    mingw64/bin, lib/gdk-pixbuf-2.0, share/{glib-2.0/schemas,icons,themes,thumbnailers}, etc
##	- Run under the mingw64 environment: MSYSTEM=MINGW64 bash cicd/win/stage-native.bash <build-dir> <dest-dir>
##	- Syntax: stage-native.bash <build-dir> <dest-dir>   (dest is wiped and rebuilt)

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

BUILD="${1:?usage: stage-native.bash <build-dir> <dest-dir>}"
DEST="${2:?usage: stage-native.bash <build-dir> <dest-dir>}"
MINGW="${MINGW_PREFIX:-/mingw64}"

fEcho(){ echo "[ $* ]"; }

[[ -f "${BUILD}/src/nemo-anywhere.exe" ]] || { fEcho "FAILED: no exe at ${BUILD}/src/nemo-anywhere.exe"; exit 1; }

fEcho "Staging native runtime -> ${DEST}"
rm -rf "${DEST}"
mkdir -p "${DEST}/app" "${DEST}/mingw64/bin" "${DEST}/mingw64/lib" \
	"${DEST}/mingw64/share/glib-2.0" "${DEST}/mingw64/etc"

## App: the main exe, its statically-linked extension dll (the exe won't even load
## without it beside it), and the sibling helper exes (open-with, connect-server, ...).
cp "${BUILD}/src/"*.exe "${DEST}/app/"
cp "${BUILD}/libnemo-extension/libnemo-anywhere-extension-1.dll" "${DEST}/app/"

## Runtime helper exes that GLib/GTK spawn or that nemo discovers as thumbnailers.
## bin is on PATH in the launched app, so these resolve; the thumbnailer .thumbnailer
## descriptors come across with share/thumbnailers below.
helper_exes=(gdbus.exe gspawn-win64-helper.exe gspawn-win64-helper-console.exe
	gdk-pixbuf-thumbnailer.exe gsf-office-thumbnailer.exe)
for h in "${helper_exes[@]}"; do
	[[ -f "${MINGW}/bin/${h}" ]] && cp "${MINGW}/bin/${h}" "${DEST}/mingw64/bin/"
done

## gdk-pixbuf loaders (dlopen'd at runtime - not in the app's ldd), then rebuild the
## cache so it points at these staged loaders rather than the host's absolute paths.
cp -r "${MINGW}/lib/gdk-pixbuf-2.0" "${DEST}/mingw64/lib/"

## Dependency closure. ldd is recursive on PE; keep only /mingw64 paths (System32 and
## the app-local dlls stay out of the bundle's mingw64/bin). Collect over every binary
## that gets loaded: the app exes+dll, the helper exes, and each pixbuf loader.
closure_bins=("${DEST}/app/"*.exe "${DEST}/app/"*.dll)
for h in "${helper_exes[@]}"; do [[ -f "${DEST}/mingw64/bin/${h}" ]] && closure_bins+=("${DEST}/mingw64/bin/${h}"); done
while IFS= read -r loader; do closure_bins+=("$loader"); done < <(find "${DEST}/mingw64/lib/gdk-pixbuf-2.0" -name '*.dll')

## app/ on PATH so ldd can resolve the extension dll; collect the unique /mingw64 dlls.
tmplist="$(mktemp)"
for bin in "${closure_bins[@]}"; do
	PATH="${DEST}/app:${MINGW}/bin:${PATH}" ldd "$bin" 2>/dev/null \
		| grep -oiE "${MINGW}/bin/[^ ]+\.dll" || true
done | sort -u > "$tmplist"
while IFS= read -r dll; do [[ -f "$dll" ]] && cp -n "$dll" "${DEST}/mingw64/bin/"; done < "$tmplist"
ndll="$(wc -l < "$tmplist")"; rm -f "$tmplist"

## Rebuild the loader cache in-place (arch-independent text; the query tool writes
## paths relative to the loader dir, so the bundle is relocatable).
if [[ -x "${MINGW}/bin/gdk-pixbuf-query-loaders.exe" ]]; then
	( cd "${DEST}/mingw64" && GDK_PIXBUF_MODULEDIR="lib/gdk-pixbuf-2.0/2.10.0/loaders" \
		"${MINGW}/bin/gdk-pixbuf-query-loaders.exe" > "lib/gdk-pixbuf-2.0/2.10.0/loaders.cache" ) || true
fi

## Schemas: nemo's own merged with the GTK ones, compiled (the app hard-aborts on a
## missing schema). glib-compile-schemas output is arch-independent.
mkdir -p "${DEST}/mingw64/share/glib-2.0/schemas"
cp "${BUILD}/../../../source/libnemo-private/org.nemo-anywhere.gschema.xml" \
	"${DEST}/mingw64/share/glib-2.0/schemas/" 2>/dev/null || \
	cp /src/source/libnemo-private/org.nemo-anywhere.gschema.xml "${DEST}/mingw64/share/glib-2.0/schemas/" 2>/dev/null || true
cp "${MINGW}/share/glib-2.0/schemas/"*.gschema.xml "${DEST}/mingw64/share/glib-2.0/schemas/" 2>/dev/null || true
cp "${MINGW}/share/glib-2.0/schemas/gschema.dtd"   "${DEST}/mingw64/share/glib-2.0/schemas/" 2>/dev/null || true
glib-compile-schemas "${DEST}/mingw64/share/glib-2.0/schemas" >/dev/null 2>&1 || true

## Data: icons (Adwaita + hicolor fallbacks), themes, and the thumbnailer descriptors.
for d in icons themes thumbnailers; do
	[[ -d "${MINGW}/share/${d}" ]] && cp -r "${MINGW}/share/${d}" "${DEST}/mingw64/share/"
done

## etc: fontconfig + gtk settings the runtime reads relative to the prefix.
for d in fonts gtk-3.0; do
	[[ -d "${MINGW}/etc/${d}" ]] && cp -r "${MINGW}/etc/${d}" "${DEST}/mingw64/etc/"
done

## Launcher at the bundle root. app/nemo-anywhere.exe can't be double-clicked
## directly: Windows only searches the exe's own dir + System32 + PATH for the 50+
## runtime dlls, never mingw64\bin, so a bare launch throws a wall of missing-dll
## dialogs. This wires PATH/schemas/data at the prefix and starts the real exe with
## no console window (wscript). CRLF so Windows Script Host is happy.
launcher="${DEST}/nemo-anywhere.vbs"
{
	printf '%s\r\n' "' Portable launcher - sets the GTK runtime env, then starts nemo-anywhere."
	printf '%s\r\n' "Set sh = CreateObject(\"WScript.Shell\")"
	printf '%s\r\n' "base = Left(WScript.ScriptFullName, InStrRev(WScript.ScriptFullName, \"\\\"))"
	printf '%s\r\n' "Set env = sh.Environment(\"PROCESS\")"
	printf '%s\r\n' "env(\"PATH\") = base & \"mingw64\\bin;\" & env(\"PATH\")"
	printf '%s\r\n' "env(\"GSETTINGS_SCHEMA_DIR\") = base & \"mingw64\\share\\glib-2.0\\schemas\""
	printf '%s\r\n' "env(\"XDG_DATA_DIRS\") = base & \"mingw64\\share\""
	printf '%s\r\n' "sh.CurrentDirectory = base & \"app\""
	printf '%s\r\n' "sh.Run \"\"\"\" & base & \"app\\nemo-anywhere.exe\"\"\", 1, False"
} > "$launcher"

bundle_mb="$(du -sm "${DEST}" 2>/dev/null | cut -f1)"
fEcho "OK: staged ${ndll} runtime dll(s); bundle ~${bundle_mb} MB -> ${DEST}"
