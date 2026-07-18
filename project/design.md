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
	- Cinnamon coupling - xapp, cinnamon-desktop, and Nemo drawing the Cinnamon desktop/icons. Removing this is the core "de-Cinnamon" work and benefits all targets.

	- gvfs - mounts, network shares, trash. No direct Windows/macOS equivalent; the largest gap. Replace per platform or scope out network mounts initially.

	- dbus - IPC and single-instance. Present on Linux/BSD, limited elsewhere; needs a portable path or removal.

	- POSIX file ops, permissions, inotify/kqueue, X11 - map to each platform or abstract away.

	- `.desktop` launchers, polkit ("open as root"), "open in terminal" - per-platform equivalents (Windows: `.lnk`, UAC, terminal; macOS: `.app`, `open`) or removal.

### Toolchain

First target is Windows, leaning toward MSYS2 / MinGW-w64: its GTK3 stack is well supported and stays closest to upstream's meson build. MSVC (via gvsbuild) and cross-compiling from Linux are alternatives. Final choice is confirmed when we reach that stage; the repo and a Linux baseline come first.

The Linux reference build lives in a stock Debian 13 container rather than on the dev host directly - we decided that a pinned, clean distro image is the better known-good baseline, and it sidesteps host library drift. Upstream 6.6.4 builds and runs there unmodified with distro packages only.

### Building (Linux reference)

The fork still builds exactly like upstream. Stock Debian 13 is the known-good baseline.

- Install the toolchain and dev libraries:
	- `meson ninja-build gcc pkg-config gobject-introspection intltool itstool python3-gi`
	- `libgtk-3-dev libglib2.0-dev libpango1.0-dev libatk1.0-dev libgail-3-dev`
	- `libcinnamon-desktop-dev libxapp-dev` (still required; removing these is the point of Phase 2)
	- `libjson-glib-dev libgirepository1.0-dev libgsf-1-dev libexempi-dev libexif-dev`
	- `libx11-dev libxext-dev libxrender-dev`
- Configure and build:
	- `meson setup build`
	- `ninja -C build`
- The binary lands at `build/src/nemo`. Desktop drawing is the separate `nemo-desktop` binary - just don't run that one outside Cinnamon.

### Open questions

- How far to push a clean internal platform-abstraction boundary vs. per-target `#ifdef`s.

## New project

## Project structure

### Folder structure

### Logical code structure

### Data flow

### Execution flow/loops

## Decisions along the way

## Architecture

### Software stack

### Configuration model

### Saves and persistence

### UI

### Testing
