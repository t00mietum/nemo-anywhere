#!/bin/bash

#  shellcheck disable=2001  ## 'See if you can use ${variable//search/replace} instead.' Complains about good uses of sed.
#  shellcheck disable=2016  ## 'Expressions don't expand in single quotes, use double quotes for that.' I know, and I often want an explicit '$'.
#  shellcheck disable=2034  ## 'variable appears unused.' Complains about valid use of variable indirection (e.g. later use of local -n var=$1)
#  shellcheck disable=2046  ## 'Quote to prevent word-splitting.' (OK for integers.)
#  shellcheck disable=2086  ## 'Double quote to prevent globbing and word splitting.' (OK for integers.)
#  shellcheck disable=2128  ## 'Expanding an array without an index only gives the element in the index 0.' False hits on associative arrays.
#  shellcheck disable=2155  ## 'Declare and assign separately to avoid masking return values.' Cumbersome and unnecessary.

##	Purpose:
##		- Project-specific CI/CD settings for nemo-anywhere. cicd.bash stays generic;
##		  this file wires each stage to a concrete command.
##		- The stage sequence (format -> build -> test -> profile -> release -> package
##		  -> dogfood -> publish) is the enduring shape carried over from the source
##		  pipeline. What differs here is the toolchain: nemo-anywhere is C/GTK built
##		  with meson/ninja inside the `nemo-build` container, not a Rust/cargo tree.
##		- Stages nemo-anywhere can do TODAY are wired live: debug build, smoke test,
##		  git backup+publish. The rest are present but DISABLED - each keeps the
##		  original cargo-era line commented out verbatim plus a "NEEDS:" note on what
##		  a meson/C equivalent would take. Nothing below has been ported; unready
##		  stages self-skip.
##	History: At bottom of script.

##	Copyright © 2026 t00mietum (ID: f⍒Ê🝅ĜᛎỹqFẅ▿⍢Ŷ‡ʬẼᛏ🜣)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT


## Check if sourced
declare -i isSourced_t6wqf=0; [[ "${BASH_SOURCE[0]}" == "${0}" ]] || isSourced_t6wqf=1
((isSourced_t6wqf)) || { echo -e "\nError in $(basename "${BASH_SOURCE[0]}"): This script is meant to be 'sourced' from within another script.\n"; exit ${ERRNUM_MSG_ALREADY_SHOWN:-3}; }


#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Identity
APP_NAME="Nemo Anywhere"
EXE_NAME="nemo-anywhere"

## The reference Linux build runs inside this container (image nemo-build-deps:latest,
## created with --shm-size=2g). It mounts the repo root (github/) at /src and builds
## into /build. See design.md "Building". The build/test commands below auto-start it.
NEMO_CONTAINER="nemo-build"

## Directory of this config, for locating sibling helper scripts (self-contained,
## no reliance on cicd.bash internals).
_cfgdir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

## Wrapper that runs a shell command in NEMO_CONTAINER but stays resilient to a
## down/absent docker daemon: it skips-with-warning (exit 0) on an environmental
## miss instead of aborting the push with a raw socket error, and propagates a
## genuine build/smoke failure. See utility/docker-run.bash.
DOCKER_RUN="${_cfgdir}/utility/docker-run.bash"


#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Stage 1: format the source in place before anything is compiled or tested.
## Decided: NO in-place formatter for the inherited tree (a full reformat would
## bury real history in churn). The gate is the check-only lint below instead.
FMT_CMD=()
FMT_CHECK_CMD=()

## Pinned helper-tool versions the engine warns on when drifted. NOT READY - unset.
## NEEDS: pins for whatever C-side tools the pipeline ends up depending on (meson,
## ninja, a formatter). The cargo tool pins below don't apply.
#	Rust-era original (reference only):
#	TOOL_PINS=(
#		"cargo-deny|0.19.9|cargo deny --version"
#		"cargo-zigbuild|0.23.0|cargo-zigbuild --version"
#		"cargo-deb|3.7.0|cargo-deb --version"
#		"cargo-generate-rpm|0.21.0|cargo-generate-rpm --version"
#		"makensis|3.11|makensis -VERSION"
#	)


