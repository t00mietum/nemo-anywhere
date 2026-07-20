<!-- markdownlint-disable MD007 -- Unordered list indentation -->
<!-- markdownlint-disable MD010 -- No hard tabs -->
<!-- markdownlint-disable MD033 -- No inline html -->
<!-- markdownlint-disable MD055 -- Table pipe style [Expected: leading_and_trailing; Actual: leading_only; Missing trailing pipe] -->
<!-- markdownlint-disable MD041 -- First line in a file should be a top-level heading -->

<!-- TOC ignore:true -->
# nemo-anywhere design

High-level design and decisions for a portable, de-Cinnamon Nemo. Companion to [backlog.md](backlog.md).

<!-- TOC ignore:true -->
## Table of contents

<!-- TOC -->

- [Overview](#overview)
	- [What and why](#what-and-why)
	- [Fork decisions](#fork-decisions)
	- [High-level port strategy](#high-level-port-strategy)
	- [Dependency landscape](#dependency-landscape)
	- [Toolchain](#toolchain)
	- [Building Linux reference](#building-linux-reference)
	- [Open questions](#open-questions)
- [New project](#new-project)
- [Project structure](#project-structure)
	- [Folder structure](#folder-structure)
	- [Logical code structure](#logical-code-structure)
	- [Data flow](#data-flow)
	- [Execution flow/loops](#execution-flowloops)
- [Decisions along the way](#decisions-along-the-way)
- [Architecture](#architecture)
	- [Software stack](#software-stack)
	- [Configuration model](#configuration-model)
	- [Saves and persistence](#saves-and-persistence)
	- [UI](#ui)
	- [Testing](#testing)
- [Delivery (CI/CD, branches, releases)](#delivery-cicd-branches-releases)

<!-- /TOC -->

## Overview

### What and why

A hard fork of linuxmint/nemo (based on 6.6.4) that decouples Nemo from Cinnamon and from Linux-desktop assumptions, so it runs standalone across platforms. Independent and divergent: no upstream contribution, no downstream sync. Started from a clean detached baseline at the fork point. License is GPL-2.0-only.

Targets, in order: Windows (first), de-Cinnamon Linux (standalone on any desktop or none), then BSD and macOS. One codebase; per-platform builds are informal labels, not separate projects.

### Fork decisions

- Among the import decisions: start from a clean detached baseline at the fork point rather than dragging in upstream commit history, giving the fresh fork an uncluttered starting point. Lineage and attribution are carried by fork.md and the retained per-file copyright/license headers, not by git ancestry.

- Base is the latest stable release tag (6.6.4), not master, to start from a known-good baseline instead of a moving unstable target.

- Name: nemo-anywhere, chosen to signal portability and the de-Cinnamon "belongs to no desktop" identity while keeping "nemo" for discoverability and honest lineage. Per-OS shorthand (e.g. "for Windows") is presentational only.

	- "Nemo Anywhere" and OG "Nemo" should be able to run at the same time on the same machine, without conflict.

- Scope of the first runnable milestone: browse/copy/move/trash/delete.

- Packaging/installer approach per platform. (Common package managers per-platform, including .deb, .rpm, .AppImage, and eventually Flatpak for Linux.)

### High-level port strategy

The unifying work is decoupling. The same de-Cinnamon, de-Linux-desktop separation benefits every target, including Linux itself. Windows is first because it forces the cleanest separation (nothing Linux-specific can be assumed).

Staged, lowest-risk-first:

1. Establish the fork repo and baseline (done).

1. Cleanly reorganize project file and folder structure, for a modern project, as if started from scratch (with light refactoring where necessary).

1. Build upstream on Linux (meson) for a known-good reference to diff behavior against.

1. Carve out the hard Cinnamon/Linux couplings behind clean boundaries (desktop management, xapp/cinnamon-desktop, gvfs, dbus) so they can be stubbed or swapped per platform.

1. Decouple from gconf/dconf, and existing config-file engine[s]. Use config file engine [SHCL](https://github.com/jim-collier/shcl) for settings and persistence.

1. Stand up the first cross-platform toolchain (Windows) and get it to compile.

1. Get it to launch and browse the local filesystem.

1. Iterate feature by feature per target, replacing platform integrations or removing them gracefully.

A de-Cinnamon Linux build tends to fall out of the same decoupling, and is a good early proof that the separation is clean before tackling Windows-specific APIs.

### Dependency landscape

Nemo is C with GTK3, built with meson. The stack splits into portable and platform-bound layers.

- Portable (GTK3 runs on Windows via MSYS2/MinGW or gvsbuild, and natively on Linux/BSD/macOS):
	- GTK3, GLib / GObject / GIO, Pango, Cairo, gdk-pixbuf.

	- GIO already abstracts some platform work (GFileMonitor, GVolumeMonitor) with per-OS backends, though coverage varies.

- Platform-bound (the real porting work):
	- Cinnamon coupling - xapp, cinnamon-desktop, and Nemo drawing the Cinnamon desktop/icons. Removing this was the core "de-Cinnamon" work and benefits all targets. Done: desktop management removed, both libraries replaced with in-tree portable equivalents (see "Decisions along the way").

	- gvfs - mounts, network shares, trash. No direct Windows/macOS equivalent; the largest gap. Replace per platform or scope out network mounts initially.

	- dbus - IPC and single-instance. Present on Linux/BSD, limited elsewhere; needs a portable path or removal.

	- POSIX file ops, permissions, inotify/kqueue, X11 - map to each platform or abstract away.

	- `.desktop` launchers, polkit ("open as root"), "open in terminal" - per-platform equivalents (Windows: `.lnk`, UAC, terminal; macOS: `.app`, `open`) or removal.

### Toolchain

First target is Windows, leaning toward MSYS2 / MinGW-w64: its GTK3 stack is well supported and stays closest to upstream's meson build. MSVC (via gvsbuild) and cross-compiling from Linux are alternatives. Final choice is confirmed when we reach that stage; the repo and a Linux baseline come first.

The Linux reference build lives in a stock Debian 13 container rather than on the dev host directly - we decided that a pinned, clean distro image is the better known-good baseline, and it sidesteps host library drift. Upstream 6.6.4 builds and runs there unmodified with distro packages only.

### Building (Linux reference)

Standard meson/ninja. Stock Debian 13 is the known-good baseline. The buildable project lives under `source/` (the repo root is kept clean), so meson is pointed there.

- Install the toolchain and dev libraries:
	- `meson ninja-build gcc pkg-config gobject-introspection intltool itstool python3-gi`
	- `libgtk-3-dev libglib2.0-dev libpango1.0-dev libatk1.0-dev libgail-3-dev`
	- `libcinnamon-desktop-dev libxapp-dev` (still required; removing these is the point of Phase 2)
	- `libjson-glib-dev libgirepository1.0-dev libgsf-1-dev libexempi-dev libexif-dev`
	- `libx11-dev libxext-dev libxrender-dev`
- Configure and build:
	- `meson setup build source`
	- `ninja -C build`
- The binary lands at `build/src/nemo-anywhere`. There is no desktop-drawing binary - desktop management was removed (see "Decisions along the way").

### Open questions

- How far to push a clean internal platform-abstraction boundary vs. per-target `#ifdef`s.

## New project

## Project structure

### Folder structure

Repo root is kept deliberately clean: docs and license files, plus a handful of top-level dirs.

- `source/` - the buildable project (meson entry point and all C sources; point meson here).
- `project/` - design and backlog.
- `assets/` - fork-authored assets.
- `utility/` - standalone helper scripts and actions.
- `cicd/` - local build/release automation (the pipeline engine, git backup+publish, release helper, git hooks). See "Delivery".
- `.github/` - repo metadata (ownership, funding).

Upstream shipped everything at the root with decades of accumulated meta-files; the fork consolidated the build under `source/` and dropped the files that no longer serve a standalone, cross-platform project (old changelogs, distro packaging, upstream CI). Internal `source/` layout is the conventional GTK/meson structure, left intact.

### Logical code structure

### Data flow

### Execution flow/loops

## Decisions along the way

- Desktop management is removed, not made optional. Nemo Anywhere is a file manager, not a desktop shell - drawing/owning the root desktop is inherently a Linux/Cinnamon-session concern and pulls in the deepest coupling (the `nemo-desktop` binary, the `org.Cinnamon` proxy, the per-monitor `x-nemo-desktop://` directory model). Cutting it outright is the cleanest de-Cinnamon step and benefits every target. Kept: the `.desktop` launcher-file properties editor and the multi-monitor geometry helper, both of which are ordinary file-manager features despite their "desktop" names.

- The remaining Cinnamon libraries (xapp, cinnamon-desktop) are reimplemented with portable equivalents rather than compiled out behind flags, so the standalone build keeps favorites, thumbnails, tray/progress feedback, and the icon chooser instead of silently losing them. This is now done - the build links neither library.

	- Favorites and the thumbnailer were adapted from their upstream implementations into libnemo-private (provenance and licenses noted per file), with settings moved under our own schema so nothing is shared with a co-installed Mint stack.
	- The tray icon uses GTK's built-in status icon (deprecated upstream but still the only portable tray mechanism). Window taskbar progress was dropped outright - it is a Mint-only window-manager protocol with no portable equivalent.
	- The icon chooser is a plain file picker with an image preview; browsing theme icons by name went away with it, which is an accepted simplification.

## Architecture

### Software stack

### Configuration model

### Saves and persistence

### UI

### Testing

## Delivery (CI/CD, branches, releases)

Guiding constraint: GitHub is dumb git hosting plus optional release storage, nothing more. No hosted CI, no Actions, as few third-party tools as possible; the whole pipeline runs locally (`cicd/cicd.bash`). This is the same delivery model proven on a sibling project, brought over as high-level concepts and actions - the branch flow, the merge gate, the release cut, the git backup+publish - not the language tooling. That sibling is a Rust/cargo project; nemo-anywhere is C/GTK built with meson/ninja in the `nemo-build` container, so each stage is wired to its meson/container equivalent (or left disabled until it exists).

- Branch flow: feature branches merge `--no-ff` into `dev` (the integration target). `main` is release-only: merging dev into main cuts a release. Nothing is ever committed directly on main. Feature-branch pushes are not gated.
- Merge gate: `cicd/cicd.bash --gate` runs as the `pre-push` hook for pushes to main or dev - the local stand-in for a hosted CI workflow. For nemo-anywhere today the gate is format-check (none yet) + lints (none yet) + tests, and "tests" is a container build followed by a headless `--version` smoke launch. Install the hook per clone with `cicd/hooks/install.bash`; override a run with `git push --no-verify` or `SKIP_GATE=1`.
- Version-bump guard: the same pre-push hook blocks a push to main unless `source/meson.build` is a strict version increase over what's on main, and (once one exists) the README `Release-<ver>` badge matches. Skips the first main push and branch deletes.
- Pipeline stages (the enduring shape; a stage self-skips when unconfigured): format -> debug build -> tests+lints -> profiler -> release build (native + cross) -> packages -> dogfood -> git backup+publish. Ready now: debug build, smoke test, backup+publish. The rest are present but disabled in `cicd/config.bash`, each carrying a `NEEDS:` note on what a meson/C equivalent would take (a C formatter/linter gate, a host-side release binary out of the container's `/build`, meson cross-compilation for Windows/ARM, Linux/Windows packaging). Nothing was speculatively ported.
- Releases: `cicd/utility/release.bash` cuts from a clean main - tag `v<version>` (version read from `source/meson.build` alone) and optional push + GitHub Release upload. Tag+push work today; artifact attach is gated until the release-build stage produces host-side artifacts, and the first release needs a `Release-<ver>` README badge added on dev.
- Backup+publish: `cicd/utility/n8git_backup-and-publish` rar-backs the project tree into `../versions/` (GFS-rotated) and then syncs/commits/pushes the current branch. It is the pipeline's last stage and can be run on its own.
- Build matrix: Linux x86_64 today (container). Windows (MSYS2/MinGW-w64) is the first cross target and is Phase 2; ARM and others follow. macOS/BSD deferred.
