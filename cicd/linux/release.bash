#!/usr/bin/env bash

##	- Purpose: Build the Linux release artifacts under the names the installers
##	  look for: nemo-anywhere-<ver>-linux-<arch>.tar.gz plus a shared
##	  nemo-anywhere-<ver>-sha256sums.txt (see design.md, Delivery).
##	- Build box is nemo-build-jammy (Ubuntu 22.04), NOT the day-to-day nemo-build
##	  container: the release binary's glibc floor is whatever it was built against,
##	  and trixie's 2.41 would rule out every distro older than 2025. See the
##	  Dockerfile beside this script.
##	- The image and container are created on demand, so a fresh clone can cut a
##	  release with one command.
##	- The sums file covers every artifact sitting in the release dir, so a Windows
##	  exe dropped in beside the tarball (the CI builds and signs that one) is
##	  covered by the same file the installers verify against.
##	- Syntax: release.bash [--clean]      (--clean forces a from-scratch build)

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
SLUG="nemo-anywhere"
IMAGE="${NEMO_RELEASE_IMAGE:-nemo-build-jammy:latest}"
CONTAINER="${NEMO_RELEASE_CONTAINER:-nemo-build-jammy}"
OUT="${ROOT}/cicd/artifacts/release"
BUILD=/build-release
STAGE=/build-prefix

fEcho(){ echo "[ $* ]"; }
fEcho_Clean(){ echo "$*"; }
fDie(){ echo "FAILED: $*" >&2; exit 1; }

clean=0
case "${1:-}" in
	--clean) clean=1 ;;
	-h|--help) sed -n '/^##	- Purpose:/,/^##	Copyright/p' "${BASH_SOURCE[0]}" | sed '$d; s/^##	\{0,1\}//'; exit 0 ;;
	"") ;;
	*) fDie "unknown option: $1 (try --help)" ;;
esac

ver="$(grep -oP "version\s*:\s*'\K[^']+" "${ROOT}/source/meson.build" | head -1)"
[[ -n "$ver" ]] || fDie "no version in source/meson.build"


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Build box

fEcho_Clean ""
fEcho "Release build box"
docker info >/dev/null 2>&1 || fDie "docker daemon is not reachable"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
	fEcho_Clean "building image ${IMAGE}"
	docker build -t "$IMAGE" "${ROOT}/cicd/linux/" >/dev/null
fi
if ! docker exec "$CONTAINER" true 2>/dev/null; then
	fEcho_Clean "starting container ${CONTAINER}"
	docker rm -f "$CONTAINER" >/dev/null 2>&1 || true
	## --init reaps orphans; --ulimit core=0 keeps crash dumps out of the mounted tree.
	docker run -d --init --ulimit core=0 --name "$CONTAINER" --shm-size=2g \
		-v "${ROOT}:/src" "$IMAGE" sleep infinity >/dev/null
fi
fEcho_Clean "$(docker exec "$CONTAINER" sh -c '. /etc/os-release; printf "%s, glibc %s, gtk %s" "$PRETTY_NAME" "$(ldd --version | head -1 | grep -oE "[0-9]+\.[0-9]+$")" "$(pkg-config --modversion gtk+-3.0)"')"

arch="$(docker exec "$CONTAINER" uname -m)"


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Build, smoke, stage

fEcho_Clean ""
fEcho "Building ${SLUG} ${ver} (linux-${arch})"
((clean)) && docker exec "$CONTAINER" rm -rf "$BUILD" || true
docker exec "$CONTAINER" sh -c "
	set -e
	if [ -d ${BUILD} ]; then reconf=--reconfigure; else reconf=; fi
	meson setup \$reconf --buildtype=release -Dstrip=true -Dprefix=/opt/${SLUG} ${BUILD} /src/source >/dev/null
	ninja -C ${BUILD}" | tail -1

fEcho_Clean ""
fEcho "Smoke test"
smoke="$(docker exec "$CONTAINER" xvfb-run -a "${BUILD}/src/${SLUG}" --version)"
[[ "$smoke" == "${SLUG} ${ver}" ]] || fDie "smoke test said '${smoke}', expected '${SLUG} ${ver}'"
fEcho_Clean "$smoke"

fEcho_Clean ""
fEcho "Staging prefix"
docker exec "$CONTAINER" bash "/src/cicd/linux/stage-prefix.bash" "$BUILD" "$STAGE"


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Pack

name="${SLUG}-${ver}-linux-${arch}"
fEcho_Clean ""
fEcho "Packing ${name}.tar.gz"
mkdir -p "$OUT"
rm -rf "${OUT:?}/${name}" "${OUT}/${name}.tar.gz"
## /build* is not host-mounted, so docker cp is the only way out of the container.
docker cp "${CONTAINER}:${STAGE}" "${OUT}/${name}" >/dev/null
tar -czf "${OUT}/${name}.tar.gz" -C "$OUT" \
	--owner=0 --group=0 --numeric-owner --mtime="@$(git -C "$ROOT" log -1 --format=%ct)" \
	"${name}"
rm -rf "${OUT:?}/${name}"

## One sums file per release covering every artifact present - the installers grep
## it for their own asset's line, so extra lines (the Windows exe) are free.
sums="${SLUG}-${ver}-sha256sums.txt"
tmpsums="$(mktemp)"
rm -f "${OUT:?}/${sums}"
( cd "$OUT" && find . -maxdepth 1 -type f -printf '%P\n' | sort | xargs sha256sum ) > "$tmpsums"
mv "$tmpsums" "${OUT}/${sums}"
chmod 644 "${OUT}/${sums}"	# mktemp makes it 0600

fEcho_Clean ""
fEcho "Artifacts in cicd/artifacts/release"
( cd "$OUT" && find . -maxdepth 1 -type f -printf '  %-52f %10s bytes\n' | sort )
fEcho_Clean ""
fEcho_Clean "$(cat "${OUT}/${sums}")"
fEcho_Clean ""


##	History:
##		- 2026-08-04 JC: Created (Linux half of the v1.0.0-beta1 release assets).
