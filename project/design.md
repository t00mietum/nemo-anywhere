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
	- [Building Windows cross](#building-windows-cross)
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
	- [Appearance and themes](#appearance-and-themes)
	- [Testing](#testing)
- [Delivery CI/CD, branches, releases](#delivery-cicd-branches-releases)

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

1. Decouple from gconf/dconf and the Windows registry. Settings and persistence move to the [SHCL](https://github.com/jim-collier/shcl) config engine (done).

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

	- gvfs - mounts, network shares, trash, per-file metadata. No direct Windows/macOS equivalent; the largest gap. Decided approach: keep gvfs as an optional runtime dependency on Linux (it is desktop-agnostic, present on virtually every distro), and fill the gaps natively per platform - see the gvfs decision under "Decisions along the way".

	- dbus - IPC and single-instance. Present on Linux/BSD, limited elsewhere; needs a portable path or removal.

	- POSIX file ops, permissions, inotify/kqueue, X11 - map to each platform or abstract away.

	- `.desktop` launchers, polkit ("open as root"), "open in terminal" - per-platform equivalents (Windows: `.lnk`, UAC, terminal; macOS: `.app`, `open`) or removal.

### Toolchain

First target is Windows. Among the options - native MSYS2/MinGW-w64 on Windows, MSVC via gvsbuild, and cross-compiling from Linux - we chose to **cross-compile from the Linux host with mingw-w64 and smoke-test under wine**. It reuses the toolchain already on the box, needs no Windows hardware, and fits the same "containerized reference build" model as Linux. The GTK3 Windows stack still comes from MSYS2, but as prebuilt packages extracted into a cross sysroot rather than a native MSYS2 environment. Native-Windows validation (running the .exe on real Windows) is deferred to when the cross build first links and runs under wine.

The Linux reference build lives in a stock Debian 13 container rather than on the dev host directly - we decided that a pinned, clean distro image is the better known-good baseline, and it sidesteps host library drift. Upstream 6.6.4 builds and runs there unmodified with distro packages only.

### Building (Linux reference)

Standard meson/ninja. Stock Debian 13 is the known-good baseline. The buildable project lives under `source/` (the repo root is kept clean), so meson is pointed there.

- Install the toolchain and dev libraries:
	- `meson ninja-build gcc pkg-config gobject-introspection intltool itstool python3-gi`
	- `libgtk-3-dev libglib2.0-dev libpango1.0-dev libatk1.0-dev libgail-3-dev`
	- `libjson-glib-dev libgirepository1.0-dev libgsf-1-dev libexempi-dev libexif-dev`
	- `libx11-dev libxext-dev libxrender-dev`
- Configure and build:
	- `meson setup build source`
	- `ninja -C build`
- The binary lands at `build/src/nemo-anywhere`. There is no desktop-drawing binary - desktop management was removed (see "Decisions along the way").

### Building (Windows cross)

Cross-compiled from Linux with mingw-w64; the GTK3 dependency stack is prebuilt MSYS2 packages unpacked into a sysroot. All of it lives in a dedicated `nemo-winbuild` container so neither the host nor the repo carries the Windows binaries.

- `cicd/win/fetch-sysroot.bash` - resolves the transitive dependency closure of a few root packages (gtk3, json-glib, libexif, libgsf) from the MSYS2 pacman database and unpacks each `.pkg.tar.zst` into `/opt/win-sysroot`. No pacman needed; the `.db` is just a tarball of `desc` files we parse ourselves.
- `cicd/win/win64.cross.txt` - meson cross file: mingw-w64 binaries, `wine` as the exe wrapper, `PKG_CONFIG_SYSROOT_DIR` pointed at the sysroot (the `.pc` files keep `prefix=/mingw64`).
- `cicd/win/Dockerfile` - builds `nemo-winbuild`: mingw toolchain + native glib codegen tools (run on the build host) + wine + the baked sysroot.
- Configure/build (source mounted at `/src`):
	- `meson setup --cross-file /opt/win64.cross.txt -Dxmp=false /build-win /src/source`
	- `ninja -C /build-win`
- Deliberately off for Windows: XMP/exempi (not packaged for mingw - `-Dxmp=false`), and the Unix-only pieces (`gio-unix`, `x11`, SELinux, Tracker) which get `host_machine.system()` guards in meson plus `#ifdef` guards in the affected C files.

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

Four layers, bottom to top, each depending only on the ones below it.

- `eel/` - a small widget and utility library inherited from the fork's ancestry: string and GTK helpers, stock dialogs, the editable label and canvas used by the icon view. It sits below everything and knows nothing about files or settings, which is why the couple of desktop-integration helpers that live here read the desktop's own settings directly rather than asking the config store.
- `libnemo-extension/` - the public plugin interface: the interfaces a third-party extension implements (menu provider, column provider, property page, info provider) and the small value types they exchange. It is a standalone shared library with its own headers, so it deliberately depends on GTK and nothing else of ours.
- `libnemo-private/` - the model. Files and directories (`NemoFile`, `NemoDirectory`) with their asynchronous attribute loading, the file operations engine, search, thumbnails, favorites, the settings store, the per-file metadata store, and the platform backends for trash, network and shell integration. No window or view lives here.
- `src/` - the application and its views: the GtkApplication, windows, tabs and slots, the icon/list/tree views, the sidebar and path bar, the properties and preferences dialogs.

Platform-specific code is kept out of the shared files where it can be: `*-win32.c` modules for trash, network, shortcuts and shell actions, and a POSIX compatibility header that lets ordinary callers compile unchanged where the platform has no equivalent. Some large shared files still carry inline platform blocks; consolidating on one convention is an open item.

### Data flow

A location is a URI throughout, and everything hangs off two model objects.

- `NemoDirectory` owns the list of files at one location and the machinery that loads them. Views ask for a set of attributes (names and sizes, mime types, deep counts, thumbnails); the directory works out what is missing, issues the asynchronous requests, and reports each answer as it lands.
- `NemoFile` is one file. Attributes arrive in stages, so a file starts out with a name and fills in over time. A file emits `changed` whenever anything about it moves, and every view redraws from that one signal - which is also why the caches added for redraw speed all invalidate there.
- Anything the filesystem does not store is layered on top: per-folder view state, custom icons, emblems and favorite markers come from the app's own metadata store and are merged into the file's attributes as they load. On Linux a gvfs metadata daemon may supply the same keys; ours takes precedence.
- Settings flow the other way. A read goes through one settings store to one file; a change emits a per-key signal, and the widgets and views that care are bound to it. An external edit to the file produces exactly the same signals as a change made in the UI.

### Execution flow/loops

One process, one main loop, and a firm rule that nothing slow runs on it.

- Startup registers the application, opens the settings store, and either creates a window or hands the location to an already-running instance over the session bus.
- The main loop drives everything the user sees. Directory loading, file operations, search and thumbnailing all run off it - GIO asynchronous calls for anything that touches a filesystem, worker threads for thumbnail generation and for the file operations engine.
- Work started off the main loop reports back on it. File operations own a progress object that the UI observes; thumbnails hand back a finished image; a completed directory load emits `done_loading`. Callbacks that outlive their object are the recurring hazard here, so long-running work holds a reference and cancels on dispose.
- Debounce and coalesce, rather than write or redraw on every event: settings saves, metadata saves, window geometry, and sidebar rebuilds all batch.

## Decisions along the way

- Desktop management is removed, not made optional. Nemo Anywhere is a file manager, not a desktop shell - drawing/owning the root desktop is inherently a Linux/Cinnamon-session concern and pulls in the deepest coupling (the `nemo-desktop` binary, the `org.Cinnamon` proxy, the per-monitor `x-nemo-desktop://` directory model). Cutting it outright is the cleanest de-Cinnamon step and benefits every target. Kept: the `.desktop` launcher-file properties editor and the multi-monitor geometry helper, both of which are ordinary file-manager features despite their "desktop" names.

- Settings moved off GSettings entirely, onto SHCL, rather than keeping the GSettings API over a SHCL-backed store. Both were on the table: a storage backend would have been a fraction of the work and left every call site untouched, but it would have kept a compiled schema to install and ship on every platform. Among these options it was decided to take the full replacement, so that configuration is one plain file the user can open, with no build-time or install-time artifact behind it. The costs are real and were accepted: roughly three hundred call sites moved, and change notification, property binding and enum mapping are now ours to maintain. Notification and binding kept the shapes they had (a detailed `changed::key` signal, a `bind` with optional mappings), so the call sites read as they did before.

	- Defaults stayed central, in one table, instead of being restated at each call site as SHCL's own guidance suggests. With a hundred and sixty-eight settings, many read from several places, a restated default is a bug waiting to happen - two call sites disagreeing about what a setting means when it is absent.
	- The `compat.*` fallback schemas introduced for non-Cinnamon sessions are gone. What they stood in for is now simply our own settings, which a desktop may override where it publishes its own answer.
	- Two of them turned out to be dead and were removed rather than carried across: the desktop background setting (nothing has read it since desktop management was removed) and the command-line lockdown setting (watched, but its value never read).

- The remaining Cinnamon libraries (xapp, cinnamon-desktop) are reimplemented with portable equivalents rather than compiled out behind flags, so the standalone build keeps favorites, thumbnails, tray/progress feedback, and the icon chooser instead of silently losing them. This is now done - the build links neither library.

	- Favorites and the thumbnailer were adapted from their upstream implementations into libnemo-private (provenance and licenses noted per file), with settings moved under our own schema so nothing is shared with a co-installed Mint stack.
	- The tray icon uses GTK's built-in status icon (deprecated upstream but still the only portable tray mechanism). Window taskbar progress was dropped outright - it is a Mint-only window-manager protocol with no portable equivalent.
	- The icon chooser is a plain file picker with an image preview; browsing theme icons by name went away with it, which is an accepted simplification.

- gvfs: keep it on Linux, replace the gaps natively elsewhere. gvfs turned out to be desktop-agnostic (a freedesktop/GIO service present on virtually every Linux desktop, not a Cinnamon thing), so on Linux it stays as an optional runtime dependency - when present it provides network shares, trash, mtp/sftp and so on; when absent the UI self-hides those entries. The per-platform gaps are filled as follows:

	- Per-file/per-folder metadata (view and sort state, custom icons, emblems, favorite markers) moves to an app-owned portable store on all platforms, replacing the gvfs metadata daemon entirely. One store, one behavior everywhere; nothing is lost on Linux since these keys are already app-private.
	- Trash on Windows: deleting to the Recycle Bin already works natively through GLib. In-app trash browsing (view, restore, empty) gets a native Recycle Bin backend rather than being scoped out.
	- Network on Windows: native networking rather than a gvfs port - UNC paths work as ordinary paths, and network browsing enumerates the Windows network neighborhood natively.
	- Virtual locations (network, computer, trash) are shown only when the running platform actually supports them, extending the runtime scheme check the codebase already uses.

- Installing is a script, not a package. The primary install path is a one-liner that fetches a release, checks it, and puts it where that platform expects - no repository to add, no dependency hunt, and no packaging format to maintain per distro. Distro packages can come later without changing this.

	- Two standalone installers rather than one script with a helper: `install.bash` (bash 3.2, so stock macOS runs it) and `install.ps1` (PowerShell 7). Each covers every platform it can reach on its own - the PowerShell one installs on unix itself instead of handing off - so neither depends on the other being present. The duplication is deliberate: it buys a one-liner that works from whichever shell someone already has open, and both are small.
	- The app installs as a whole folder plus the two things that make it reachable: a menu entry, and a name on the PATH (a symlink on unix, a PATH entry on Windows). A file manager gets launched both ways, so both are worth wiring.
	- User install is the default and needs no privileges. A system-wide install is opt-in and is the only path that escalates, which it states in the plan first.
	- Every run prints what it is about to do and waits for a yes. Downloads are checksum-verified before anything is unpacked, so a bad download can never replace a working install. Reinstalling replaces in place, and `--uninstall` removes exactly what was added.

- D-Bus and single-instance: kept as-is, no per-platform gating. Probing showed that GLib autolaunches a per-user D-Bus session bus on Windows as well, shared across processes, so GApplication's single-instance behavior works everywhere - launching a second copy hands its arguments to the first rather than opening a rival process - and the two D-Bus services (the freedesktop file-manager interface and the internal file-operations one) get a real connection. The only thing that needed hardening was the bus-less case: on a headless or minimal system, or a locked-down Windows where autolaunch fails, there is no connection at all, and the file-operations service (which only ever serves other processes) must simply not set itself up rather than fail. A single-instance process-per-window mode, if wanted, is a separate future choice layered on top of this, not a change to it.

- Path separators: `/` and `\` both work in typed locations on every platform, without reserving `\`. On Windows both are already native separators. On POSIX, `\` is a legal filename character (files created over SMB shares really do contain it), so it is not reserved and no escape syntax is introduced; instead, typed input is normalized by fallback - the literal path is tried first, and only if it does not resolve is a `\`->`/` retry attempted. Pasted Windows-style paths work, real backslash-filenames keep working, and copy-paste interop with the rest of the platform is preserved.

- Desktop-environment settings schemas are optional at runtime. Upstream read several Cinnamon/GNOME settings schemas that only exist on those desktops, and a missing schema is a hard abort in GLib. The app bundles fallback copies with the same keys and neutral defaults, and prefers the real desktop schema whenever the session provides it - Cinnamon integration is preserved, and every other environment (including Windows) starts clean.

- Windows drive letters are first-class roots: the sidebar lists each fixed drive with a disk-usage bar, replacing the single Unix filesystem root, which has no meaning on Windows. Removable, optical, and network drives stay on the normal devices path, since that path carries eject and unmount.

- Per-type file icons on Windows are derived from the file's content type, because the platform's file layer reports one generic icon for nearly every file. Thumbnails keep the freedesktop thumbnailer mechanism on every platform; the Windows runtime ships the thumbnailer tools and image-loader cache it needs.

- "Open in terminal" and "open elevated" map to native equivalents per platform. On Windows: the native console (Windows Terminal, then PowerShell, then cmd) opened at the folder, and an elevated relaunch through the normal UAC prompt, labeled "Open as Administrator". On Linux: the configured terminal and a pkexec relaunch, labeled "Open as Root".

## Architecture

### Software stack

- **Language**: C, built with meson and ninja. No C++ and no additional language runtime.
- **Toolkit**: GTK 3 with GLib/GObject/GIO. GTK 3 rather than 4 because the fork inherits a large GTK 3 codebase and GTK 3 still has the better Windows story; the deprecated pieces still in use (the status icon, a few stock dialogs) are isolated and marked.
- **Filesystem access**: GIO everywhere, with native backends filling the gaps that have no portable answer - the Windows Recycle Bin, Windows network browsing, and Windows shell shortcuts.
- **Other libraries**: json-glib for the metadata store, libexif/libgsf/exempi for file property extraction, and a single vendored header for the settings format. Deliberately absent: xapp, cinnamon-desktop, and GSettings for the app's own settings.
- **Optional at runtime**: gvfs on Linux, for network shares, trash and remote mounts. Absent, the affected entries hide themselves rather than fail.

### Configuration model

Settings are ours, in a file we own, in a format a person can read. There is no
settings daemon, no compiled schema, and no per-platform store to keep in step.

- One file, `settings.shcl`, in the same format everywhere, in whichever
  directory the platform holds per-user configuration in: `~/.config` on Linux
  and BSD, `%APPDATA%` on Windows, `~/Library/Application Support` on macOS. A
  folder left behind by an older build is moved to the new place on first run.
- The declared shape of every setting - type, default, allowed values,
  description - lives in a table in the code, and is mirrored by a schema file
  shipped with the app for validating a hand-edited config.
- Values the desktop owns rather than us are read from the desktop where it
  publishes them, and fall back to ours where it does not.

The trade accepted here: reading and writing settings is now our code rather
than a well-worn library's, and settings do not migrate from a pre-1.0 install
because nothing remains that can read the old store.

### Saves and persistence

Three separate stores, each with its own lifetime.

- Application settings (everything in the Settings dialog, plus menu toggles like Show Hidden Files) live in one plain-text SHCL file, `settings.shcl`, in the user's config directory - the same file and the same format on every platform, in whichever directory that platform keeps configuration in. Neither the Linux desktop settings database nor the Windows registry is involved any more.
	- The file is meant to be read and edited by hand. It holds only what was actually chosen: a value equal to its default is dropped, the way per-folder view state already worked, so the file stays short and a later change to a default still reaches the user. Each key carries its one-line description as a comment.
	- Edits made while the app is running are picked up straight away, so hand-editing behaves like changing the setting in the UI.
	- Types, defaults and allowed values live in a table in the code, and a matching schema ships beside the app so `shcl check --schema` can validate a hand-edited file and catch typos.
	- A handful of settings are the desktop's to decide rather than ours - which terminal to open, whether the session remembers recent files, 12h or 24h clocks. Where a desktop publishes them we read its answer; everywhere else our own value stands in. That is the only remaining use of the desktop settings database, it is read-only, and it never touches a schema of ours.

- Per-folder view state - view mode, zoom, sort column, column layout - is app-owned and portable, in a single file under the user's config directory. This replaced the Linux-only metadata service so the behaviour is identical everywhere.
	- Only a real per-folder choice is stored. A value that merely matches the current default is left out, so the folder keeps following the default if it later changes. Upstream stored it either way, which quietly pinned every folder you had ever opened.
	- Changing a default in Settings also applies to the folders already on screen. Folders you are not looking at keep their own view and zoom until you visit them.

- Window size, position, and maximized state are shared by all windows and live with the application settings. They are written shortly after a move or resize settles, rather than only when a window closes, so an abnormal exit doesn't discard them.
	- On a first run there is nothing saved yet, so the window opens at 1280x720 including its title bar and borders, with the side pane at about a fifth of the width.

Settings are deliberately isolated from an upstream Nemo installed alongside: our own config file, separate config directory, and app-private per-file keys. A few genuinely shared per-file keys (custom icons, emblems, annotations) stay interoperable on purpose.

### UI

The window is a menu and toolbar, a sidebar, a path bar, and a view - and the view is interchangeable.

- Three views share one interface: icon, compact and list, with an optional tree column in list view. Each reads its layout from per-folder state where the folder has any, and from the defaults where it does not.
- A window holds tabs; each tab is a slot with its own location, history and view. Navigation, loading state and the busy cursor belong to the slot, which is why a slow location can only block its own tab.
- The sidebar is one tree store rebuilt from bookmarks, mounts, drives and network locations. Everything that could be slow to answer - free space, mount state - is fetched off the main loop and folded in when it arrives.
- Extensions can add context-menu items, list columns, property pages and file attributes; nothing in the shipped UI depends on one being present.
- Look and feel follows the platform: the desktop's theme and font on Linux, a bundled theme set with Segoe UI and the system light/dark preference on Windows. See Appearance and themes below.
- **A launch shows something at every stage.** The window is put on screen at its remembered size and place as soon as it exists, before the first folder resolves, with its panes still empty. On Windows, where getting that far takes measurably longer, a small panel appears first - drawn with the platform's own toolkit, since it has to be up before GTK is - listing what startup is doing and leaving as soon as the real window has drawn.

### Appearance and themes

Two settings decide how the app looks: a light/dark mode, and the widget and icon themes to draw with. Both live in `settings.shcl` under `appearance`, and both apply while the app is running rather than at the next launch.

- **Mode is Light, Dark, or Follow the system.** Following means asking the platform: on Windows that is the `AppsUseLightTheme` personalisation value, watched for changes so the app turns with the rest of the desktop; anywhere the desktop has already told GTK, it means leaving that answer alone. An explicit Light or Dark overrides the platform on every target.
- **Themes are offered by the mode they suit.** A theme states which backgrounds it was drawn for; one that says nothing is judged by its name, which is how the convention already works in practice - a trailing `-dark` marks the dark half of a pair, and a theme with a `-dark` sibling is the light half. Most colourful icon sets genuinely serve both, because the monochrome half of any theme is recoloured to the foreground by GTK - and where an upstream theme does draw for dark, it turns out to redraw two or three icons and no more, so a dark variant carries only those and inherits the rest from its light half.
- **Picking one half of a pair picks the pair.** Choosing a theme and then changing mode swaps to its counterpart rather than leaving a dark theme on a light window. A widget theme that ships its own dark stylesheet needs no counterpart, since GTK swaps sheets on its own.
- **Targets unlikely to have GTK themes installed carry their own set.** That is Windows and macOS; Linux and the BSDs use what the desktop already provides. Each bundled icon theme is trimmed to the icon names a file manager actually asks for - roughly 180 - which is what keeps a theme to a few hundred KB instead of tens of MB. Anything not shipped falls through the standard `Inherits` chain to Adwaita and then hicolor, so a gap is a mismatched glyph, never a missing one.
- **The Windows XP and Windows 7 icon sets are our own artwork.** No cleanly-licensed set of either exists; what circulates is Microsoft's shell art extracted and repackaged, which this project will not ship. The two sets are drawn from a shared vocabulary of shapes and glyphs and carry the project's own license. Every other bundled theme is an upstream open-source theme, unmodified apart from the trim, keeping its own license file and a pinned source commit.
- **Themes can be dropped in on any platform**, bundled set or not, by putting an ordinary GTK theme folder in `themes` or an icon theme in `icons` beside the settings file. Drop-ins are searched before the bundled set, so a same-named theme shadows it.
- **The bundled set lives inside the binary rather than as files beside it.** It was a couple of thousand small files, and the Windows single-file build was spending nearly all of its startup unpacking them - the cost there is per file, not per byte. As one compiled-in resource it costs a few MB of binary and nothing at launch. The trade is that a bundled theme cannot be edited in place any more, which is what the drop-in folders are for; and one small icon theme still ships as files, because it is where the directory conventions the resource is matched against are defined.

### Testing

Tests are ordinary executables run by meson, and the bar for adding one is a defect that could come back.

- Each regression test is written against a specific defect and is checked by backing the fix out and watching the test fail. A test that passes either way is not evidence.
- Coverage is concentrated where the risk is: the settings parser and its bindings, the metadata store, favorites, search patterns, drag-and-drop parsing, extension objects, symlink handling, and the Windows trash and shortcut backends.
- The suite runs headless (a virtual display where GTK needs one) and forms part of the pre-push gate along with the build, a lint pass and a launch smoke test. A test that cannot run on the current platform reports a skip, never a pass.
- Interactive behaviour that no assertion can reach - the visible free-space bar, icon redraw, keyboard shortcuts - is verified by hand against a build kept on the desktop for daily use.

## Delivery (CI/CD, branches, releases)

Guiding constraint: GitHub is dumb git hosting plus optional release storage, and as few third-party tools as possible; the whole pipeline runs locally (`cicd/cicd.bash`). The one deliberate exception is a release-only GitHub Actions workflow (`.github/workflows/release-win.yml`) that builds, packs and publishes the Windows release exe. It exists because the code signing service chosen at the time would only sign artifacts produced by a verifiable public build; that application was refused and signing is deferred, so what the workflow earns its keep for now is being that reproducible public build, with the signing step left dormant. Everything else stays local. This is the same delivery model proven on a sibling project, brought over as high-level concepts and actions - the branch flow, the merge gate, the release cut, the git backup+publish - not the language tooling. That sibling is a Rust/cargo project; nemo-anywhere is C/GTK built with meson/ninja in the `nemo-build` container, so each stage is wired to its meson/container equivalent (or left disabled until it exists).

- Branch flow: feature branches merge `--no-ff` into `dev` (the integration target). `main` is release-only: merging dev into main cuts a release. Nothing is ever committed directly on main. Feature-branch pushes are not gated.
- Merge gate: `cicd/cicd.bash --gate` runs as the `pre-push` hook for pushes to main or dev - the local stand-in for a hosted CI workflow. For nemo-anywhere today the gate is format-check (none yet) + lints (none yet) + tests, and "tests" is a container build followed by a headless `--version` smoke launch. Install the hook per clone with `cicd/hooks/install.bash`; override a run with `git push --no-verify` or `SKIP_GATE=1`.
- Version-bump guard: the same pre-push hook blocks a push to main unless `source/meson.build` is a strict version increase over what's on main, and (once one exists) the README `Release-<ver>` badge matches. Skips the first main push and branch deletes.
- Version line: nemo-anywhere numbers its own releases from 1.0.0, independent of the inherited upstream code baseline (6.6.4). The project version in `source/meson.build` was reset to `1.0.0-beta1` for the first release; since 6.6.4 was never tagged or released, that reset is a clean one-time step, and the strict-increase guard governs 1.x onward.
- Pipeline stages (the enduring shape; a stage self-skips when unconfigured): remote sync -> format -> debug build -> tests+lints -> profiler -> release build (native + cross) -> packages -> dogfood -> git backup+publish. Ready now: remote sync, debug build, tests, lints, profiler, packages, backup+publish. Still disabled in `cicd/config.bash`: the format stage (no in-place C formatter, on purpose) and the engine's own release-build collector, because the per-platform release lanes produce the artifacts instead.
	- Remote sync runs first for a reason: the publish stage pulls at the end, so without it a change merged remotely during a run would be pushed having never been built or tested. It fast-forwards when the branch is only behind, wrapping any dirty tree in a stash, and stops the run outright when the branch has diverged. Skipped in gate mode, since a pre-push hook must not rewrite the tree underneath the push that called it.
	- No stage is allowed all the cores. Build parallelism is capped at half of them, so a full run leaves the machine usable.
- Profiling: `cicd/utility/profile-run.bash` browses a generated folder tree on a private headless display while sampling every thread, then renders a flamegraph into `cicd/artifacts/profiling` (GFS-rotated), and `cicd/utility/flame-report.py` prints the hot spots into the run log.
	- It samples by attaching a debugger rather than using perf. perf needs a privileged sysctl on this machine, and a profiler that cannot run without root is a profiler nobody runs. The cost is that samples are wall-clock rather than CPU time, so a blocked thread reads as work; the report keeps waiting in its own bucket and gives every other figure as a share of busy time as well as of total.
	- It profiles the debug build. The release binaries are stripped, and a flamegraph with no function names says nothing.
- Packaging: built from what the release lanes already produced, never rebuilt. `cicd/linux/package.bash` turns the Linux tarball into a `.deb` and an `.rpm`, both installing the same relocatable prefix under `/opt` plus a launcher, a menu entry and icons in the shared theme. `cicd/win/pack-zip.bash` flattens the cross-build into the Windows zip layout. BSD, macOS, AppImage and Flatpak are deferred for want of a toolchain here.
	- The `.deb`'s dependency versions are read off the built binaries inside the Ubuntu release container, not on the development box, so the package claims the same floor the binary was actually built against. `rpmbuild` derives its own requirements from the ELF, so the `.rpm` needs no such help.
- Releases: `cicd/utility/release.bash` cuts from a clean main - tag `v<version>` (version read from `source/meson.build` alone) and optional push + GitHub Release upload. Tag+push work today; artifact attach is gated until the release-build stage produces host-side artifacts. The README release badge reads the current release off GitHub, so nothing has to be bumped by hand for it; a project that used a hand-written badge instead would have that checked against the version.
	- Release notes are the hand-written changelog section for the version, never a generated commit list: `cicd/utility/changelog-notes.bash <version>` prints it, and both the local release helper and the Windows release workflow publish with it. A version with no changelog section falls back to generated notes rather than an empty body, so a release is never published blank.
	- A version carrying a pre-release part (`1.0.0-beta2`) is published as a prerelease from both sides. That matters to the installers: their `stable` channel resolves to the latest non-prerelease, so a beta is only reachable with `--release dev`.
- Backup+publish: `cicd/utility/n8git_backup-and-publish` rar-backs the project tree into `../versions/` (GFS-rotated) and then syncs/commits/pushes the current branch. It is the pipeline's last stage and can be run on its own.
	- What the archive keeps: source, project docs, the pipeline itself, the repo's own assets, and anything under `cicd/artifacts/release` - release builds and the packages cut from them. What it drops: anything a command regenerates on demand, chiefly the staged Windows runtime snapshot (~67MB of library copies, rebuilt with one `--restage`), tool logs, and crash dumps. The decision was that a version backup should hold what would be painful to lose, not what a rebuild reproduces; the practical trigger was the snapshot alone taking each archive from about 1.6MB to 36MB.
- Install: `install.bash` (Linux, BSD, WSL, macOS) and `install.ps1` (all of those plus Windows) at the repo root, run as one-liners straight from a shell. They read the releases page, so they depend on a fixed naming contract for release assets - the packaging stage has to produce exactly these names:
	- `nemo-anywhere-<version>-<os>-<arch>.tar.gz` for unix, `.zip` for Windows, with `<os>` one of `linux`/`windows` and `<arch>` one of `x86_64`/`arm64`.
	- `nemo-anywhere-<version>-sha256sums.txt` alongside them, in `sha256sum` format. This is what the installers verify against, and `release.bash` already writes and checks a file of that name.
	- Each archive holds one top-level folder. On unix its entry point is `bin/nemo-anywhere` (the wrapper that wires the runtime environment); on Windows `nemo-anywhere.exe` sits at the folder root beside its DLLs, so the normal Windows library search finds them with no environment wiring at all.
- Windows single-exe: alongside the zip, the pipeline packs the whole runtime (dlls, schemas, icons, themes) into one self-contained `nemo-anywhere.exe` with Enigma Virtual Box - an in-memory virtual file system, nothing extracted at run time. We decided this is the flagship Windows artifact: no library folder, no launcher, just an exe to copy anywhere.
	- The pack source is the same flat prefix layout the zip contract uses (exe + dlls at the root, `lib/` `share/` `etc/` beside them); GLib-stack libraries resolve their data relative to their own dll, so that tree also runs unpacked with a bare double-click.
	- Known trade-off: virtualizer-packed exes are occasionally false-flagged by antivirus; the plain zip stays available as the fallback artifact.
	- The packer is a prerequisite of the Windows build box, not of the build itself: without it the pipeline still lints, builds, tests and stages, and only the pack and dogfood stages warn-skip. The dogfood launcher then keeps serving whatever exe it last held, which reads as a build that silently stopped moving - so a box meant for day-to-day Windows work wants it installed.
- Linux release artifact: `cicd/linux/release.bash` builds it, and deliberately not in the day-to-day build container. A binary's glibc floor is whatever it was built against, so a release built on Debian 13 would refuse to start on anything older than 2025. The release box is therefore Ubuntu 22.04 (`cicd/linux/Dockerfile`, container `nemo-build-jammy`): glibc 2.35 and GTK 3.24.33 as the floor, which reaches Ubuntu 22.04, Debian 12, Mint 21 and Fedora 36 onward. Newer runtimes stay compatible; older ones cannot be.
	- Thin prefix, not a bundle: the GTK3 runtime comes from the distro rather than riding along. It keeps the download at a couple of MB, and on Linux a bundled GTK is the thing that goes stale and mismatches the desktop's theme, portals and input methods - the opposite of the Windows situation, where nothing is installed to begin with. That difference in the two platforms' artifacts is intentional.
	- What makes it relocatable: `bin/nemo-anywhere` is a wrapper that resolves its own location (through the symlink the installer puts on PATH) and points `LD_LIBRARY_PATH` and `XDG_DATA_DIRS` at the folder it sits in; the real binary moves to `libexec/`. Everything the app finds through the XDG data dirs - actions, search helpers, icons, mime info - then resolves wherever the folder was installed.
	- Still baked in at build time: the paths behind `NEMO_DATADIR` and `LIBEXECDIR`. They point at the system prefix (`/opt/nemo-anywhere`), so a per-user install loses two info-bar documents and the plugin tab's extension listing. Deriving them from the executable's own location is the open item that also fixes the same gap on Windows.
- Build matrix: Linux x86_64 today (container). Windows (MSYS2/MinGW-w64) is the first cross target and is Phase 2; ARM and others follow. macOS/BSD deferred.
