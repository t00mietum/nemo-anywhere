#!/usr/bin/env bash

##	- Purpose: Run the cross-built Windows nemo-anywhere.exe as a GUI app on the
##	  host's live X display via host wine (not the container's - that one is headless).
##	  Stages a runtime snapshot (GTK DLLs, pixbuf loaders, icons, schemas, the exe +
##	  extension dll) from the nemo-winbuild container into cicd/artifacts/win-run,
##	  then launches wine against it. The mingw64/ layout is preserved so GTK's
##	  relative lookups (loaders, schemas, icons) resolve without extra env.
##	- The staged copy is a snapshot: the exe + extension dll are re-copied on every
##	  run when the container is up, so a fresh ninja build is picked up automatically.
##	  Pass --restage to rebuild the whole snapshot (after a sysroot change).
##	- Syntax: run-windows-build-via-wine.bash [--restage] [URI]

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST="${ROOT}/cicd/artifacts/win-run"
CONTAINER="${NEMO_WIN_CONTAINER:-nemo-winbuild}"

fEcho(){ echo "[ $* ]"; }

restage=0
uri=""
for arg in "$@"; do
	case "$arg" in
		--restage) restage=1 ;;
		*) uri="$arg" ;;
	esac
done

container_up(){ docker exec "$CONTAINER" true 2>/dev/null; }

if [[ ! -x "${DEST}/app/nemo-anywhere.exe" ]] || (( restage )); then
	container_up || { fEcho "FAILED: no staged copy and container '$CONTAINER' is not running"; exit 1; }
	fEcho "Staging wine runtime from $CONTAINER"
	docker exec "$CONTAINER" sh -c '
		set -e
		D=/src/cicd/artifacts/win-run
		rm -rf "$D"
		mkdir -p "$D/mingw64/bin" "$D/mingw64/lib" "$D/mingw64/share/glib-2.0" "$D/app"
		cd /opt/win-sysroot/mingw64
		cp bin/*.dll "$D/mingw64/bin/"
		cp bin/gdbus.exe bin/gspawn-win64-helper.exe bin/gspawn-win64-helper-console.exe "$D/mingw64/bin/" 2>/dev/null || true
		cp -r lib/gdk-pixbuf-2.0 "$D/mingw64/lib/"
		cp -r share/glib-2.0/schemas "$D/mingw64/share/glib-2.0/"
		cp -r share/icons share/themes "$D/mingw64/share/"
		cp -r etc "$D/mingw64/"'
	docker exec "$CONTAINER" chown -R "$(id -u):$(id -g)" /src/cicd/artifacts/win-run
fi

# fresh build -> fresh exe, every run the container is up
if container_up; then
	docker exec "$CONTAINER" sh -c '
		cp /build-win/src/nemo-anywhere.exe /build-win/libnemo-extension/libnemo-anywhere-extension-1.dll /src/cicd/artifacts/win-run/app/
		cp /src/source/libnemo-private/org.nemo-anywhere.gschema.xml /src/cicd/artifacts/win-run/mingw64/share/glib-2.0/schemas/'
	docker exec "$CONTAINER" chown -R "$(id -u):$(id -g)" /src/cicd/artifacts/win-run/app /src/cicd/artifacts/win-run/mingw64/share/glib-2.0/schemas
	glib-compile-schemas "${DEST}/mingw64/share/glib-2.0/schemas"
else
	fEcho "WARNING: container '$CONTAINER' not running - using the staged snapshot as-is"
	[[ -f "${DEST}/mingw64/share/glib-2.0/schemas/gschemas.compiled" ]] || glib-compile-schemas "${DEST}/mingw64/share/glib-2.0/schemas"
fi

to_win(){ echo "Z:${1//\//\\}"; }

export WINEDEBUG="${WINEDEBUG:--all}"
WINEPATH="$(to_win "${DEST}/mingw64/bin");$(to_win "${DEST}/app")"
GSETTINGS_SCHEMA_DIR="$(to_win "${DEST}/mingw64/share/glib-2.0/schemas")"
export WINEPATH GSETTINGS_SCHEMA_DIR

fEcho "Launching nemo-anywhere.exe under wine (DISPLAY=${DISPLAY:-:0.0})"
export DISPLAY="${DISPLAY:-:0.0}"
if [[ -n "$uri" ]]; then
	exec wine "${DEST}/app/nemo-anywhere.exe" "$uri"
else
	exec wine "${DEST}/app/nemo-anywhere.exe"
fi
