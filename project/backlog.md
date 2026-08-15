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
	- [Milestone 6 - CI/CD](#milestone-6---cicd)
	- [Milestone 7 - Packaging](#milestone-7---packaging)
- [Backlog](#backlog)
	- [Misc to-do](#misc-to-do)
	- [Bugs](#bugs)
	- [Code review 20260804](#code-review-20260804)
	- [Features and enhancements](#features-and-enhancements)
	- [Done](#done)
		- [Done - Bugs](#done---bugs)
		- [Done - Features and enhancements](#done---features-and-enhancements)
		- [Done - Code reviews](#done---code-reviews)
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

In each section, items are listed approximately from newest to oldest. (Note: if adding/editing frequently, map clipboard or keyboard macro shortcuts to these icons, to go faster.)

| Icon | Status
| :--: | :--
| 🔘   | Not started
| 🛠️   | Started, and/or partially complete
| ✋   | Defer
| ✅   | Complete
| 🚫   | Canceled

## Milestones

### Milestone 3 - First cross-platform target (Windows)

### Milestone 4 - Feature port (iterative, per target)

- 🛠️ Ultra-portable Windows: a single self-contained executable.
	- ✅ No separate library folder - pack the runtime into one `.exe` (in-memory virtual FS, e.g. Enigma Virtual Box).
		- Pack lane added: `cicd/win/pack-portable.ps1` flattens the staged bundle into a prefix layout (exe + dlls at the root, lib/share/etc beside - the same layout the release zip contract expects, and it double-click-runs with no launcher) then packs it with the EVB console into `cicd/artifacts/win-portable/nemo-anywhere.exe`. Wired as cicd-win stage 5.
		- Font-rendering env (freetype v35 interpreter) now set inside the exe on Windows, so no launcher is needed for the native text look.
		- Verified: 167 MB bundle packs to one 38.7 MB exe; version check passes on a bare System32-only PATH and the GUI launches and stays responsive. Hands-on pass still pending.
		- Fixed the leftover console window on launch: the exes were linked with the console subsystem (mingw default), so Windows opened a terminal before the GUI. Main + connect-server + open-with now build with the GUI subsystem; extensions-list stays console on purpose. `--version` output still works when piped, so the cicd smokes are unchanged.
	- ✅ One binary only - Windows now builds just `nemo-anywhere.exe`. The connect-server and open-with dialogs already run in-process (nothing spawned the standalone launchers), and the extensions lister is gone with no plugins to enumerate; the three helper `executable()`s are Unix-only in meson.
		- Extension library folded in too: with no external plugins on Windows, the exe was its only consumer, so it's now a static lib on Windows (still shared on Linux for third-party extensions). No more sibling `libnemo-anywhere-extension-1.dll` - the exe loads and smokes standalone.
	- 🔘 No shell/Explorer coupling - read file associations from the registry (system defaults only), layered under a nemo-anywhere override map. Overrides launch directly. All nemo config + overrides live in the `.shcl` file, never written to the registry. (No longer blocked - the config engine is in.)
	- ✅ No external plugin loading on Windows (a bad plugin must never hang the app); keep the extension-management UI in-exe.
		- `nemo_module_setup` skips the plugin dir on Windows, so a stray DLL can never load and hang the app. The Settings plugins tab still shows, listing nothing ("No extensions found").

- 🛠️ Windows look: make it feel native even though it isn't Explorer.
	- ✅ Fix the thin, poorly anti-aliased text - Segoe UI 9 with full hinting and subpixel (generated settings.ini + fontconfig).
	- 🛠️ Themes: bundle a Windows 11 (Fluent) icon + widget theme, light and dark. Keep a lightweight Linux light/dark pair compiled in (Adwaita / Adwaita-dark). Permissive licenses only - not Microsoft's own art.
		- Done: Fluent widget theme (light + dark) and icon theme bundled, vendored at a pinned commit with licenses kept. Icon set trimmed to file-manager contexts, falls back to Adwaita for the rest.
		- Done: standard icon names materialized as real files (the theme ships them as symlink aliases, which a Windows checkout breaks).
		- 🔘 Linux side: Adwaita / Adwaita-dark pair.
	- 🔘 Custom theming: nemo-anywhere theme search folders at system (prefix) and user level, so themes can be dropped in.
	- 🛠️ Theme + light/dark selection stored in config; auto-follow the Windows light/dark setting with a manual override.
		- ✅ Auto-follow: reads Windows AppsUseLightTheme at startup and live (registry watch), toggles GTK prefer-dark. One icon theme serves both modes.
		- 🔘 Manual override + theme choice persisted in the config file (no longer blocked - the config engine is in).

- ✅ Config engine: settings + persistence moved to SHCL in a user-level `settings.shcl`; gconf/dconf and the Windows registry are out of the picture.
	- Done: GSettings replaced outright rather than kept over a SHCL backend, so no compiled schema is installed or shipped. All 168 settings, ~300 call sites, 84 change handlers and 16 property binds moved over.
	- Done: the file holds only non-default values, carries each key's description as a comment, and is re-read while running so a hand-edit applies immediately.
	- Done: a schema file ships beside the app so `shcl check --schema` validates a hand-edited config (catches typos and bad values).
	- Done: the `compat.*` fallback schemas are gone; desktop-owned settings (terminal, recent files, 12/24h clock) are read from the desktop where it publishes them, ours otherwise.
	- Note: settings do not carry over from a pre-1.0 install - nothing left can read the old store. Fresh defaults on first run after upgrading.
	- Note: nemo actions can still name any GSettings schema in a condition; that reads other programs' settings and is unaffected.

- 🔘 Windows: "Open in terminal" should refer to an ordered list of shells and terminals in settings (if there's not a standard Windows way). At install time - and at launch in a background thread once the UI renders and settles:
	- Check for a hardcoded list of terminals. For each that exist, add them to config. (Add nonexistent ones too, commented out.) For each, prefer to launch in what's installed, in this order of preference: SilkTerm, Windows Terminal, conhost. User can override which terminal is opened, for each shell.
		- Powershell 7
		- WSL2 distros
		- WSL1 distros
		- NuShell
		- PyCmd
		- CMD.exe
		- Powershell 5

- ✅ No autorun, ever, on any platform - not even an option. Notice a new drive; never run anything off it. Remove the autorun-software helper and its media-autorun path.
	- Done: the autorun-software helper, its menu entry, and the "prompt or autorun programs" preference are gone.
	- Done: the inserted-media bar never offers to run software from media. Other media notices (audio CD, photos) unchanged, and automount / auto-open still work - drives are noticed, nothing runs.

- 🛠️ Native Windows shortcuts: create and edit `.lnk` files, the Windows analog of `.desktop` launchers.
	- ✅ Create: "Make Link" and the drag "_Link Here" now write a `.lnk` shell shortcut on Windows (via `IShellLinkW`), in place of the POSIX symlink the win32 file layer can't make. Round-trip verified by a test that loads the shortcut back through the shell.
	- 🔘 Edit: a properties view to see/change a `.lnk`'s target (the analog of the `.desktop` launcher editor).
	- ✅ Follow on open: opening a `.lnk` now follows through to its target - a folder navigates in place, a file opens as if the target were double-clicked. Reading the target round-trips through the shell (test-verified).

- 🔘 Ship nemo's own bundled icons and data files on Windows.
	- Cause: the data dir is a compile-time absolute Unix path, so the sort-menu icons, eject icon, and emblem art don't resolve on Windows.
	- Probable fix: derive the data dir from the exe location on Windows. Waits on the final release folder layout.

- 🔘 Real-Windows validation pass. Everything so far is verified under wine only.
	- Covers: trash, network browsing, single-instance, default-app setting, the Windows half of the installer, elevated relaunch (UAC prompt), keyboard shortcuts.
	- Note: moving a file to the trash raises a Windows confirmation dialog of its own on this box, on top of ours. Worth deciding whether ours should stand down there. The test that hit it now skips that step unless asked for it, since nothing can answer the dialog unattended.

### Milestone 6 - CI/CD

- 🛠️ Enable the disabled pipeline stages as the build matures.

- 🛠️ Get release binaries onto the host, plus an optimized buildtype, then turn on artifact collection.
	- ✅ Done: host dogfood path proven. Release build staged in the container, copied out to a self-contained folder, launched via a small wrapper.
	- ✅ Done: Linux release lane at `cicd/linux/release.bash` - optimized stripped build on an Ubuntu 22.04 box (the glibc floor is what the binary is built against), staged into a relocatable prefix, packed as the tarball plus the sums file.
	- ✅ Done: artifacts come out under the names the installers look for, and the artifact dir is wired in `config.bash` so `utility/release.bash` verifies and attaches them.
	- 🔘 Wire the lane into the pipeline engine itself - its collector still assumes a bare binary and Cargo-shaped versions, so `RELEASE_ENABLE` stays 0.

- 🔘 Linux arm64 release build. Needs an arm64 GTK3 build environment; nothing cross-compiles it today, so the installers' arm64 path has nothing to fetch.

- 🔘 Recorded demo of the app in use, generated by the pipeline and skippable on a quick run.
	- A short video showing the main features, ten to twenty seconds, rendered without a visible display.
	- A smaller looping animation of the same thing for the top of the README, at its own native size.
	- Everything anonymized - no real user name, no distinctive paths.
	- Re-recorded after a noticeable change to the interface or to the demo script.
	- Note: sister projects already have most of the recording machinery to copy from.

### Milestone 7 - Packaging

- ✅ Single-exe packaging stage in `cicd-win.ps1` - pack the staged DLL closure into one portable `.exe`.
	- Done: `cicd/win/pack-portable.ps1` flattens the bundle and packs it with Enigma Virtual Box into one self-contained exe; wired as cicd-win stage 5.

- 🛠️ Windows code signing + AV false-positive reduction.
	- ✅ Embedded VERSIONINFO in the exe (real publisher/version metadata; a blank-metadata binary scores worse with AV heuristics and looks unfinished in Properties).
	- ✅ Local `signtool` signing scaffold in cicd-win stage 5 - env-driven, no-op until a cert is configured (fits a token/store cert: Certum OSS, Azure Trusted Signing, or a commercial EV).
	- 🛠️ SignPath Foundation (free OSS signing) for the released exe: release-only CI at `.github/workflows/release-win.yml` builds + packs + submits to SignPath. First prerelease `v1.0.0-beta1` is cut (unsigned exe attached), so the "already-released" gate is met, and the CI is proven green (build + pack + upload validated). Remaining: the Foundation application and the repo secrets - the signed tag path then runs on its own. Signed publisher shows as "SignPath Foundation". Setup steps in `cicd/win/signing.md`.
	- 🔘 Also sign the release `.zip` contents and, once it exists, the installer (the workflow signs only the single exe today).
	- 🔘 Submit any remaining AV false positives (VirusTotal to find the flagging engines, then vendor FP forms); keep the zip as the FP-free fallback.

- ✅ Publish the Windows `.zip` alongside the single exe. `install.ps1` only ever looks for the contract-named zip, so on Windows the one-liner installer had nothing to fetch even though the release carried a working exe.
	- Done: `cicd/win/pack-zip.bash` builds it from the cross build, and it ships from `v1.0.0-beta2` on.

## Backlog

### Misc to-do

### Bugs

- 🚫 Launching `app\nemo-anywhere.exe` straight from the dogfood folder throws missing-dll dialogs (libcairo-goobject-2 and friends) - the exe has to go through the root `nemo-anywhere.vbs`, which wires the dll path. Punted: the single-exe work above removes the whole launcher/dll-folder arrangement.

- 🔘 The action layout editor never opens: the app spawns it as `nemo-action-layout-editor`, but the binary installs under the app slug as `nemo-anywhere-action-layout-editor`. One missed rename from the rebrand.

- 🔘 Startup logs a dozen pairs of "invalid (NULL) pointer instance" / `g_signal_connect_data` criticals on this host. Harmless so far - the window comes up fine - and not tied to the release build; the day-to-day container build does the same thing here.

- 🛠️ Often when right-clicking on the breadcrumb buttons, the menu closes immediately and has to be right-clicked again.
	- Believed fixed with the path-button menu work (menu now pops synchronously inside the press instead of async after an attribute load); awaiting hands-on confirm.

### Code review 20260804

Full adversarial review of everything written or changed since the fork point. Ordered roughly most serious first. Technical detail is kept out of this file.

- ✅ Code Review 20260804 item 4. "Make Link" on Windows can destroy an existing file, and can crash.
	- Cause: the shortcut is saved over whatever is already there instead of reporting the clash, so the usual "another link to..." renaming never happens.
	- Cause: dropping a link onto a location that is not a real folder returns a failure with no message attached, and reading that message crashes.
	- Fixed: creating a shortcut now refuses to write over anything already at that name and reports the clash, so the existing renaming retry takes over.
	- Fixed: a link dropped somewhere with no real folder behind it now says so instead of failing silently into a crash.
	- Verified: the destruction is reproducible. With the fix backed out, the test overwrites a file it was told not to touch; with it in, the file survives and the clash is reported.
	- Note: the shortcut test was failing two checks before any of this, on a correct product - it compared a short-form temporary path against the long form the system reports. Fixed alongside.

- 🔘 Code Review 20260804 item 5. Repairing the thumbnail cache as an administrator can change ownership of unrelated files.
	- Cause: the repair walks symbolic links instead of skipping them, and changes ownership of whatever they point at.
	- Note: the app itself suggests running this with administrator rights, so an unprivileged process could aim it at system files.

- ✅ Code Review 20260804 item 6. Favorites can hang the app or read freed memory.
	- Cause: listing favorites can stop advancing and spin on one entry forever, leaking as it goes.
	- Cause: the favorites list is rebuilt without locking while background threads are reading it.
	- Cause: entries are stored with a separator that occurs in ordinary file names, so a file with two colons in its name silently repoints somewhere else.
	- Cause: a blank entry, or one whose target no longer exists, crashes rather than being skipped.
	- Cause: the "is this folder inside that one" test has its two sides swapped, and reads one byte past the end of the text.
	- Fixed: the listing always moves on, and an entry that has gone away is left out instead of ending the whole folder.
	- Fixed: the list has a lock, and the two lookups the background threads use hand back copies rather than pointers into it.
	- Fixed: entries are stored the other way round, mimetype first, which cannot be split in the wrong place. Entries in the old order are still read, and rewritten on the next change.
	- Fixed: blank entries are dropped, and a favorite with no mimetype or an unreachable target still lists and draws.
	- Fixed: the inside-that-one test compares the right way round and stops at the end of the text.
	- Verified: new test, every check proven against the old code. The listing spin runs until killed; the concurrent read segfaults; the missing target aborts on a critical; the rest fail their checks.
	- Note: settings written by older versions keep working - only the write order changed, and both are read.

- ✅ Code Review 20260804 item 7. Favorites and thumbnails keep working after the object they belong to is gone.
	- Cause: both release a shared settings object they never owned.
	- Cause: change handlers and a queued callback are left connected at teardown.
	- Fixed: neither releases the shared settings any more - it belongs to the settings store and outlives them both.
	- Fixed: teardown now cancels the queued callback and disconnects the change handlers before anything else goes.
	- Fixed: the favorites file also stopped taking a hold on the settings it never gave back, and three error paths no longer walk away still holding a lock.
	- Verified: with the fixes backed out, the shared settings object really is destroyed while still in use, and a change after teardown lands on a freed object.

- ✅ Code Review 20260804 item 8. A stuck thumbnail helper is never given up on.
	- Cause: there is no time limit on an external thumbnail program, so one hung file permanently costs a worker slot until restart.
	- Cause: a failed reload of the thumbnail helper list reads the entry it just freed.
	- Cause: a very long, very thin image produces no thumbnail and a warning instead of a graceful fallback.
	- Fixed: a helper that has not finished in 30 seconds is stopped, logged and moved on from, so the slot comes back. Thumbnailing on a one-thread machine no longer ends for the session.
	- Fixed: the reload walk stops at the entry it removed instead of stepping off it.
	- Fixed: a thumbnail is never asked for at zero pixels wide or tall, so a 5000x1 image thumbnails instead of failing.
	- Verified: new test. With the fixes backed out the hung helper is still blocking after 75 seconds and the thin image produces nothing. The freed-entry read is fixed by inspection - it is invisible at runtime - with the test covering the path it happens on.

- ✅ Code Review 20260804 item 9. Emptying the Windows trash fails whenever it holds a folder.
	- Cause: trashed folders are reported as folders but refuse to list their contents, and the delete path needs to list them.
	- Note: this affects both "Empty Trash" and permanently deleting a single item.
	- Fixed: a trashed folder now goes with everything inside it, so emptying the trash gets through a bin holding folders.
	- Fixed: a trashed folder lists its contents. Permanently deleting one counts what is in it first, and that count used to fail before the delete even started - a second, separate stopping point.
	- Fixed: with that, a trashed folder can be opened and browsed rather than showing an error page. Its contents carry no original location or deletion date of their own, which is correct - only the folder was trashed.
	- Verified: new case in the trash test that recycles a folder of its own for real, so it runs on Windows rather than only under wine. Pre-fix, listing says "not a directory" and the delete says "directory not empty".
	- Note: a link or junction inside a trashed folder is deleted as the link it is, never followed out of the bin.

- 🔘 Code Review 20260804 item 10. Windows trash items can go missing, and restore can aim at the wrong place.
	- Cause: items the shell describes in a form the code does not expect are skipped silently, while the item count still includes them.
	- Cause: a long original location is cut short, and the shortened path is what a restore would use.
	- Note: only reproducible on real Windows. Belongs with the real-Windows validation pass.

- 🔘 Code Review 20260804 item 11. The Windows trash monitor can freeze the app.
	- Cause: it announces changes while still holding its own lock, so a listener that closes or opens a trash view deadlocks.

- 🔘 Code Review 20260804 item 12. Windows network browsing builds wrong addresses and cannot report a failure.
	- Cause: a share's address is joined to its server without a separator, so shares get malformed addresses and two servers can collide.
	- Cause: no network, or access denied, looks exactly like an empty network - no message either way.
	- Cause: any typed network address is presented as a valid empty folder rather than "not found".
	- Cause: nothing limits how deep the enumeration recurses.

- 🔘 Code Review 20260804 item 13. Windows context-menu actions break on ordinary paths.
	- Cause: "Open as Administrator" passes the folder unquoted, so anything with a space arrives as two separate locations.
	- Cause: "Open in Terminal" at a drive root passes a trailing backslash that swallows the closing quote.

- 🔘 Code Review 20260804 item 14. Opening a Windows shortcut can truncate its target or hang the app.
	- Cause: targets past the old length limit are silently cut short and then opened, wrongly.
	- Cause: a shortcut pointing at itself, or at a loop of shortcuts, recurses until the app runs out of stack.

- 🔘 Code Review 20260804 item 15. A duplicated line in the settings file empties a list instead of falling back.
	- Cause: an unreadable list is treated as a deliberately empty one. Only lists behave this way; single values fall back correctly.
	- Note: a duplicated column list opens the list view with no columns at all. Hand-editing is a supported way to use this file, so this is easy to hit.

- 🔘 Code Review 20260804 item 16. An external edit arriving mid-change throws the change away.
	- Cause: settings are written a couple of seconds after they are changed, and a file reload in that window replaces the pending change with no warning.

- 🔘 Code Review 20260804 item 17. Settings changes can be announced from a background thread.
	- Cause: deleting files updates favorites from a worker thread, and the change is announced on that same thread.
	- Note: the previous settings system always announced on the main thread, which is what every listener assumes. Nothing fires today, so this is a trap for the next listener added.

- 🔘 Code Review 20260804 item 18. A damaged per-folder settings file is discarded without a word, then overwritten.
	- Cause: a parse failure leaves an empty store, and the next change writes that empty store over the file.
	- Note: costs every folder's saved view, zoom, sort and layout. A failed save is likewise ignored.

- 🔘 Code Review 20260804 item 19. Setting the thumbnail size limit above two gigabytes breaks thumbnails.
	- Cause: the limit is stored in a smaller number than the dialog offers, so the large choices wrap. Eight gigabytes turns every thumbnail off; two and four turn the limit off entirely.

- 🔘 Code Review 20260804 item 20. Opening a folder on an unresponsive drive freezes the whole window.
	- Cause: the fallback added for unreadable folders asks for the listing in a way that blocks until the system gives up.
	- Cause: it also treats any general failure as that same case, so a passing glitch is remembered as a made-up folder with no way to tell.
	- Cause: a folder with many unreadable entries stops partway and shows an error over a half-listed folder.

- 🔘 Code Review 20260804 item 21. Right-clicking a path segment can offer actions the folder will not allow.
	- Cause: the menu is now built before the folder's details have loaded, and the unknown state reads as "everything is permitted", so Delete and New Folder appear on read-only places.

- 🔘 Code Review 20260804 item 22. Changing the default zoom discards a zoom deliberately set in another tab.
	- Cause: every open view reacts, not just the visible one, so background tabs lose their own setting.

- 🔘 Code Review 20260804 item 23. The "treat root as a normal user" preference is read before settings are open.
	- Cause: it is consulted while handling the command line, which happens first, so it is answered wrongly and then remembered.

- 🔘 Code Review 20260804 item 24. Folder listing and file moves do more per-file work than they used to.
	- Cause: every file now builds an address and takes a shared lock to check the per-folder store, where before there was a cheap early exit.
	- Cause: every moved file scans the whole store, so a large move gets slower the more is stored.
	- Cause: on Windows the per-type icon is rebuilt for every file on every update, not just when the type changes.
	- Cause: the store is rewritten whole on every save and never pruned.

- 🔘 Code Review 20260804 item 25. Reading a setting costs more than it should, and text settings grow memory.
	- Cause: every read searches the whole settings table from the start.
	- Cause: reads of text, list and choice settings allocate inside the settings document and never give it back, and one of them runs on every icon the mouse passes over.

- 🔘 Code Review 20260804 item 26. The Windows recycle bin is rescanned far more than needed.
	- Cause: a full scan runs every few seconds for the life of the app, twice more on every look at the trash folder, and once more for every item not already known.

- ✅ Code Review 20260804 item 27. The release checksums file can be written wrong.
	- Cause: an empty release folder still writes a bogus line, and any artifact name with a space would be split in two.
	- Note: this is the file both installers verify a download against.
	- Fixed: null-separated, and it no longer runs the checksum tool at all when there is nothing to check.

- 🛠️ Code Review 20260804 item 28. Assorted unsafe or non-portable paths in the pipeline and installer scripts.
	- Fixed: the Windows installer no longer moves the new copy into place in a way that fails across drives after the old one is already gone.
	- Fixed: the installer's own `--help` now prints when run the documented way.
	- Fixed: a prerelease version in an archive name is no longer reported as the plain release number.
	- Fixed: the release archive no longer fails outside a checkout over its timestamp.
	- Remaining below.
	- Cause: refreshing the bundled themes rewrites a notes file that now also records the vendored settings parser and its license.
	- Cause: the Windows installer removes the old copy and then moves the new one into place, which fails outright across drives - leaving nothing installed.
	- Cause: the Windows pipeline's publish step merges instead of fast-forwarding, against the rule the rest of the project follows.
	- Cause: the publish step does not stash untracked files, so a pull can abort after the backup has already run.
	- Cause: a failed image-loader cache build leaves an empty cache, which is worse than none at all.
	- Cause: the Windows pipeline can sail past its own message prompt when nothing is typing, and get stuck later.
	- Cause: the launcher written into the Linux package hardcodes the Intel library folder, so the planned arm64 build would ship without its extension library.
	- Cause: the publish helper runs an environment variable as script.
	- Cause: two delete-and-replace paths have no guard on where they are pointing.
	- Cause: the installer's own `--help` prints nothing when run the documented way.

- 🛠️ Code Review 20260804 item 29. Script style and speed debt.
	- Cause: several loops start external programs once per item where a builtin would do, the worst being the backup rotation and the dogfood pruning.
	- Cause: one unused function would fail immediately if anything ever called it.
	- Fixed: the output helpers now live in one file that the helper scripts share, instead of each carrying its own lesser copy.
	- Fixed: the Windows installer gained proper built-in help, so `Get-Help` and `-?` work.
	- Note: the review said three scripts had diverged output helpers; only one actually had. The others define a single matching helper, which is fine.

- 🔘 Code Review 20260804 item 30. A Windows-only test reports a pass when it did not run.
	- Cause: the trash test exits successfully unless it detects the compatibility layer used for development, so on real Windows it silently skips.
	- Note: that is exactly where items 9 and 10 would have been caught.

### Features and enhancements

- ✅ Dimmer highlight of mouseover line. It can easily get confused with line selection.
	- App CSS dims file-pane/tree row :hover to 0.035 alpha (theme was 0.08), scoped `:not(:selected)`; confirmed by eyeball.

- 🛠️ Right-click from - and drag-n-drop to - a path button, should behave as if it were acting on a folder.
	- DnD-to already worked (drop-target proxy on each button's folder); confirmed fine.
	- Right-click menu was the trimmed `location` menu. Added Open, Open in Terminal, Open as Admin, and New Folder (create inside) as `Location*` variants so a segment acts like a folder. Deferred the heavy selection/extension submenus (Open With, Copy/Move To, Rename, Duplicate, Create Link, Scripts, Actions) - tightly coupled to the live selection and odd on an ancestor dir.
	- New Folder only enabled when the segment is the currently displayed folder (else grayed) - it lands inside that folder.
	- Fixed adjacent bug: right-click often flashed the menu shut (had to click twice). The location menu popped up async after a file-attribute load, firing post-release with a stale event; now it pops synchronously inside the press and just warm-loads mount/fs info for the volume items.

- 🔘 Path button bar should immediately return to buttons, any time the path defocuses, not just 'esc' hit.

- 🔘 Hit 'Esc' when focus is in the folder/file pane to completely remove selection. (E.g. to use menu key on background.) Esc again to return it to where it was.

- In "find" mode:
	- 🔘 Shrink the "Name" column to fit, and make the 'Location' column adjust as wide as possible as the window resizes. Then go back to the way it was, when exiting "find" mode.
	- 🔘 Instead of showing a filename selected in the status bar, show the entire path.

- 🔘 When a value is longer that the column can display, allow a mouseover tooltip to show the whole value.
	- Using a reusable tooltip mechanism

- 🔘 Ship with "Copy path(s)" script from current nemo install.
	- Rewrite to be cross-platform friendly.
		- Either a .bash script for Linux/BSD/macOS and .ps1 script for Windows, or build into the program code.

- 🔘 Confirm mouse-movement-based actions that don't already ask for some kind of confirmation. (E.g. drag and drop to a new folder)
	- 🔘 A major enhancement to call out in README, e.g.: "Helps prevent one of the biggest pain points with GUI file managers: Accidental file & folder moves, sometimes without realizing it."

- 🔘 Always operate on whole rows. E.g. if when right-clicking in between columns and not on part of an existing selection, select the entire row before opening right-click menu.

- 🔘 Windows-specific:
	Hide/show hidden files should consider both:
		- Linux-style dot-files
		- Native Windows "hidden" attribute
	Windows & NTFS: Any dir symlink through any mechanism should also allow junction (ordered higher in preference than symlink).

- 🔘 New flag: `--reset`. Clears bookmarks, resets to default state. (Maybe just delete the config file?)

- 🔘 If the Windows version has never run before, the bookmarks should be cleared, and populated with only the main Windows defaults. (C:\, Desktop, Documents, Downloads, Pictures, Videos, AppData). Also, all linux-specific settings and bookmarks should be cleared on first startup.

- 🔘 Allow '~' in bookmarks to specify home dir (only if at the start and unquoted).
	- 🔘 '~' should work on Windows too.
	- 🔘 Allow environment variables in bookmarks, pathnames, etc.
		- E.g. $HOME on Linux, %USERPROFILE% on Windows.

- 🔘 New process for each window. A crash in one shouldn't affect all others.

- 🔘 Allow moving tabs to other windows.

- 🔘 Option to always show a tab.

- 🔘 Tabs shouldn't take up the whole space, only what's needed for title (and a reasonable minimum width).

- 🔘 Target: BSD

- 🔘 Target: macOS

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

- ✅ Don't continuously spam stdout/stderr with meaningless debug messages.
	- Cause: on Windows, any file type without a registry MIME mapping fell through a wildcard and got a doomed image-thumbnail attempt - two warnings per file, every folder browsed. A few one-shot startup notices added to the noise.
	- Fixed: unknown types are no longer treated as thumbnailable, the image loader gets a real MIME type, and the per-file / startup notices are debug-level now (visible with G_MESSAGES_DEBUG when wanted).
	- Verified: browsing a mixed folder of images and non-images runs silent; image thumbnails unaffected.

- ✅ Add a C formatter/linter gate and wire it into the format/lint stages.
	- Done: check-only cppcheck over the changed C files only, wired into both pipelines (Windows stage 1 + gate, Linux lint stage). No in-place formatter - a full-tree reformat of the inherited code would bury history in churn.
	- Done: a box without cppcheck skips with a warning instead of blocking a push.

- ✅ Change default settings:
	- ✅ List view, 66% size.
	- ✅ Ask before moving items to trash.
	- ✅ Date display in ISO format.
	- ✅ Showing owner, group, and perms.
	- Done: new out-of-the-box defaults - list view at 66%, trash moves ask first, ISO dates, owner/group/permissions columns visible. Existing installs that changed a setting keep their value.

- ✅ Remove features:
	- Option to display date in monospace font.
	- Done: the date font style option, its setting, and the mono-font matching are gone. Dates use the regular font.

- ✅ Allow select and copy of error message dialogs.
	- Done: the message text in the stock error/question dialogs is selectable, so it can be copied. The expandable details text already was.

- ✅ "Name" column should always be as large as possible, the other columns don't auto-adjust. When window grows or shrinks, the Name column does too to as wide as possible without pushing other columns off.
	- Cause: the Name cell asked for a 40-character width, which acted as a floor the column could never shrink past, so a narrowing window pushed the trailing columns off instead.
	- Fixed: dropped that request, so Name now gives space back down to its existing minimum. Long names ellipsize as before.
	- Verified: at 600px wide all four columns fit where Date Modified used to be cut off; at 1500px Name still takes all the slack; shrinking back from wide re-fits correctly.

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

#### Done - Code reviews

- ✅ Code Review 20260804 item 1. Every dropdown and radio choice in Settings saved the wrong value.
	- Cause: the settings layer stored the choice by number, but the dialog only ever supplied the name, leaving the number at zero. Whatever was picked, the first option was saved.
	- Note: worst case was "Executable text files", where the first option is "run it" - so any visit to that setting quietly armed scripts to run on double click.
	- Fixed: choices are now saved by name. Regression test added, and confirmed to fail before the fix.

- ✅ Code Review 20260804 item 2. The settings file grew a duplicate comment line on every write.
	- Cause: setting a comment appends a line rather than replacing one, and the comment was re-applied on every save.
	- Note: the window size is saved shortly after every move or resize, so a session of dragging the window added dozens of identical lines, and they survived restarts.
	- Fixed: the comment is written only when a setting first appears in the file.

- ✅ Code Review 20260804 item 3. Hand-editing a setting that was already in the file did nothing until restart.
	- Cause: the live-reload comparison could only see a setting appear or disappear, never change, so nothing was announced to the app.
	- Fixed: the comparison now reads the values themselves.

#### Done - Milestones

##### Done; Milestone 6 - CI/CD

- ✅ Dogfood launcher script.
	- Done: keeps date-stamped copies of the latest build in a local pool, prunes aged-out copies not in use, launches the newest with args passed through.
	- Done: one cross-platform PowerShell script for Linux and Windows. Working copy deployed to the common util dir.
	- Done: launches detached and returns immediately. App output goes to a log in the target dir, so it never holds the calling console open.
	- Done: a source on a network share is written off after a moment rather than blocking the launch while the network gives up in its own time.
	- Done: a launch with nothing to copy went from nine seconds to one. Working out which programs are running was the whole cost on Windows, and it was being done twice.
	- Fixed: the newest copy could age out and be re-fetched on every run whenever the source build was itself older than the pruning cutoff.
	- Fixed: copies left by the pre-single-exe layout were invisible to the pruning and sat there for good.

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
		- Done: browsing into a trashed folder works. It was a flat item list at first; the code review turned up that the same gap also stopped a trashed folder being permanently deleted, and both were fixed together.
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

- ✋ Make the CICD test gate resilient to a down or absent docker daemon.
	- Done: build and smoke steps go through a wrapper that probes the daemon first.
	- Done: an environmental miss (docker absent, daemon down, container gone) skips with a warning instead of blocking the push. A real build or test failure still gates. A strict mode turns a miss back into a hard failure.
	- Note: the daemon needs root to start, so the unattended hook never auto-starts it. The skip message shows the manual command.
	- Verified: gate passes normally, and skips cleanly when docker is unreachable.
	- ✋ Revisit whether one container-Linux smoke test is a meaningful gate once Windows/cross lanes exist.

## Canceled

- 🚫 Keyboard shortcuts do nothing in the Windows build when run under wine.
	- Cause: wine has no keyboard layout DLL, so GTK can't turn a keypress into a key value and no shortcut ever matches. Plain keys (arrows, typing) still work, and so do the menus and mouse.
	- Note: a wine limitation, not our code. Expected to work on real Windows - added to the real-Windows validation pass.
