<!-- markdownlint-disable MD007 -- Unordered list indentation -->
<!-- markdownlint-disable MD010 -- No hard tabs -->
<!-- markdownlint-disable MD033 -- No inline html -->
<!-- markdownlint-disable MD055 -- Table pipe style [Expected: leading_and_trailing; Actual: leading_only; Missing trailing pipe] -->
<!-- markdownlint-disable MD041 -- First line in a file should be a top-level heading -->
# Requirements

This is a product backlog just for pre-v1.0.0 release. After that, bugs, features, and enhancements will be managed in Github Issues.

<!-- TOC ignore:true -->
## Table of contents
<!-- TOC -->

- [Conventions](#conventions)
- [Milestones](#milestones)
	- [Milestone 1 - Linux baseline](#milestone-1---linux-baseline)
	- [Milestone 2 - Decouple from Cinnamon benefits every target](#milestone-2---decouple-from-cinnamon-benefits-every-target)
	- [Milestone 3 - First cross-platform target Windows](#milestone-3---first-cross-platform-target-windows)
	- [Milestone 4 - Feature port iterative, per target](#milestone-4---feature-port-iterative-per-target)
	- [Milestone 5 - More targets](#milestone-5---more-targets)
	- [Milestone 6 - CI/CD](#milestone-6---cicd)
	- [Milestone 7 - Packaging](#milestone-7---packaging)
- [Backlog](#backlog)
	- [Misc to-do](#misc-to-do)
	- [Bugs](#bugs)
	- [Features and enhancements](#features-and-enhancements)
	- [Done](#done)
		- [Done - Bugs](#done---bugs)
		- [Done - Features and enhancements](#done---features-and-enhancements)
		- [Done - Initial milestones](#done---initial-milestones)
	- [Milestone 0 - Fork setup](#milestone-0---fork-setup)
- [Future and/or deferred](#future-andor-deferred)
- [Canceled](#canceled)

<!-- /TOC -->

## Conventions

In each section, items are listed approximately from newest to oldest.

| Icon | Status
| :--: | :--
| 🔘   | Not started
| 🛠️   | Started, and/or partially complete
| ✋   | Defer
| ✅   | Complete
| 🚫   | Canceled

## Milestones

### Milestone 1 - Linux baseline

- ✅ Build upstream as-is on Linux (meson) to confirm a known-good reference
	- Builds and runs clean on stock Debian 13; done in a container since this dev box has newer mixed libs
- ✅ Note the exact dependency set and versions that produce a working build
	- Recorded in the build notes outside the repo
- ✅ Reorganize into a clean project structure; build consolidated under `source/`, root kept lean
	- Meson project moved under `source/` with its internal layout intact; builds and runs green
- ✅ Rebrand to "Nemo Anywhere" / `nemo-anywhere` so it co-installs and runs alongside upstream Nemo without conflict
	- Renamed the installed identity only (binary, helpers, D-Bus names, GSettings schema `org.nemo-anywhere.*`, config/data dirs, `.desktop`/icon/mime/polkit/man/lang, extension SDK); internal C symbols and in-binary GResource paths left as-is (no clash)
	- Settings fully isolated (fresh `org.nemo-anywhere.*` schema, `~/.config/nemo-anywhere`); does not claim `org.freedesktop.FileManager1` when upstream holds it
	- Verified by staged install: no shared-dir filename collisions; window runs headless
- 🔘 Isolate per-file view metadata keys (`metadata::nemo-*`) so the two builds don't share icon-view/layout state on the same files

### Milestone 2 - Decouple from Cinnamon (benefits every target)

- 🔘 Isolate desktop management (Nemo drawing the Cinnamon desktop/icons) behind a boundary; make it optional/removable
- 🔘 Isolate xapp / cinnamon-desktop coupling
- 🔘 Prove a de-Cinnamon Linux build that runs standalone on any desktop or none

### Milestone 3 - First cross-platform target (Windows)

- 🔘 Choose and stand up the Windows toolchain (leaning MSYS2/MinGW-w64)
- 🔘 Get GTK3 + GLib/GIO building/available on the chosen toolchain
- 🔘 Compile on Windows, stubbing/excluding hard platform deps
- 🔘 Launch on Windows and browse the local filesystem
- 🔘 Map drive letters / roots into the location model

### Milestone 4 - Feature port (iterative, per target)

- 🔘 gvfs replacement or scope-out (mounts, network, trash)
- 🔘 File operations (copy/move/delete/rename) on native APIs
- 🔘 File monitoring via GIO backends (Win32 / kqueue) or native calls
- 🔘 dbus / single-instance handling
- 🔘 Context-menu actions: open in terminal, open elevated, launchers
- 🔘 Thumbnails, icon theme, and default-app association per platform

### Milestone 5 - More targets

- 🔘 BSD
- 🔘 macOS

### Milestone 6 - CI/CD

### Milestone 7 - Packaging

## Backlog

### Misc to-do

### Bugs

### Features and enhancements

### Done

#### Done - Bugs

#### Done - Features and enhancements

#### Done - Initial milestones

### Milestone 0 - Fork setup

- ✅ Clean detached baseline from linuxmint/nemo 6.6.4 (no upstream commit history)
- ✅ Fork branding + provenance (README, FORK.md), GPL-2.0-only
- ✅ Name chosen: nemo-anywhere
- ✅ Create `t00mietum/nemo-anywhere` GitHub repo and push (visibility TBD)
	- Created public
- ✅ Strip upstream CI (`.github` Linux workflows) - keep `./github` clear of unrelated automation
	- Done in the fork-setup commit: workflows and issue templates removed

## Future and/or deferred

## Canceled
