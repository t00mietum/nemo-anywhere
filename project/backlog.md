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
- ✅ Compile on Windows, stubbing/excluding hard platform deps - `nemo-anywhere.exe` (plus all helpers + the extension DLL) builds and links clean, and prints `nemo-anywhere 6.6.4` under wine. Linux stays green. POSIX gaps closed via a shared `nemo-posix-compat.h` (uid/gid, passwd/group, chown, getuid/geteuid all degrade to non-root no-ops on non-Unix) plus per-site guards: portable geometry parser replacing X11's `XParseGeometry`, `realpath`->`g_canonicalize_filename`, `GDesktopAppInfo` launch paths falling back to generic `g_app_info_launch_uris`, and pathconf/S_ISUID-GID-VTX/pwent-deref/vestigial-gdkx guards
- ✅ Launch on Windows and browse the local filesystem - the GTK GUI comes up under wine and browses the `C:` drive: sidebar tree (My Computer / Devices / Network), icon view of the home folder with per-type icons, item count + free space. Closed the last startup abort: nemo read a handful of Cinnamon/GNOME desktop schemas (lockdown, media-handling, terminal, privacy, interface, background) that don't exist off-Cinnamon, so `g_settings_new` aborted; added bundled `org.nemo-anywhere.compat.*` fallbacks (identical keys, neutral defaults) via a `settings_new_or_compat` helper that still prefers the real DE schema when the session has one. Headless GUI smoke scripted at `cicd/win/gui-smoke.bash` (Xvfb + wine + screenshot); Xvfb/x11 tooling added to the winbuild Dockerfile. Remaining non-fatal gaps noted for later: no gvfs metadata backend (per-file view-state not persisted), no thumbnailer binary in the sysroot, wine font fallback is cosmetic
- ✅ Map drive letters / roots into the location model - on Windows the sidebar's "My Computer" now lists each fixed drive (`(C:)`, `(D:)`, ...) as a first-class root with a disk-usage bar, replacing the meaningless Unix `file:///` "File System" root (guarded `#ifdef G_OS_WIN32`; Linux keeps the single File System root unchanged). Drives enumerated via `GetLogicalDrives` + `GetDriveTypeA==DRIVE_FIXED`; each is navigable (`file:///C:/`). Removable/optical/network drives are left to flow through the normal Devices/Network path (they carry a GMount for eject), and fixed-drive roots are de-duplicated out of all three Devices loops so they don't appear twice. Verified under wine: `(C:)`/`(Z:)` show as roots and open to their contents
- ✅ Make the CICD test gate resilient to a down/absent docker daemon - the build + smoke now route through `cicd/utility/docker-run.bash`, which probes the daemon, best-effort nudges only a rootless (non-sudo) service, and on an environmental miss (docker absent, daemon down, container gone) skips-with-warning (exit 0) so a push is never blocked by a raw socket error; a genuine build/smoke failure still propagates and gates. The daemon here is rootful, so auto-start would need `sudo` - which an unattended hook must not run, so the skip message hints the manual command instead. `DOCKER_GATE_STRICT=1` flips a miss back to a hard failure. Verified: gate PASSES normally, and skips (rc=0) with a bogus `DOCKER_HOST`
	- 🔘 Still open (deferred): revisit whether one container-Linux smoke test is a meaningful gate once Windows/cross lanes exist

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
