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
		- [Done - Milestones](#done---milestones)
			- [Done; Milestone 6 - CI/CD](#done-milestone-6---cicd)
		- [Done; Milestone 4 - Feature port iterative, per target](#done-milestone-4---feature-port-iterative-per-target)
			- [Done; Milestone 3 - First cross-platform target Windows](#done-milestone-3---first-cross-platform-target-windows)
			- [Done; Milestone 2 - Decouple from Cinnamon benefits every target](#done-milestone-2---decouple-from-cinnamon-benefits-every-target)
			- [Done; Milestone 1 - Linux baseline](#done-milestone-1---linux-baseline)
			- [Done; Milestone 0 - Initial](#done-milestone-0---initial)
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

### Milestone 3 - First cross-platform target (Windows)

- 🛠️ Make the CICD test gate resilient to a down or absent docker daemon.
	- Done: build and smoke steps go through a wrapper that probes the daemon first.
	- Done: an environmental miss (docker absent, daemon down, container gone) skips with a warning instead of blocking the push. A real build or test failure still gates. A strict mode turns a miss back into a hard failure.
	- Note: the daemon needs root to start, so the unattended hook never auto-starts it. The skip message shows the manual command.
	- Verified: gate passes normally, and skips cleanly when docker is unreachable.
	- 🔘 Revisit whether one container-Linux smoke test is a meaningful gate once Windows/cross lanes exist.

### Milestone 4 - Feature port (iterative, per target)

- 🛠️ Windows look: make it feel native even though it isn't Explorer.
	- ✅ Fix the thin, poorly anti-aliased text - Segoe UI 9 with full hinting and subpixel (generated settings.ini + fontconfig).
	- 🔘 Themes: bundle a Windows 11 (Fluent) icon + widget theme, light and dark. Keep a lightweight Linux light/dark pair compiled in (Adwaita / Adwaita-dark). Permissive licenses only - not Microsoft's own art.
	- 🔘 Custom theming: nemo-anywhere theme search folders at system (prefix) and user level, so themes can be dropped in.
	- 🛠️ Theme + light/dark selection stored in config; auto-follow the Windows light/dark setting with a manual override.
		- ✅ Auto-follow: reads Windows AppsUseLightTheme at startup and live (registry watch), toggles GTK prefer-dark. One icon theme serves both modes.
		- 🔘 Manual override + theme choice persisted in the `.shcl` config (waits on the SHCL config item).

- 🔘 Config engine: move settings + persistence to SHCL (jim-collier/shcl) in a user-level `.shcl` file; decouple from gconf/dconf and the Windows registry. File-assoc overrides and theme/mode selection live here. (Already the intended engine in design.md.)

- 🔘 Ultra-portable Windows: a single self-contained executable.
	- 🔘 No separate library folder - pack the runtime into one `.exe` (in-memory virtual FS, e.g. Enigma Virtual Box).
	- 🔘 One binary only - fold connect-server, open-with, and extensions-list into the main exe on Windows. Linux keeps its separate helpers.
	- 🔘 No shell/Explorer coupling - read file associations from the registry (system defaults only), layered under a nemo-anywhere override map. Overrides launch directly. All nemo config + overrides live in the `.shcl` file, never written to the registry.
	- 🔘 No external plugin loading on Windows (a bad plugin must never hang the app); keep the extension-management UI in-exe.

- 🔘 No autorun, ever, on any platform - not even an option. Notice a new drive; never run anything off it. Remove the autorun-software helper and its media-autorun path.

- 🔘 Native Windows shortcuts: create and edit `.lnk` files, the Windows analog of `.desktop` launchers.

- 🔘 Ship nemo's own bundled icons and data files on Windows.
	- Cause: the data dir is a compile-time absolute Unix path, so the sort-menu icons, eject icon, and emblem art don't resolve on Windows.
	- Probable fix: derive the data dir from the exe location on Windows. Waits on the final release folder layout.

- 🔘 Real-Windows validation pass. Everything so far is verified under wine only.
	- Covers: trash, network browsing, single-instance, default-app setting, the Windows half of the installer, elevated relaunch (UAC prompt), keyboard shortcuts.

### Milestone 5 - More targets

- 🔘 BSD

- 🔘 macOS

### Milestone 6 - CI/CD

- 🛠️ Enable the disabled pipeline stages as the build matures.

- 🔘 Add a C formatter/linter gate and wire it into the format/lint stages.

- 🛠️ Get release binaries onto the host, plus an optimized buildtype, then turn on artifact collection.
	- Done: host dogfood path proven. Release build staged in the container, copied out to a self-contained folder, launched via a small wrapper.
	- 🔘 Wire into the pipeline: optimized-size buildtype, automatic artifact collection.
	- 🔘 Artifacts must come out under the names the installers look for (see design.md, Delivery).

### Milestone 7 - Packaging

- 🔘 Single-exe packaging stage in `cicd-win.ps1` - pack the staged DLL closure into one portable `.exe`.

## Backlog

### Misc to-do

### Bugs

- 🔘 Keyboard shortcuts do nothing in the Windows build when run under wine.
	- Cause: wine has no keyboard layout DLL, so GTK can't turn a keypress into a key value and no shortcut ever matches. Plain keys (arrows, typing) still work, and so do the menus and mouse.
	- Note: a wine limitation, not our code. Expected to work on real Windows - added to the real-Windows validation pass.

### Features and enhancements

- ✅ "Name" column should always be as large as possible, the other columns don't auto-adjust. When window grows or shrinks, the Name column does too to as wide as possible without pushing other columns off.
	- Cause: the Name cell asked for a 40-character width, which acted as a floor the column could never shrink past, so a narrowing window pushed the trailing columns off instead.
	- Fixed: dropped that request, so Name now gives space back down to its existing minimum. Long names ellipsize as before.
	- Verified: at 600px wide all four columns fit where Date Modified used to be cut off; at 1500px Name still takes all the slack; shrinking back from wide re-fits correctly.

- In "find" mode:
	- 🔘 Shrink the "Name" column to fit, and make the 'Location' column adjust as wide as possible as the window resizes. Then go back to the way it was, when exiting "find" mode.
	- 🔘 Instead of showing a filename selected in the status bar, show the entire path.

- 🔘 When a value is longer that the column can display, allow a mouseover tooltip to show the whole value.
	- Using a reusable tooltip mechanism

- 🔘 Change default settings:
	- 🔘 List view, 66% size.
	- 🔘 Ask before moving items to trash.
	- 🔘 Date display in ISO format.
	- 🔘 Showing owner, group, and perms.

- 🔘 Remove features:
	- Option to display date in monospace font.

- 🔘 Allow select and copy of error message dialogs.

- 🔘 Ship with "Copy path(s)" script from current nemo install.
	- Rewrite to be cross-platform friendly.
		- Either a .bash script for Linux/BSD/macOS and .ps1 script for Windows, or build into the program code.

- 🔘 Confirm mouse-movement-based actions that don't already ask for some kind of confirmation. (E.g. drag and drop to a new folder)
	- 🔘 A major enhancement to call out in README, e.g.: "Helps prevent one of the biggest pain points with GUI file managers: Accidental file & folder moves, sometimes without realizing it."

- 🔘 Always operate on whole rows. E.g. if when right-clicking in between columns and not on part of an existing selection, select the entire row before opening right-click menu.

- 🔘 Never show ghost row selection(s). Under some circumstances, there can appear to be two sets of files "selected", but only one set actually is. This is confusing. (Figure out reproducibility steps.)

- 🔘 New flag: `--reset`. Clears bookmarks, resets to default state. (Maybe just delete the config file?)

- 🔘 If the Windows version has never run before, the bookmarks should be cleared, and populated with only the main Windows defaults. (C:\, Desktop, Documents, Downloads, Pictures, Videos, AppData). Also, all linux-specific settings and bookmarks should be cleared on first startup.

- 🔘 Allow '~' in bookmarks to specify home dir (only if at the start and unquoted).
	- 🔘 '~' should work on Windows too.
	- 🔘 Allow environment variables in bookmarks, pathnames, etc.
		- E.g. $HOME on Linux, %USERPROFILE% on Windows.

- 🔘 New process for each window. A crash in one shouldn't affect all others.

- 🔘 Allow moving tabs to other windows.

- 🔘 Option to always show a tab.

- 🔘 Tabs don't take up the whole space, only what's needed for title (and a reasonable minimum width).

### Done

#### Done - Bugs

- ✅ Setting list view to 66% doesn't affect current list view. It should.
	- Also, setting default view to List mode, should affect current view immediately as well.
	- Cause: a folder stored its own view and zoom the first time it was opened, even when that just matched the default, so it was pinned to whatever the default was that day and later changes to the default never reached it. Nothing was watching the default view setting at all.
	- Fixed: a setting that only matches the default is no longer stored, so folders keep following it. Changing a default now also applies to the folders already on screen, and folders you deliberately set to their own view or zoom keep it.

- ✅ Settings don't seem to be persisting.
	- Verified: settings do persist, on both Linux and Windows. Checked the menus, the Settings dialog, per-folder view state, and window size, each set in one run and read back in the next.
	- Cause: the Settings dialog was crashing the whole app at the time this was filed, so nothing set in that session was kept. That crash is fixed.
	- Fixed as well: window size and position were only written when a window was closed cleanly, so a crash - or the wine launcher replacing the running copy - threw them away. They are now saved shortly after a move or resize settles.

- ✅ Windows via Wine: error message at startup. 'The folder contents could not be displayed.', 'Sorry, could not display all the contents of "<username>": Input/output error.' Mouse cursor also stuck at "busy spinner".
	- Reproduced: opening a home folder containing a unix symlink.
	- Cause: one unreadable child failed the whole folder listing. The aborted load also left the busy cursor on.
	- Fixed: the unreadable child is skipped and the rest of the folder lists. The load completes and the cursor clears.

- ✅ Windows via Wine: cursor seems stuck on the "busy" mouse icon.
	- Cause: same as the startup error above. The folder load never finished, so the busy cursor never cleared.

- ✅ Icons don't match OG nemo.
	- Cause: two gaps. Windows reports one generic icon for every file type, and the Windows dependency snapshot was missing its image-loader cache, so no symbolic (SVG) icons rendered.
	- Fixed: per-type icons now derived from the file type on Windows. The loader cache is generated when the snapshot is built.
	- Note: nemo's own bundled PNG icons still don't resolve on Windows. Now its own Milestone 4 item.

- ✅ Windows: dot-name folders don't say "Folder". Regular folders say "Program", not "Folder".
	- Cause: folder type was guessed from the name whenever size read as zero, which every Windows folder does.
	- Fixed: folders always report the folder type, never guessed.

#### Done - Features and enhancements

- ✅ Wine launcher.
	- Fixed: launches detached, so the script exits and returns immediately.
	- Fixed: initial directory is the user's home if it exists, falling back to the drive root, then C:\.

- ✅ Installer script(s) - one-liner install from a shell, for every target.
	- Done: two standalone installers. The bash one covers Linux, BSD, WSL, and macOS; the PowerShell one covers all of those plus Windows.
	- Done: both take channel, target, and architecture options; print the plan and wait for a yes; verify the download checksum before unpacking; replace an existing install in place; and reverse themselves with an uninstall option.
	- Done: installs as a folder plus a menu entry and a name on PATH. User install is the default; only the system-wide install escalates, and says so in the plan.
	- Done: README gained an Installation section. The release-asset naming the installers depend on is in design.md under Delivery.
	- Verified: end to end on the unix side against a stand-in releases service - channel and asset resolution, checksum pass and tamper-fail, install, reinstall, uninstall, prompt accept and decline, and both installers leaving identical results.
	- Note: the Windows half still needs the real-Windows validation pass.

#### Done - Milestones

##### Done; Milestone 6 - CI/CD

- ✅ Dogfood launcher script.
	- Done: keeps date-stamped copies of the latest build in a local pool, prunes aged-out copies not in use, launches the newest with args passed through.
	- Done: one cross-platform PowerShell script for Linux and Windows. Working copy deployed to the common util dir.
	- Done: launches detached and returns immediately. App output goes to a log in the target dir, so it never holds the calling console open.

- ✅ Adopt the local-only delivery model: dev = integration target, main = release-only (dev to main = release cut). Feature branches merge --no-ff into dev.
	- Note: copied as high-level concepts (not language tooling) from the sibling project.

- ✅ Stand up the local pipeline: engine, config, git backup+publish, release helper, and a pre-push merge gate.
	- Verified: container build + smoke test, and backup+publish, all pass.

#### Done; Milestone 4 - Feature port (iterative, per target)

- ✅ dbus / single-instance handling.
	- Verified: single-instance works unchanged on Windows. A second launch hands its arguments to the first. No per-platform gating needed. Details in design.md, "Decisions along the way".
	- Fixed: a bus-less environment (headless or minimal system) crashed the internal file-operations service. It now skips setup cleanly. Regression test added, passes on both platforms.

- ✅ Context-menu actions: open in terminal, open elevated, launchers.
	- Done: on Windows, "open in terminal" opens the native console at the folder, and "open elevated" relaunches the app through the normal elevation prompt. Linux paths unchanged. Menu labels are per-platform.
	- Note: `.desktop` launcher files already degrade cleanly on Windows. Native `.lnk` creation is its own Milestone 4 item.

- ✅ Thumbnails, icon theme, and default-app association per platform.
	- Done: the portable file-and-app layer already carries most of this. The real gaps were the two icon bugs (see Done - Bugs) and packaging the thumbnailer tools with the Windows runtime.
	- Verified: default-app lookup, launch, and set-default work on Windows through the portable layer. Image thumbnails render.
	- Note: on Windows 10/11 the per-user default-app choice may not stick. Not worked around.

- ✅ gvfs replacement or scope-out (mounts, network, trash).
	- Done: gvfs stays an optional runtime dep on Linux; gaps filled natively per platform. Details in design.md, "Decisions along the way".
	- ✅ Portable per-file metadata store on all platforms - view/sort state, custom icons, emblems, favorite markers.
		- Done: one file under the app config dir. Entries follow moves and renames, including folder contents.
		- Verified: per-folder view state now persists on Windows, where before it errored on every write.
	- ✅ Show virtual locations (network, computer) only when the platform supports them.
		- Fixed: the sidebar Network entries are now gated on runtime support. The empty section disappears on Windows until the native backend is present.
	- ✅ Windows trash: native Recycle Bin backend for in-app browse, restore, and empty (deleting to the bin already worked).
		- Done: trash is served in-process from the Recycle Bin, so the existing sidebar row, restore/empty bar, monitor, and delete paths all work unchanged.
		- Note: browsing into a trashed folder's contents shows an error page for now (flat item list only). Minor, revisit if it ever matters.
	- ✅ Windows network: native network-neighborhood browsing + UNC paths in the location bar.
		- Done: the network location is served in-process from native enumeration. Servers list their shares, and a share opens as an ordinary folder.
		- Verified: graceful-empty in the dev rig (no real network there). Populated browsing is part of the real-Windows validation pass.
	- ✅ Accept `\` as a separator in typed locations on all platforms.
		- Done: the literal path is tried first, then a `\`-to-`/` retry only if it doesn't resolve. Real backslash-named files and remote URIs are never touched.

- ✅ File operations (copy/move/delete/rename) on native APIs.
	- Verified: the existing operations engine drives all core operations correctly on Windows - copy, conflict, overwrite, recursive folder copy, move, rename, delete. No porting needed. Probe test added, runs on both platforms.
	- Fixed: link-creation options are hidden on Windows (no symlink support there). The permissions tab, columns, and change-permissions paths are hidden too, since Windows fabricates the mode bits.

- ✅ File monitoring via portable backends.
	- Verified: change events deliver through the native monitor backends on both platforms. Nothing to port.

##### Done; Milestone 3 - First cross-platform target (Windows)

- ✅ Choose and stand up the Windows toolchain.
	- Done: cross-compile from Linux with mingw-w64, smoke-test under wine, in a dedicated container. Details in design.md, "Building (Windows cross)".

- ✅ Get GTK3 + GLib/GIO building on the chosen toolchain.
	- Done: cross configure comes up clean with all deps resolved. Unix-only deps guarded out per platform.

- ✅ Compile on Windows, stubbing/excluding hard platform deps.
	- Done: the app, its helpers, and the extension library all build and link clean, and run under wine. Linux stays green.
	- Done: POSIX gaps closed via a shared compatibility header plus per-site guards.

- ✅ Launch on Windows and browse the local filesystem.
	- Done: the GUI comes up under wine and browses the local drive - sidebar, icon view, per-type icons, item count, free space.
	- Fixed: startup abort caused by desktop settings schemas that only exist on Cinnamon/GNOME. Bundled neutral fallbacks now cover them (see design.md, "Decisions along the way").
	- Done: headless GUI smoke test scripted.

- ✅ Map drive letters / roots into the location model.
	- Done: on Windows, each fixed drive is a first-class sidebar root with a disk-usage bar, replacing the single Unix filesystem root (meaningless on Windows). Removable and network drives keep the normal devices path, which carries eject.
	- Verified: drives show as roots and open to their contents.

##### Done; Milestone 2 - Decouple from Cinnamon (benefits every target)

- ✅ Portable fallbacks for the remaining Mint-flavored theme icon names.
	- Cause: menus and toolbars referenced icon names only Mint themes ship. Pre-existing gap on non-Mint, cosmetic only.
	- Fixed: all names mapped to standard freedesktop names (mostly a straight prefix strip; the non-standard ones got closest equivalents).
	- Verified: every mapped name present in both the Linux and Windows icon themes.

- ✅ Remove desktop management entirely (Nemo Anywhere is a file manager, not a desktop shell).
	- Done: the desktop binary, desktop windows, and the Cinnamon session coupling all deleted. Kept the launcher-file editor and the monitor-geometry helper, both real file-manager features.

- ✅ Isolate xapp / cinnamon-desktop coupling (reimplement portably, not just disable).
	- Done: favorites, thumbnails, tray icon, and the icon chooser all reimplemented portably. Details in design.md, "Decisions along the way".

- ✅ Prove a de-Cinnamon Linux build that runs standalone (no xapp, no cinnamon-desktop) on any desktop or none.
	- Verified: builds and links with neither library. Favorites and thumbnails work on the standalone build.

##### Done; Milestone 1 - Linux baseline

- ✅ Isolate per-file view metadata keys so the two builds don't share view state on the same files.
	- Done: view/layout keys and the favorite markers carry the app name. Keys other file managers also read (custom icon, emblems, annotation, backgrounds) stay shared on purpose.

- ✅ Build upstream as-is on Linux (meson) to confirm a known-good reference.
	- Done: builds and runs clean on stock Debian 13, in a container (this dev box has newer mixed libs).

- ✅ Note the exact dependency set and versions that produce a working build.
	- Done: recorded in the build notes outside the repo.

- ✅ Reorganize into a clean project structure; build consolidated under `source/`, root kept lean.
	- Done: meson project moved under `source/` with its internal layout intact. Builds and runs green.

- ✅ Rebrand to "Nemo Anywhere" / `nemo-anywhere` so it co-installs and runs alongside upstream Nemo without conflict.
	- Done: renamed the installed identity only (binaries, service names, settings schema, config/data dirs, menu entries, icons). Internal code identifiers left as-is; no clash.
	- Done: settings fully isolated from upstream Nemo. Doesn't claim the freedesktop file-manager service when upstream holds it.
	- Verified: staged install has no filename collisions with upstream. Window runs headless.

- ✅ Install nemo-anywhere and upstream Nemo into separate prefixes and confirm both run simultaneously without conflict (real side-by-side runtime proof).

##### Done; Milestone 0 - Initial

- ✅ Clean detached baseline from linuxmint/nemo 6.6.4 (no upstream commit history).

- ✅ Fork branding + provenance (README, fork.md), GPL-2.0-only.

- ✅ Name chosen: nemo-anywhere.

- ✅ Create the GitHub repo and push.
	- Done: created public.

- ✅ Strip upstream CI - keep the repo clear of unrelated automation.
	- Done: workflows and issue templates removed in the fork-setup commit.

## Future and/or deferred

## Canceled
