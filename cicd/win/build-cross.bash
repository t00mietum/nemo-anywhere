#!/usr/bin/env bash

##	- Purpose: Run the Windows cross build in the nemo-winbuild container with the
##	  build stamp set, so the exe is reproducible. The linker writes a timestamp
##	  into the PE header, and left alone it writes the clock - meaning two builds
##	  of the same commit differ. SOURCE_DATE_EPOCH replaces it with HEAD's commit
##	  date, and docker exec does not carry the host environment across, so it has
##	  to be handed over here.
##	- Same meson/ninja invocation the build notes have always used; this only adds
##	  the stamp and the job cap.
##	- Syntax: build-cross.bash [--clean]

##	Copyright (c) 2026 Bubbles
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT


set -Eeuo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "${HERE}/../.." && pwd)"
CONTAINER="${NEMO_WIN_CONTAINER:-nemo-winbuild}"
BUILD="${NEMO_WIN_BUILD:-/build-win}"
CROSS="${NEMO_WIN_CROSSFILE:-/opt/win64.cross.txt}"

# shellcheck source=../utility/include/echo.bash
source "${ROOT}/cicd/utility/include/echo.bash"
# shellcheck source=../utility/include/source-date.bash
source "${ROOT}/cicd/utility/include/source-date.bash"

clean=0
case "${1:-}" in
	--clean)   clean=1 ;;
	-h|--help) sed -n '/^##	- Purpose:/,/^##	Copyright/p' "${BASH_SOURCE[0]}" | sed '$d; s/^##	\{0,1\}//'; exit 0 ;;
	"") ;;
	*) fDie "unknown option: $1 (try --help)" ;;
esac

## A fresh clone has none yet, so it is made from cicd/win/Dockerfile on first
## use, the same way nemo-build is. --init reaps orphans (wine leaves plenty);
## --ulimit core=0 keeps crash dumps out of the mounted tree.
if ! docker ps -a --format '{{.Names}}' 2>/dev/null | grep -qx "$CONTAINER"; then
	if ! docker image inspect nemo-winbuild-deps:latest >/dev/null 2>&1; then
		fEcho "building image nemo-winbuild-deps (first run only, takes a while)"
		docker build -t nemo-winbuild-deps:latest "${ROOT}/cicd/win/" >/dev/null || fDie "could not build image nemo-winbuild-deps"
	fi
	fEcho "creating container ${CONTAINER}"
	docker run -d --init --ulimit core=0 --shm-size=2g --name "$CONTAINER" \
		-v "${ROOT}:/src" nemo-winbuild-deps:latest sleep infinity >/dev/null || fDie "could not create container '${CONTAINER}'"
fi
## A reboot leaves it stopped, with no restart policy.
docker exec "$CONTAINER" true 2>/dev/null || docker start "$CONTAINER" >/dev/null 2>&1 || true
docker exec "$CONTAINER" true 2>/dev/null || fDie "cross-build container '${CONTAINER}' is not running and would not start"

fSetSourceDate "$ROOT"
fWarnIfSourceDateIsAGuess "$ROOT"

cores="$(nproc 2>/dev/null || echo 2)"
jobs="${CICD_MAX_JOBS:-$(( cores / 2 ))}"
(( jobs >= 1 )) || jobs=1

fBuild(){
	docker exec -e "SOURCE_DATE_EPOCH=${SOURCE_DATE_EPOCH}" "$CONTAINER" sh -c "
		set -e
		if [ -f ${BUILD}/build.ninja ]; then reconf=--reconfigure; else reconf=; fi
		meson setup \$reconf --cross-file ${CROSS} -Dxmp=false ${BUILD} /src/source >/dev/null
		ninja -C ${BUILD} -j ${jobs}" | tail -1
}

fStamp(){ fPeTimestamp <(docker exec "$CONTAINER" head -c 4096 "${BUILD}/src/nemo-anywhere.exe") ;}

fEcho_Clean ""
fEcho "Cross build (stamped $(date -u -d "@${SOURCE_DATE_EPOCH}" '+%Y-%m-%d %H:%M:%S UTC'))"
((clean)) && docker exec "$CONTAINER" rm -rf "$BUILD" || true
fBuild

## The stamp is part of the output, but ninja does not know that, so an exe left
## over from a build of an earlier commit looks up to date. Drop it and relink.
if [[ "$(fStamp)" != "${SOURCE_DATE_EPOCH}" ]]; then
	fEcho_Clean "restamping (the existing exe is from another commit)"
	docker exec "$CONTAINER" rm -f "${BUILD}/src/nemo-anywhere.exe"
	fBuild
fi

pe="$(fStamp)"
[[ "$pe" == "${SOURCE_DATE_EPOCH}" ]] || fDie "linker stamped ${pe:-nothing}, not ${SOURCE_DATE_EPOCH} - the toolchain is ignoring SOURCE_DATE_EPOCH"
fEcho_Clean "exe stamped ${pe}"
fEcho_Clean ""


##	History:
##		- 2026-08-26: Created.
