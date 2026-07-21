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

- ✅ Install nemo-anywhere and upstream Nemo into separate prefixes and confirm both run simultaneously without conflict (real side-by-side runtime proof)

- 🔘 Isolate per-file view metadata keys (`metadata::nemo-*`) so the two builds don't share icon-view/layout state on the same files

### Milestone 2 - Decouple from Cinnamon (benefits every target)

- ✅ Remove desktop management entirely (Nemo Anywhere is a file manager, not a desktop shell)

	- Deleted the `nemo-anywhere-desktop` binary, the desktop-application subclass, desktop windows/manager/overlay/icon-views, the `x-nemo-desktop://` directory model in libnemo-private, the `org.Cinnamon` D-Bus proxy, and the desktop autostart; stripped the "am I the desktop?" branches throughout the file manager. Builds and runs green; kept the `.desktop` launcher-file editor and the monitor-geometry util (both real file-manager features)

- ✅ Isolate xapp / cinnamon-desktop coupling (reimplement portably, not just disable)

	- xapp favorites: own favorites store + `favorites:///` scheme in libnemo-private, settings under our own schema
	- xapp status-icon/taskbar-progress: tray icon via GTK, taskbar progress dropped (Mint-only WM protocol); icon-chooser dialog now a file picker with preview
	- cinnamon-desktop thumbnailer: own freedesktop-spec thumbnailer in libnemo-private (also fixed cache-dir creation on fresh homes); session-user pwent via POSIX

- ✅ Prove a de-Cinnamon Linux build that runs standalone (no xapp, no cinnamon-desktop) on any desktop or none

	- Builds and links with neither library; favorites and thumbnails verified working on the standalone build

- 🔘 Portable fallbacks for the remaining Mint-flavored theme icon names (`xsi-*`) - menus/toolbars reference ~77 unique names that only Mint themes ship; map to standard freedesktop names or bundle icons. Pre-existing gap (icons already missing on non-Mint), cosmetic only

### Milestone 3 - First cross-platform target (Windows)

- ✅ Choose and stand up the Windows toolchain - cross-compile from Linux with mingw-w64, smoke-test under wine. Dedicated `nemo-winbuild` container; GTK3 deps are prebuilt MSYS2 packages unpacked into a sysroot (`cicd/win/`)
- ✅ Get GTK3 + GLib/GIO building/available on the chosen toolchain - `meson setup --cross-file` configures clean (all deps resolve from the sysroot); Unix-only deps (gio-unix, x11, gobject-introspection) guarded behind `host_machine.system()`
- 🛠️ Compile on Windows, stubbing/excluding hard platform deps - first cut done; X11/gdkx cascade eliminated (dropped X11 type leak from `eel-gtk-extensions.h`). ~19 objects still failing on POSIX gaps: pwd/grp/getpwuid/getuid/geteuid, sys/wait, pathconf, S_ISUID/GID, realpath, gio-desktopappinfo, 3 direct gdkx includes + the X11 geometry parser
- 🔘 Launch on Windows and browse the local filesystem
- 🔘 Map drive letters / roots into the location model
- 🔘 Make the CICD test gate resilient to a down/absent docker daemon - detect and auto-start (or skip-with-warning) instead of a raw socket error aborting every dev/main push; revisit whether one container-Linux smoke test is still a meaningful gate once Windows/cross lanes exist

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

- ✅ Adopt the local-only delivery model: `dev` = integration target, `main` = release-only (dev->main = release cut); feature branches merge `--no-ff` into dev
	- Copied as high-level concepts/actions (not language tooling) from the sibling project; identity-swapped to t00mietum
- ✅ Stand up the local pipeline: `cicd/cicd.bash` engine + `config.bash`, git backup+publish, `release.bash`, and a pre-push merge gate (`--gate`, installed via `cicd/hooks/install.bash`)
	- Ready + verified now: container meson build + `--version` smoke test, and backup+publish. Gate passes (prints `nemo-anywhere 6.6.4`)
- 🛠️ Enable the disabled stages as the build matures (present but commented with `NEEDS:` notes; nothing was pre-ported)
- 🔘 Add a C formatter/linter gate (clang-format/clang-tidy, or meson warnings-as-errors) and wire it into format/lint stages
- 🛠️ Get release binaries onto the host (mount a build dir or `docker cp` out of the container `/build`) + an optimized buildtype, then turn on artifact collection (`RELEASE_ENABLE`/`RELEASE_ARTIFACT_DIR`)
	- ✅ Host dogfood path proven: `--buildtype=release` install staged in-container, `docker cp`'d out to a self-contained prefix beside the dogfood bin, launched via a small env-wiring wrapper - runs on the host (schema/extension lib/data all resolve). Refresh recipe in memory.
	- 🔘 Still to wire into the pipeline itself: an optimized-size buildtype choice, and automatic artifact collection into `RELEASE_ARTIFACT_DIR` (stage still `RELEASE_ENABLE=0`)

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