#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Stage 2: debug build - READY. Configure (or reconfigure) and build in-container.
## The wrapper starts the container (and skips gracefully if docker is down) so a
## stopped box or dead daemon doesn't fail the stage. This is what the smoke runs
## against.
## -j is not optional: left alone ninja takes cores+2, and the engine's whole
## point in computing CICD_MAX_JOBS is that a pipeline run leaves the box usable.
DEBUG_BUILD_CMD=(bash "${DOCKER_RUN}" "debug build" "
	if [ -f /build/build.ninja ]; then meson setup --reconfigure /build /src/source
	else meson setup /build /src/source; fi && ninja -C /build -j ${CICD_MAX_JOBS:-2}
")

## Stage 3: regression tests - PARTIAL. There is no real test suite yet, so "tests"
## is a headless launch + --version smoke check inside the container (proves the
## build links and starts). Runs on the container's Xvfb via xvfb-run. Same wrapper,
## so a down/absent daemon skips-with-warning instead of aborting the gate.
## NEEDS: an actual regression suite (behavioral/unit) once there's portable code to
## assert on; swap this smoke check for it, or run both.
TEST_CMD=(bash "${DOCKER_RUN}" "smoke test" 'xvfb-run -a /build/src/nemo-anywhere --version')

## Stage 3 (after tests): lints - READY. Check-only cppcheck over the CHANGED C
## files only (cicd/utility/lint-c.bash); never reformats, never lints the whole
## inherited tree. A box without cppcheck skips with a warning (probe below).
LINT_PROBE=(cppcheck --version)
LINT_CMD=(bash cicd/utility/lint-c.bash)

## Stage 3 (after lints): dependency policy (licenses/advisories). NOT READY - unset.
## NEEDS: a C-world equivalent if wanted (there is no Cargo.lock to police); likely
## not applicable until there are vendored deps.
#	Rust-era original (reference only):
#	DENY_PROBE=(cargo deny --version)
#	DENY_CMD=(cargo deny check)

## Stage 3 (last): a headless behavioral harness. NOT READY - unset.
## NEEDS: an app-specific harness (the source project drove a terminal-scroll
## regression); nemo-anywhere has none yet.
#	Rust-era original (reference only):
#	SCROLL_HARNESS=(cicd/tests/scroll/run.bash)
#	SCROLL_HARNESS_WAYLAND=1


#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Stage 5: native release build. NOT READY - disabled (RELEASE_ENABLE=0).
## The container builds into its own /build, which is NOT mounted back to the host,
## so there's no host-side release binary to collect, version, or dogfood yet. The
## build is also a single meson buildtype right now (no separate optimized release).
## NEEDS: (1) an optimized release buildtype (meson --buildtype=release or a release
## build dir), and (2) get the binary onto the host - either mount a build dir or
## `docker cp` it out - before artifact collection/packaging/dogfood can run.
RELEASE_ENABLE=0
RELEASE_NATIVE_CMD=()
RELEASE_NATIVE_BIN=""
RELEASE_NATIVE_OSARCH="linux-x86_64"
#	Rust-era original (reference only):
#	RELEASE_NATIVE_CMD=(cargo build --release)
#	RELEASE_NATIVE_BIN="target/release/${EXE_NAME}"

## Stage 5: cross-release targets. NOT READY - disabled.
## NEEDS: a meson cross-compilation story per target (Windows via MSYS2/MinGW-w64 is
## the first planned toolchain; ARM later). This is Phase 2 work - see design.md.
BUILD_CROSS=0
CROSS_TARGETS=()
#	Rust-era original (reference only - cargo/zig cross, not applicable to meson):
#	CROSS_TARGETS=(
#		"Windows x86_64 (mingw)|windows-x86_64|target/x86_64-pc-windows-gnu/release/${EXE_NAME}.exe|cargo build --release --target x86_64-pc-windows-gnu"
#		"Linux ARM64 (zig)|linux-arm64|target/aarch64-unknown-linux-gnu/release/${EXE_NAME}|cargo zigbuild --release --target aarch64-unknown-linux-gnu"
#		"Windows ARM64 (zig)|windows-arm64|target/aarch64-pc-windows-gnullvm/release/${EXE_NAME}.exe|cargo zigbuild --release --target aarch64-pc-windows-gnullvm"
#	)

## Stage 5 (after builds): collect versioned artifacts + sha256sums.
## The artifacts themselves come from the per-platform release lanes, not from this
## engine stage: cicd/linux/release.bash writes the Linux tarball + the sums file
## here, and the Windows exe is built and signed by the release-win workflow. Setting
## the dir is what lets utility/release.bash verify and attach them.
## NEEDS (before RELEASE_ENABLE can flip to 1): the engine's own collector wipes this
## dir and copies bare RELEASE_NATIVE_BIN binaries under Cargo-shaped version parsing
## - it has to learn the meson version form and the archive contract first.
RELEASE_ARTIFACT_DIR="cicd/artifacts/release"
VERSION_MANIFEST="source/meson.build"
#	Rust-era original (reference only):
#	RELEASE_ARTIFACT_DIR="cicd/artifacts/release"
#	VERSION_MANIFEST="source/Cargo.toml"


#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Stage 6: distributable packages - READY.
## Each entry is "label|shell command"; a failure warns and the rest still run.
## Both steps work from what the per-platform release lanes already produced, so
## nothing is rebuilt here:
##   - .deb and .rpm are made from the Linux release tarball, and install the same
##     relocatable prefix under /opt plus a launcher, menu entry and icons. The
##     .deb's dependency versions are read in the Ubuntu release container so the
##     package claims the same floor the binary was built against.
##   - The Windows .zip is flattened out of the cross-build - exe at the folder
##     root beside its DLLs, which is the layout install.ps1 expects.
## Deferred: BSD .pkg, macOS .pkg, AppImage, Flatpak - no toolchain here yet.
PACKAGE_ENABLE=1
PACKAGE_CMDS=(
	"Linux .deb + .rpm|bash cicd/linux/package.bash"
	"Windows .zip|bash cicd/win/pack-zip.bash"
)


#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Stage 4: profiler (non-gating flamegraph artifact) - READY.
## profile-run.bash browses a generated folder tree on a private headless display
## while sampling every thread, and renders a flamegraph. It samples by attaching
## a debugger rather than with perf, because perf needs a privileged sysctl on this
## box - the trade is wall-clock samples, so a blocked thread reads as waiting.
## flame-report.py knows that and reports busy time separately.
## It profiles the DEBUG build (stage 2): the release binaries are stripped, and a
## flamegraph with no function names is worthless.
PROFILE_ENABLE=1
PROFILE_SECS=15
PROFILE_CMD=(bash cicd/utility/profile-run.bash)
PROFILE_PROBE=(gdb --version)
PROFILE_OUT_DIR="cicd/artifacts/profiling"
PROFILE_STRICT=0
## Unused by the current profiler; the engine's preflight still prints them.
PROFILE_WORKLOAD_SCRIPT=""
PROFILE_WORKLOAD_ARGS=""

## Pre-publish README screenshot refresh + demo video: NOT READY - off.
## NEEDS: headless screenshot/record hooks for the file-manager UI if wanted later.
SHOTS_ENABLE=0
DEMO_ENABLE=0

## Full-run output is tee'd here (gitignored) so warnings from any stage can be
## reviewed after the fact. READY - kept.
LINT_LOG_DIR="cicd/artifacts/lint"


#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Stage 7: dogfood (install the native release locally). NOT READY - disabled
## (empty dest lists -> stage self-skips).
## NEEDS: a host-side release binary (see RELEASE_ENABLE note) and a chosen install
## story. nemo-anywhere installs via `meson install` into a prefix and co-installs
## alongside upstream Nemo, so "dogfood" here likely means installing to a throwaway
## prefix and launching from it, not dropping a single binary into ~/.local/bin.
DOGFOOD_FIXED_DESTS=()
DOGFOOD_ROTATING_DESTS=()
DOGFOOD_PREFIX=""
#	Rust-era original (reference only - single-binary drop, not how meson installs):
#	DOGFOOD_FIXED_DESTS=("${HOME}/synced/0-0/common/exec/util/linux/bin" "/usr/local/sbin")
#	DOGFOOD_ROTATING_DESTS=("${HOME}/.local/bin"); DOGFOOD_PREFIX="nmanywdf"


#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Stage 8: backup + publish to git (runs from repo root). READY.
GIT_PUBLISH=(cicd/utility/n8git_backup-and-publish)

## Set non-empty to publish hands-off (suppresses the prompt and supplies the commit
## message so `git commit` won't open an editor). Empty = interactive unless -m/-y.
PUBLISH_AUTO_MESSAGE=""

## Extra rar excludes for the version backup live in cicd/rar-excludes.conf, not here:
## the publisher reads that file itself, so the excludes hold when it is run standalone
## too. GIT_BACKUP_AND_PUBLISH_RAR_EXCLUDES stays available for one-off additions.


##	History:
##		- 2026-07-18: Adapted from the source pipeline's config.bash for nemo-anywhere
##		  (meson/container build). Ready: build, smoke test, publish. Rest disabled
##		  with NEEDS notes; nothing ported.
