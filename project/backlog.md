<!-- markdownlint-disable MD007 -- Unordered list indentation -->
<!-- markdownlint-disable MD010 -- No hard tabs -->
<!-- markdownlint-disable MD033 -- No inline html -->
<!-- markdownlint-disable MD055 -- Table pipe style [Expected: leading_and_trailing; Actual: leading_only; Missing trailing pipe] -->
<!-- markdownlint-disable MD041 -- First line in a file should be a top-level heading -->
# Requirements

This is a product backlog just for pre-v1.0.0 release. After that, bugs, features, and enhancements will be managed in Github Issues.

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
	- [Code review 20260815](#code-review-20260815)
		- [High](#high)
		- [Medium](#medium)
		- [Low](#low)
		- [Architecture and UX notes](#architecture-and-ux-notes)
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
	- ✅ Themes: bundle a curated set of icon and widget styles, light and dark. Permissive licenses only - not Microsoft's own art.
		- Done: eight widget themes (Windows 11, 10, 7, XP light+dark, macOS light+dark) and nine icon styles, about 5 MB all told. Each vendored at a pinned commit with its license kept.
		- Done: icon themes trimmed to the ~180 names a file manager asks for, which took Fluent from 1.8 MB to 261 KB; the rest falls back to Adwaita.
		- Done: standard icon names materialized as real files (themes ship them as symlink aliases, which a Windows checkout breaks).
		- Done: Windows XP and Windows 7 icon sets drawn in-house - no cleanly-licensed set of either exists, only repackaged Microsoft art.
		- Bundled where the platform is unlikely to have themes installed (Windows, macOS). Linux keeps using the desktop's own, so the thin prefix stays thin.
	- ✅ Custom theming: theme search folders beside the settings file, so themes can be dropped in on any platform. Drop-ins are searched before the bundled set.
	- ✅ Theme + light/dark selection stored in config; auto-follow the Windows light/dark setting with a manual override.
		- Done: auto-follow reads Windows AppsUseLightTheme at startup and live (registry watch).
		- Done: an Appearance page in settings with Light / Dark / Follow the system, plus style and icon pickers filtered to the mode in force. Picking one half of a light/dark pair follows the pair when the mode changes.

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

- 🛠️ Real-Windows validation pass. Everything so far is verified under wine only.
	- Covers: trash, network browsing, single-instance, default-app setting, the Windows half of the installer, elevated relaunch (UAC prompt), keyboard shortcuts.
	- Note: moving a file to the trash raises a Windows confirmation dialog of its own on this box, on top of ours. Worth deciding whether ours should stand down there. The test that hit it now skips that step unless asked for it, since nothing can answer the dialog unattended.
	- Done on real Windows: the recycle bin end to end, network browsing against this box's own shares, single instance and location forwarding, the installer's install/reinstall/uninstall round trip, and elevated relaunch. Each of the code-review items below was re-checked here, and the fix removed to watch the check fail first.
	- Found doing it, and fixed: the whole compiled-resource bundle was missing from the Windows build, so there was no menu bar at all; a drive root was named three different ways; "Set as default" failed silently forever; the installer read a prerelease version as the release it precedes.
	- Still not exercised here: the signing path, which only runs in the hosted release workflow on a tag. Confirmed that the repo has no secrets and no variables set at all, so the SignPath step is skipped and a release cut today would publish an unsigned exe - the documented fallback, working as intended, but worth knowing before announcing a build. The UAC consent prompt itself was not seen either - this box elevates without prompting and the session is already elevated - so what is proven is that the relaunch starts an elevated copy at the right folder, not the consent dialog.

### Milestone 6 - CI/CD

- 🛠️ Enable the disabled pipeline stages as the build matures.

- 🛠️ Get release binaries onto the host, plus an optimized buildtype, then turn on artifact collection.
	- ✅ Done: host dogfood path proven. Release build staged in the container, copied out to a self-contained folder, launched via a small wrapper.
	- ✅ Done: Linux release lane at `cicd/linux/release.bash` - optimized stripped build on an Ubuntu 22.04 box (the glibc floor is what the binary is built against), staged into a relocatable prefix, packed as the tarball plus the sums file.
	- ✅ Done: artifacts come out under the names the installers look for, and the artifact dir is wired in `config.bash` so `utility/release.bash` verifies and attaches them.
	- 🔘 Wire the lane into the pipeline engine itself - its collector still assumes a bare binary and Cargo-shaped versions, so `RELEASE_ENABLE` stays 0.

- 🔘 Linux arm64 release build. Needs an arm64 GTK3 build environment; nothing cross-compiles it today, so the installers' arm64 path has nothing to fetch.

- 🔘 Recorded demo of the app in use, generated by the pipeline and skippable on a quick run.
	- A short video showing the main features, ~twenty seconds, rendered without a visible display.
	- A looping animation of the same thing for the top of the README.
	- Everything anonymized - no real user name, no distinctive paths.
	- Re-recorded after a noticeable change to the interface or to the demo script.
	- Note: sister projects already have most of the recording machinery to copy from.
	- Follow the 'Automated demo of program use' notes kept with the shared project directives.

### Milestone 7 - Packaging

- ✅ Single-exe packaging stage in `cicd-win.ps1` - pack the staged DLL closure into one portable `.exe`.
	- Done: `cicd/win/pack-portable.ps1` flattens the bundle and packs it with Enigma Virtual Box into one self-contained exe; wired as cicd-win stage 5.

- ✋ Windows code signing + AV false-positive reduction. Deferred: the SignPath Foundation application was refused, so releases ship an unsigned exe with the `.zip` as the fallback.
	- ✅ Embedded VERSIONINFO in the exe (real publisher/version metadata; a blank-metadata binary scores worse with AV heuristics and looks unfinished in Properties).
	- ✅ Local `signtool` signing scaffold in cicd-win stage 5 - env-driven, no-op until a cert is configured (fits a token/store cert: Certum OSS, Azure Trusted Signing, or a commercial EV).
	- ✋ SignPath Foundation (free OSS signing) for the released exe: applied for and refused. The release-only CI at `.github/workflows/release-win.yml` still builds, packs and publishes; its submission step is left dormant behind the token gate, so nothing needs unpicking if this is revisited. Consequence worth remembering: that workflow existed because SignPath would only sign CI-built artifacts, so with it gone nothing forces a release into hosted CI and a local cut is viable again. Options weighed (Azure Artifact Signing, Certum open source, commercial cloud, reapplying) are in `cicd/win/signing.md`.
	- ✋ Also sign the release `.zip` contents and, once it exists, the installer. Blocked on there being any signing identity at all.
	- 🔘 Submit any remaining AV false positives (VirusTotal to find the flagging engines, then vendor FP forms); keep the zip as the FP-free fallback.

- ✅ Publish the Windows `.zip` alongside the single exe. `install.ps1` only ever looks for the contract-named zip, so on Windows the one-liner installer had nothing to fetch even though the release carried a working exe.
	- Done: `cicd/win/pack-zip.bash` builds it from the cross build, and it ships from `v1.0.0-beta2` on.

## Backlog

### Misc to-do

- ✅ Depend on Explorer as little as possible.
	- Audited every place the Windows build reaches into the shell. The only one that handed work to Explorer was a "show this file in the file manager" call, which asked Windows for the default handler for a folder - Explorer, by definition.
	- It was already unreachable: the only caller sits behind a desktop-view check that went permanently false when the desktop shell was removed. On Windows it would also have been asking for a handler that Windows does not answer for - nothing is registered for a folder as a type.
	- Removed, along with its declaration. Nothing in the tree launches Explorer now.
	- What remains is in-process and unavoidable: the recycle bin and `.lnk` files are shell APIs called inside our own process, with no Explorer involved. Two `ShellExecute` calls stay for good reasons - one launches the terminal the user chose (found on PATH, not via the shell's associations), the other relaunches our own executable elevated, which is the only way to ask for elevation.

### Bugs

- 🔘 Listing a folder whose path is past 260 characters quietly lists a different folder instead - whichever one the program happens to be running from.
	- Found while proving the long-path work above. The toolkit's own directory walk is what breaks; every other call on the same path is right, which is why nothing showed up until a folder that deep was actually opened.
	- In the window it reads as an empty folder, because each name it hands back is then checked against the folder that was asked for and none of them are in it. That is the harmless case. The one to worry about is search, which walks folders itself and would follow the wrong tree.
	- Reproduced three ways: the failing call from two different working directories returns the contents of each in turn, while the platform's own call on the same path returns the right thing.
	- Not ours to fix in place. Either the walk is done ourselves on Windows, or it goes upstream - nothing was found already filed for it.

- 🔘 The settings schema shipped for `shcl check` is kept in step with the key table in the code by hand, and nothing notices when it drifts.
	- Two files have to be edited for every new setting. Miss the second and a hand-edited config validates against a schema that does not know the key.
	- Noticed adding two settings at once. Wants a check that walks both and fails on a mismatch.

- ✅ In dark mode the breadcrumb bar and the checked view buttons kept a light background, unreadable against everything around them.
	- Cause: a bundled theme is loaded as a stylesheet of our own, but the theme *name* was left pointing at it. GTK cannot resolve a name it has never seen on disk, falls back to its packaged sheet, and drops the dark half while doing so - so the layer under ours was the light one. Anything our sheet did not itself paint showed it through.
	- Fixed: the name now points at a theme GTK really has, so the base follows light/dark while our sheet sits on top. Confirmed by eye, and by reproducing it the other way first.
	- Also fixed alongside: choosing a theme that cannot be found left the previous one on screen, so a bad name looked like nothing had happened.

- ✅ The three view buttons at the bottom left drew as broken-image placeholders.
	- Cause: none of the app's own artwork was in the Windows bundle at all. Only the toolkit's icons were packaged, so every one of our own icon names missed - the location button in the toolbar was the same failure.
	- Fixed: the app's artwork now rides inside the executable, the same way the bundled themes do. Costs no extra files, so nothing is added to startup time, and it works on every platform including a relocated install.

- ✅ The theme picker offered "macOS" and "Windows 10" twice in dark mode, and one of each was the light theme.
	- Cause: those two themes ship a dark sheet of their own upstream *and* have a separately drawn dark half that we also bundle, so both halves claimed dark.
	- Fixed: where a light/dark pair is named, the pair wins and the redundant sheet is dropped. A theme that states which modes it suits is no longer second-guessed either, so a hand-dropped theme cannot bring the fault back.

- ✅ On Windows a drive root is named `\` everywhere except the sidebar - the window title reads `\` and the breadcrumb reads `(C:) Windows` while the sidebar has `Windows (C:)`. Seen on this box browsing `C:\`.
	- The volume-label work only ever covered the sidebar, and it built its own name there. Everywhere else falls back to what Windows reports for a drive root, which is a bare separator.
	- Three different sources were in play: the basename, which is `\` for every drive alike; the volume monitor, which says `(C:) Windows`; and the sidebar's own string.
	- Fixed: a drive root is `C:\` everywhere - title, breadcrumb and sidebar all ask the same helper. The volume label moved to the sidebar tooltip, where it cannot be mistaken for the path.
	- Verified on Windows: a new test covers the naming, including that the first folder inside a drive keeps its own name; the three surfaces were then checked by eye. Pre-fix the checks fail.

- ✅ "Set as default" in the Open With tab did nothing on Windows, and said nothing either.
	- Cause: Windows keeps the per-user default behind a hash it will not let a program write, so the call fails outright - and the result was thrown away along with the error.
	- Fixed: the failure is reported. The choice still cannot be made on Windows; the difference is the user is told rather than left thinking it worked.
	- Verified on Windows: the underlying call refuses with "Setting default applications not supported yet". Looking a default up still works, but only by extension - asking by mime type answers nothing.

- 🔘 A leftover helper from the install folder blocks uninstall and in-place upgrade, and the message blames the app.
	- The session bus the app autolaunches lives in the install folder and outlives the window, so the in-use check still sees the folder busy. It says "Nemo Anywhere is still running", which reads as wrong to someone who just closed it.
	- Seen doing the installer round trip: uninstall failed, then succeeded a few seconds later with nothing else changed.
	- Wants either a wait-and-retry, or a message that names what is actually holding the folder.

- 🔘 The installer leaves the user PATH very slightly different from how it found it.
	- Adding then removing the entry also drops a pre-existing trailing separator, so an install/uninstall round trip is not byte-identical. Harmless - an empty trailing entry means nothing - but it is a change nobody asked for.

- 🛠️ The Windows executable carries no application manifest, so it is not marked long-path aware. With long paths switched on in Windows - as they are on this box - anything past the old 260-character limit is still out of reach for us while Explorer handles it fine.
	- The manifest landed with the DPI work under Features and enhancements, and most of this went with it. Measured on a 427-character folder holding a 462-character file: without the manifest every call failed outright; with it, reading the file, asking for its details, testing that it exists and walking into the folder all work.
	- What is left is listing a folder, and it is worse than a failure - see the bug below. Left open until that is answered.

- 🚫 Launching `app\nemo-anywhere.exe` straight from the dogfood folder throws missing-dll dialogs (libcairo-goobject-2 and friends) - the exe has to go through the root `nemo-anywhere.vbs`, which wires the dll path. Punted: the single-exe work above removes the whole launcher/dll-folder arrangement.

- ✅ The action layout editor never opens: the app spawns it as `nemo-action-layout-editor`, but the binary installs under the app slug as `nemo-anywhere-action-layout-editor`. One missed rename from the rebrand.
	- Fixed: it is spawned under the app slug, out of the folder the app itself was started from, and a failure to start now says so instead of doing nothing.
	- Also found and fixed alongside: the Restart button in extension settings was quitting and starting whichever upstream Nemo happened to be installed, not this app.

- ✅ The Windows build shipped without its compiled-in resources, so it had no menu bar at all and every `.ui`, `.glade` and `.css` lookup failed.
	- Cause: the resource bundle is attached to the extension library. On Linux that is a shared library and the whole thing loads, so the resources register themselves. On Windows it is a static one, and the linker keeps only the members that resolve a symbol - the resources register from a constructor nothing calls by name, so the object was dropped.
	- Nobody noticed because the app still starts and browses: the missing menu bar reads as a design choice, and the fallout was a wall of criticals that had been written off as noise.
	- Fixed: on Windows the resources go straight into the executable. Linux keeps them in the shared library as before.
	- Verified on Windows: the menu bar is back, and startup criticals went from 40 to 9 - none of the remainder about resources or widgets.

- 🛠️ Startup logs a dozen pairs of "invalid (NULL) pointer instance" / `g_signal_connect_data` criticals on this host. Harmless so far - the window comes up fine - and not tied to the release build; the day-to-day container build does the same thing here.
	- The Windows half of this was the missing resource bundle above, and is gone. Whether the host case has the same cause is untested - it was investigated on Linux, where the resources were never dropped.
	- What is left on Windows is a different signature: nine `g_file_get_child: assertion 'name != NULL'` at startup. Not looked into.
	- Not reproducible in the build container. Tried, with none of it producing a single critical: with and without a session bus, with and without the desktop's own settings present (the container has the full cinnamon schema set already), with a home full of bookmarks including missing and remote ones, bare launch and with a location, with and without the desktop flag.
	- So it depends on something only the real session has. Needs one capture from the host to place it; the exact command is in the private notes.

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

- ✅ Code Review 20260804 item 5. Repairing the thumbnail cache as an administrator can change ownership of unrelated files.
	- Cause: the repair walks symbolic links instead of skipping them, and changes ownership of whatever they point at.
	- Note: the app itself suggests running this with administrator rights, so an unprivileged process could aim it at system files.
	- Fixed: the repair acts on the link itself instead of following it, so a link planted in the cache can no longer hand away the file it points at.

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

- ✅ Code Review 20260804 item 10. Windows trash items can go missing, and restore can aim at the wrong place.
	- Cause: items the shell describes in a form the code does not expect are skipped silently, while the item count still includes them.
	- Cause: a long original location is cut short, and the shortened path is what a restore would use.
	- Note: only reproducible on real Windows. Belongs with the real-Windows validation pass.
	- Fixed: an item the shell describes in an unexpected form is now reported rather than silently dropped, and the original location is read at full length so a restore aims at the right place.
	- Note: written and cross-built here, exercised only under wine. Belongs to the real-Windows validation pass.

- ✅ Code Review 20260804 item 11. The Windows trash monitor can freeze the app.
	- Cause: it announces changes while still holding its own lock, so a listener that closes or opens a trash view deadlocks.
	- Fixed: the announcement is made after the lock is released, so a listener that opens or closes a trash view cannot deadlock it.
	- Note: written and cross-built here, exercised only under wine. Belongs to the real-Windows validation pass.

- ✅ Code Review 20260804 item 12. Windows network browsing builds wrong addresses and cannot report a failure.
	- Cause: a share's address is joined to its server without a separator, so shares get malformed addresses and two servers can collide.
	- Cause: no network, or access denied, looks exactly like an empty network - no message either way.
	- Cause: any typed network address is presented as a valid empty folder rather than "not found".
	- Cause: nothing limits how deep the enumeration recurses.
	- Fixed: a share's address is joined with a separator, no-network and access-denied are reported instead of reading as an empty folder, an address that cannot be reached comes back as not found, and the enumeration is depth-limited.
	- Verified on Windows: new test covers the address building - a share now lands under its server, and two server/share pairs that used to run together into one address stay apart. Pre-fix both checks fail.
	- Also verified against real shares: this box serves four of its own, and the test now browses them for real - each comes back as a link to its UNC path, and each is opened to prove the link goes somewhere. The one that does not open is an empty optical drive, which the test names rather than counting against the backend.
	- Still open: the no-network and access-denied halves. Both need a machine that fails in those specific ways, which this one does not.

- ✅ Code Review 20260804 item 13. Windows context-menu actions break on ordinary paths.
	- Cause: "Open as Administrator" passes the folder unquoted, so anything with a space arrives as two separate locations.
	- Cause: "Open in Terminal" at a drive root passes a trailing backslash that swallows the closing quote.
	- Fixed: both paths quote properly, so a folder with spaces and a drive root each work.
	- Verified on Windows: new test over the quoting itself - drive roots, UNC roots, spaces and embedded quotes. Pre-fix, both root cases fail. The two hand-offs themselves can't run unattended, since one raises a UAC prompt and the other opens a console.

- ✅ Code Review 20260804 item 14. Opening a Windows shortcut can truncate its target or hang the app.
	- Cause: targets past the old length limit are silently cut short and then opened, wrongly.
	- Cause: a shortcut pointing at itself, or at a loop of shortcuts, recurses until the app runs out of stack.
	- Fixed: the target is read at full length, a chain of shortcuts is followed to its end with a loop guard, and a failed read leaves an error behind.
	- Verified on Windows: new test creates and reads back a shortcut, including one aimed past the old length limit.
	- Also found and fixed while checking it: Windows itself refuses to store a target that long, and we were not looking at the answer - so "Make Link" wrote a shortcut pointing at nothing and called it a success. It now refuses and says why, and leaves no file behind. Pre-fix the new checks fail.

- ✅ Code Review 20260804 item 15. A duplicated line in the settings file empties a list instead of falling back.
	- Cause: an unreadable list is treated as a deliberately empty one. Only lists behave this way; single values fall back correctly.
	- Note: a duplicated column list opens the list view with no columns at all. Hand-editing is a supported way to use this file, so this is easy to hit.
	- Fixed: a setting listed twice, or holding the wrong kind of value, falls back to its default and says so instead of coming back empty.

- ✅ Code Review 20260804 item 16. An external edit arriving mid-change throws the change away.
	- Cause: settings are written a couple of seconds after they are changed, and a file reload in that window replaces the pending change with no warning.
	- Fixed: a change made in the app inside the save delay is carried across the reload instead of being replaced by what is still on disk.

- ✅ Code Review 20260804 item 17. Settings changes can be announced from a background thread.
	- Cause: deleting files updates favorites from a worker thread, and the change is announced on that same thread.
	- Note: the previous settings system always announced on the main thread, which is what every listener assumes. Nothing fires today, so this is a trap for the next listener added.
	- Fixed: change notifications are always delivered on the main thread, which is what every handler assumes.

- ✅ Code Review 20260804 item 18. A damaged per-folder settings file is discarded without a word, then overwritten.
	- Cause: a parse failure leaves an empty store, and the next change writes that empty store over the file.
	- Note: costs every folder's saved view, zoom, sort and layout. A failed save is likewise ignored.
	- Fixed: an unreadable per-folder settings file is reported and kept aside, so the next change cannot overwrite the only copy.

- ✅ Code Review 20260804 item 19. Setting the thumbnail size limit above two gigabytes breaks thumbnails.
	- Cause: the limit is stored in a smaller number than the dialog offers, so the large choices wrap. Eight gigabytes turns every thumbnail off; two and four turn the limit off entirely.
	- Fixed: the size limit is read at full width, so the large choices work instead of turning thumbnails off or on wholesale.

- ✅ Code Review 20260804 item 20. Opening a folder on an unresponsive drive freezes the whole window.
	- Cause: the fallback added for unreadable folders asks for the listing in a way that blocks until the system gives up.
	- Cause: it also treats any general failure as that same case, so a passing glitch is remembered as a made-up folder with no way to tell.
	- Cause: a folder with many unreadable entries stops partway and shows an error over a half-listed folder.
	- Fixed: the fallback for an unreadable folder no longer blocks the window, the skip allowance counts a run rather than a total, and a folder that could not be read is recorded as unknown rather than confirmed empty.

- ✅ Code Review 20260804 item 21. Right-clicking a path segment can offer actions the folder will not allow.
	- Cause: the menu is now built before the folder's details have loaded, and the unknown state reads as "everything is permitted", so Delete and New Folder appear on read-only places.
	- Fixed: a path segment whose details have not loaded no longer offers actions the folder may not allow.

- ✅ Code Review 20260804 item 22. Changing the default zoom discards a zoom deliberately set in another tab.
	- Cause: every open view reacts, not just the visible one, so background tabs lose their own setting.
	- Fixed: only the folder in front of you gives up its pinned zoom when the default changes.

- ✅ Code Review 20260804 item 23. The "treat root as a normal user" preference is read before settings are open.
	- Cause: it is consulted while handling the command line, which happens first, so it is answered wrongly and then remembered.
	- Fixed: the preference is no longer answered and remembered before settings are open.

- ✅ Code Review 20260804 item 24. Folder listing and file moves do more per-file work than they used to.
	- Cause: every file now builds an address and takes a shared lock to check the per-folder store, where before there was a cheap early exit.
	- Cause: every moved file scans the whole store, so a large move gets slower the more is stored.
	- Cause: on Windows the per-type icon is rebuilt for every file on every update, not just when the type changes.
	- Cause: the store is rewritten whole on every save and never pruned.
	- Fixed: an empty store costs nothing per file, a move only scans when there is something to re-key, and the Windows per-type icon is derived once per type instead of once per file.

- ✅ Code Review 20260804 item 25. Reading a setting costs more than it should, and text settings grow memory.
	- Cause: every read searches the whole settings table from the start.
	- Cause: reads of text, list and choice settings allocate inside the settings document and never give it back, and one of them runs on every icon the mouse passes over.
	- Fixed: settings are looked up directly rather than searched from the start, and the memory the settings document hands out is reclaimed instead of growing for the life of the run.

- ✅ Code Review 20260804 item 26. The Windows recycle bin is rescanned far more than needed.
	- Cause: a full scan runs every few seconds for the life of the app, twice more on every look at the trash folder, and once more for every item not already known.
	- Fixed: a look at the trash folder scans once instead of twice, and the periodic check notices a change that leaves the count the same.

- ✅ Code Review 20260804 item 27. The release checksums file can be written wrong.
	- Cause: an empty release folder still writes a bogus line, and any artifact name with a space would be split in two.
	- Note: this is the file both installers verify a download against.
	- Fixed: null-separated, and it no longer runs the checksum tool at all when there is nothing to check.

- ✅ Code Review 20260804 item 28. Assorted unsafe or non-portable paths in the pipeline and installer scripts.
	- Fixed: the Windows installer no longer moves the new copy into place in a way that fails across drives after the old one is already gone.
	- Fixed: the installer's own `--help` now prints when run the documented way.
	- Fixed: a prerelease version in an archive name is no longer reported as the plain release number.
	- Fixed: the release archive no longer fails outside a checkout over its timestamp.
	- Fixed: refreshing the bundled themes rewrites only its own section of the notes file, leaving the rest alone.
	- Fixed: the Windows pipeline's publish step fast-forwards, like everything else here.
	- Fixed: the publish step counts untracked files as a dirty tree, so the stash covers them.
	- Fixed: a failed image-loader cache build leaves the previous cache alone instead of an empty one.
	- Fixed: the pipeline stops with an explanation when there is no one to answer its prompt.
	- Fixed: the packaged launcher finds whichever library folder the build produced.
	- Fixed: the publish helper splits the setting instead of running it.
	- Fixed: both delete-and-replace paths check what they are pointing at first.

- ✅ Code Review 20260804 item 29. Script style and speed debt.
	- Fixed: the backup rotation, the dogfood pruning and the argument parsing all use builtins where they used to start a program per item.
	- Fixed: the unused function is gone.
	- Fixed: the output helpers now live in one file that the helper scripts share, instead of each carrying its own lesser copy.
	- Fixed: the Windows installer gained proper built-in help, so `Get-Help` and `-?` work.
	- Note: the review said three scripts had diverged output helpers; only one actually had. The others define a single matching helper, which is fine.

- ✅ Code Review 20260804 item 30. A Windows-only test reports a pass when it did not run.
	- Cause: the trash test exits successfully unless it detects the compatibility layer used for development, so on real Windows it silently skips.
	- Note: that is exactly where items 9 and 10 would have been caught.
	- Fixed: it reports a skip instead of a pass when it cannot run.

### Code review 20260815

Full code, security and performance review of the whole tree, first-party and inherited, driven by a multi-agent pass plus a full-tree static-analysis run. Findings were adversarially re-checked before landing here; the ones that survived are below, worst first. Technical detail is kept out of this file. Fixes not started. Numbers are continuous and match the private detail notes.

#### High

- ✅ Item 1. Windows trash acts on file paths from the address with no check that they belong to the recycle bin.
	- Cause: delete, move and read take the raw path straight from a `trash:///` address, so a crafted address can read or permanently delete any file.
	- Cause: the one place that does check compares un-normalized text, so a `..` inside a bin item's path escapes it.
	- Fixed: a path from a trash address is resolved to its canonical form and has to name something the recycle bin actually holds before it is read, moved or deleted.
	- Verified on Windows: new test aims a trash address at a file outside the bin and at a real bin item walked back out of it with `..`, and checks delete, move and read each refuse and leave the file alone. Each is re-seeded so one succeeding cannot mask the next. Pre-fix all four checks fail and the file is really gone.

- ✅ Item 2. Reading dragged icon-list data can walk off the end of the buffer.
	- Cause: on the no-geometry branch the remaining-length bookkeeping is skipped and the end-of-data guard tests a pointer that is never null, so the scan runs past the buffer.
	- Fix: decrement size on that branch too, test `*p` not `p`. Guard-page regression test (test-nemo-dnd).

- ✅ Item 3. "Open in Terminal" crashes when no known terminal is installed.
	- Cause: with no terminal found the prefix stays empty and is then dereferenced anyway. Likely on a minimal or KDE-only box, which is exactly the de-Cinnamon target.
	- Fix: return NULL with no terminal, caller declines. Regression test (test-eel-terminal).

- ✅ Item 4. Freeing an extension column object corrupts the heap.
	- Cause: finalize frees memory the type system owns. Latent only because built columns are cached for the process life; any extension that discards a column hits it.
	- Fix: drop the g_free of the instance-private block. Regression test (test-nemo-column).

- ✅ Item 5. An unreadable settings file is treated as empty, and a queued save can then erase it.
	- Cause: any read failure (a sync/AV/editor lock, or the delete half of a non-atomic external save) loads defaults into memory; a pending save then writes the near-empty document over the real file.
	- Fix: keep the in-memory doc on a transient read failure; only a truly absent file resets to defaults. Regression test (test-nemo-config).

- ✅ Item 6. A NUL byte anywhere in the settings file truncates it on the next save.
	- Cause: the file is written using string length, which stops at the first NUL, dropping every key after it. The follow-up check is fooled the same way, so the loss is invisible.
	- Fix: write and compare by byte length (g_memdup2 + canon.n), never strlen. Regression test (test-nemo-config).

- ✅ Item 7. The thumbnail enable-check reads the disabled-types list without its lock.
	- Cause: one reader skips the lock the writers and the other reader use, so a settings change on another thread can free the list mid-read.
	- Fix: take priv->lock around is_disabled at the can_thumbnail site (generate_thumbnail already does); is_disabled documented as caller-holds-lock. Threading race, no deterministic test.

- ✅ Item 8. Replacing a folder deletes through directory symlinks inside it.
	- Cause: the recursive remove never checks the child type, so a symlink to another directory is followed and its contents are deleted, outside the folder the user agreed to replace.
	- Fix: NOFOLLOW type-gate before recursing (mirrors delete_trash_file/set_permissions_file); only real dirs recurse, everything else is unlinked. Premise test (test-nemo-symlink-recurse); the static fn behind a modal Replace dialog can't be driven unattended.

- ✅ Item 9. An invalid filename search pattern crashes the search.
	- Cause: a regex that fails to compile leaves a null pattern but the search runs anyway, then frees an uninitialized match on every file. Reachable by pressing Enter before the typing check catches up.
	- Fix: guard NULL filename_re (match nothing), init match_info NULL and g_clear_pointer it. Regression test (test-nemo-search-regex).

- ✅ Item 10. Restoring an item from the Windows trash drops its file extension.
	- Cause: the original name is taken from the shell display name, which hides known extensions by default, and that shortened name is what restore writes.
	- Fixed: the real extension is taken from the backing file, so the listed name and the restored name both keep it.
	- Verified on Windows: the round trip is covered by a new test - a recycled file is found under its full name, reports the original location it came from, and restores to it. That part holds.
	- But the cause does not reproduce on Windows 11: with "hide extensions for known file types" switched on, the recycle bin still reports full names, in this app and at any setting. So the repair is inert here rather than load-bearing, and the test passes with it removed. Kept for older Windows, and corrected while checking - it used to give up on any name containing a dot, so `report.2026.txt` would have been repaired to `report.2026`.

- ✅ Item 11. Opening certain images can crash if the tab is closed first.
	- Cause: the image-viewer sort path dereferences the originating tab with no null check, and that pointer is cleared when the tab closes mid-open. This is the default double-click-an-image path on Mint-family setups.
	- Fix: guard the NULL weak slot/content_view in add_sorted_view_uris; the image still opens without the wrap-around loop. GUI-async path, no isolated test.

- ✅ Item 12. The places sidebar keeps reacting to settings after it is destroyed.
	- Cause: two preference handlers are left connected at teardown, so a later settings change (including a live edit of the settings file) fires on freed memory. Triggered by hiding the sidebar or switching to the tree sidebar.
	- Fix: dispose now disconnects desktop_setting_changed_callback from nemo_desktop_preferences and reset_menu from nemo_preferences.

- ✅ Item 13. New Folder in the tree sidebar aborts the app when creation fails.
	- Cause: the callback ignores the failure flag and passes a null location on, which asserts. A permission race or a dismissed error dialog triggers it.
	- Fix: bail on !success || new_folder == NULL (matches the directory-view twin).

- ✅ Item 14. Jumping more than one step forward corrupts the history lists.
	- Cause: the transfer loop reads one list but edits the other two, so the back and forward lists end up sharing and leaking nodes; a later navigation then frees entries still in use.
	- Fix: remove from forward_list / prepend to back_list (mirrors handle_go_back's symmetric form).

- ✅ Item 15. On Windows every file reports as changed on every refresh.
	- Cause: the per-type icon override is compared against the plain system icon, which never matches, so each refresh marks the whole folder changed and re-sorts, redraws and re-checks thumbnails, plus a per-file registry lookup and allocation.
	- Fixed: the icon is judged on where it ends up rather than mid-update, so a Windows refresh no longer reports every file as changed.
	- Verified on Windows: a new test refreshes real files of several types five times over and requires everything after the first sighting to report nothing changed, plus a real change that still has to come through. Pre-fix every file reports changed on all five passes.

- ✅ Item 16. Sidebar rebuilds block the whole window on filesystem queries.
	- Cause: free-space and drive-type checks run on the UI thread for every drive and mount, on every rebuild. A slow or hung mount freezes the window, and a mount change is often what triggers the rebuild.
	- Fix: back get_disk_full with a per-sidebar cache; it only ever reads the cache, so the build never waits. A miss or stale (>8s) entry fires an async filesystem-info query off the UI thread that fills the cache and coalesces a rebuild. Cancellable torn down in dispose; a hung mount leaves one entry pending and never blocks. All the inline tooltip/show-df composition is untouched.

#### Medium

- ✅ Item 17. The code-signing password is passed on the command line, visible to other local processes.
	- Fixed: the certificate is imported and signed by fingerprint, so the password never appears on a command line another process can read.

- ✅ Item 18. The Windows sysroot packages are downloaded and unpacked with no integrity check, and those libraries ship in the release.
	- Cause: neither the database signature nor the per-package checksum is verified, though the checksum sits in data the fetcher already parses.
	- Fixed: every package is checked against the checksum the database already carries, and a mismatch stops the build.

- ✅ Item 19. A malformed D-Bus Open hint from any local process crashes the running app.
	- Cause: a hint with no `=` yields a null that is parsed without a check.
	- Fix: guard split_options[1] != NULL before sscanf.

- ✅ Item 20. A pathological settings file can kill the app during parse.
	- Cause: the file is read with no size cap, the parser keeps every decoded byte for the document's life, and an allocation failure exits the whole process from library code.
	- Fix: 8 MiB read cap in load_locked; oversized file refused, in-memory doc kept. Regression test (test-nemo-config).

- ✅ Item 21. In the Windows pipeline, an abort between stash and pop strands the working changes, and a rerun can commit conflict markers.
	- Fixed: a conflicting restore now stops and says where the work is and how to get it back, instead of leaving a rerun to commit a half-merged tree.

- ✅ Item 22. In cicd.bash, a remote-sync stash-pop conflict aborts with no guidance and the stash still held.
	- Note: the natural rerun with sync off then builds and publishes a tree missing the stashed changes.
	- Fixed: same as above - it stops with the stash named and the two ways out spelled out.

- ✅ Item 23. The version-bump guard blocks the beta-to-final release push.
	- Cause: version sort orders `1.0.0` before `1.0.0-beta2`, the reverse of release order, so cutting final over the current beta fails the guard. This exact transition is next.
	- Fix: map '-' to '~' before sort -V (as package.bash does) so a prerelease sorts below its release.

- ✅ Item 24. Accessibility paste reads a freed stack value.
	- Fix: heap-allocate the paste struct, free it in the receive callback.
	- Cause: a stack struct is handed to an async clipboard callback that runs after the function returns.

- ✅ Item 25. install.bash deletes the existing install before the replacement is in place.
	- Fix: stage beside the prefix (cp onto its own filesystem first), then swap with same-filesystem renames and roll back on failure; the old install is only dropped once the new one is in place.
	- Cause: a cross-filesystem move that fails partway leaves nothing installed, and the temp copy is then wiped on abort.

- ✅ Item 26. install.ps1 can half-delete a running install.
	- Fix: same stage-beside-then-swap as bash, plus fInUse now reads paths via Win32_Process (covers protected/cross-session processes) with a separator boundary guard. Windows file-locking edge cases still want the real-Windows pass.
	- Cause: a process whose path cannot be read is treated as not running, so the delete proceeds against a locked copy and throws partway.

- ✅ Item 27. A partial extension crashes every location load.
	- Fix: guard the get_widget vfunc != NULL (as the column provider does).
	- Cause: one provider dispatch skips the null-vfunc guard its siblings have, so an extension that leaves the function unset is called through null.

- ✅ Item 28. An action's exec condition decides on an uninitialized value when the spawn fails.
	- Fix: init return_code = -1 and return FALSE on spawn failure.
	- Cause: a missing binary or a parse error leaves the result unset, so menu visibility is decided by stack garbage.

- ✅ Item 29. Actions stored in a path with spaces run the wrong command.
	- Fix: quote the dir+separator as one token so the program name joins to it through the shell split; also fixes the win32 backslash separator.
	- Cause: the action directory is prepended unquoted before the command is split on whitespace. Normal on Windows and on Linux homes with spaces.

- ✅ Item 30. Any drag-and-drop clears a pending cut or copy.
	- Fix: search the clipboard's uris, not the incoming list against itself.
	- Cause: the collision check compares the dragged list against itself, so it always matches and always clears the clipboard.

- ✅ Item 31. The settings-groups table is read from worker threads and grown on the main thread with no lock.
	- Fix: guard the table lookup/insert with config_lock; emit still fires outside the lock so handlers can re-enter.
	- Cause: a lazy insert can resize the table while a worker thread is reading it. Narrow window, but memory-unsafe.

- ✅ Item 32. The favorites change-timer id is touched from worker threads without a lock.
	- Fix: guard changed_timer_id under the existing infos_lock in queue/callback/dispose.
	- Cause: a worker can remove a timer id the main thread already reused, silently killing an unrelated source.

- ✅ Item 33. Two favorites with the same name in same-named parents collide.
	- Fix: disambiguate with the home-relative/native parent path (ellipsized), plus a counter guard so the name is always unique. Regression: new dedup case.
	- Cause: disambiguation appends only the parent's name, and the display name is the favorite's identity, so operations on one can hit the other.

- ✅ Item 34. Trashing a file drops favorites of unrelated sibling paths.
	- Fix: boundary-guard the prefix (exact or '/' at the split), not a bare has_prefix.
	- Cause: the removal matches by raw prefix with no path boundary, so trashing `ab` also drops the favorite for `abc.txt`.

- ✅ Item 35. The mount lookup matches sibling paths by prefix.
	- Fix: same boundary guard on the mount-root prefix test.
	- Cause: no trailing-separator check, so a path can be matched to the wrong mount and misclassified as local or network.

- ✅ Item 36. Successful direct-save drops are reported as failed.
	- Fix: the dead XDS branch checked 'F' twice; the success branch now checks 'S'.
	- Cause: the success branch repeats the fallback branch's test and is unreachable, so a saved file is reported as a failed drop.

- ✅ Item 37. A failed metadata save is silent and throws away the pending metadata.
	- Fix: check g_file_set_contents; only clear dirty on success, warn on failure.
	- Cause: the write error is ignored and the data is marked saved, so it is never written again and is lost on restart.

- ✅ Item 38. Large-zoom images render blurry on Windows.
	- Cause: the can-load check misses the content-type conversion the rest of the code uses, so the full-resolution path never triggers.
	- Fixed: the check converts the type first, the way the rest of the code does, so the full-resolution path runs.
	- Verified on Windows: a new test writes real images and requires the internal-thumbnail check to accept them, and to keep refusing text. Pre-fix both image cases fail. Confirmed here that the stored type for a `.png` really is ".png", which is why the conversion is needed at all.

- ✅ Item 39. A trashed folder whose status can't be read is shown as a healthy file.
	- Cause: the fallback fabricates a regular-file entry with no error inspection, and an item deleted behind the app's back still lists as existing until the next full refresh.
	- Fixed: a folder is shown as a folder, and something that has gone is no longer presented as readable.
	- Verified on Windows: a new test lists the bin, removes an item behind the backend's back, and requires the entry to come back saying outright that it cannot be read. Pre-fix it reads as a healthy file. The folder half is covered only by a live trashed folder listing as a folder - forcing a folder that is present but unreadable was not attempted.

- ✅ Item 40. Freshly trashed items get a wrong parent until the next poll.
	- Cause: the top-level check does not refresh on a miss, unlike the sibling lookup, so a not-yet-seen item is filed under a bogus parent.
	- Fixed: the top-level check refreshes on a miss, like the sibling lookup.
	- Verified on Windows: a new test warms the snapshot, recycles a file, then finds its backing path by reading the bin off disk rather than through the enumerator - which would refresh and hide the whole thing - and requires the item's parent to be the bin root. Pre-fix the parent comes back as the per-user bin folder, which is not in the bin at all.

- ✅ Item 41. The bookmarks window's no-selection guard never fires and can abort.
	- Fix: get_selected_row and its local are gint, so the < 0 no-selection check works.
	- Cause: an unsigned row holds a would-be -1, so the guard is dead and an assert or a wrapped index is reachable.

- ✅ Item 42. A failed or empty drop on the .desktop launcher editor crashes.
	- Fix: guard NULL data / negative length before g_strsplit in both drag handlers.
	- Cause: both drag handlers split the data and index the first element with no length check.

- ✅ Item 43. Rename-pending activation relies on a garbage return value and leaks the selection each tick.
	- Fix: free file_list on the renaming early-return; real GSourceFunc wrapper returns G_SOURCE_REMOVE.
	- Cause: a void function is installed as a repeating timeout, and the still-renaming early return does not free the selection it fetched.

- ✅ Item 44. Two invalid search patterns warn fatally and show the wrong message.
	- Fix: g_clear_error between the filename and content checks.
	- Cause: the content check is handed an error that is already set from the filename check.

- ✅ Item 45. Tree-sidebar Paste races a freed file and holds a stale view pointer.
	- Fix: ref the view over the async request and guard NULL popup_file in the reply.
	- Cause: the clipboard request keeps no reference and an idle frees the target first, so paste from another app degrades to nothing, and a closed sidebar leaves a dangling pointer.

- ✅ Item 46. The script debug log reads a path after freeing it.
	- Fix: free local_file_path after the DEBUG that reads it, not before.
	- Cause: the path is freed just before the debug line that formats it. Fires when the directory-view debug domain is on.

- ✅ Item 47. The failed-home fallback reopens the failing location instead of root.
	- Fix: open root (not the same failing location) so an undisplayable home stops retrying.
	- Cause: the root fallback is built but never used, so an unreadable home retries itself in a loop. The hardcoded root also resolves to the current drive on Windows.

- ✅ Item 48. The Windows trash test writes past a buffer.
	- Cause: a 64-bit size is written through a 32-bit pointer on Windows, so half the length is stack garbage that then sizes and indexes a buffer.
	- Fixed: the length is taken in the right size, so nothing past the buffer is written or read.
	- Verified on Windows: the trash test used to report itself skipped on this box whatever it had done. It now works the recycle bin directly and reports a real result, so this code runs natively on every run.

- ✅ Item 49. The dogfood launcher mangles pass-through arguments containing quotes or trailing backslashes.
	- Fix: fQuoteArg now does full MSVCRT-style quoting and is applied to every Start-Process ArgumentList element (not just whitespace ones), and the sh round-trip uses a clean `exec "$0" "$@"` script. Verified end-to-end on Linux with space/quote/trailing-backslash args; the old form also split plain spaced args.
	- Cause: Start-Process joins ArgumentList with a naive space join and the target re-splits it, so only-whitespace bare-quoting lost quotes, backslashes, and even split spaced args in the sh round trip.

- ✅ Item 50. Typing a UNC path blocks the whole window on a network probe.
	- Fix: skip the sync existence probe for `\\host\share` input (structural backslashes, not a pasted local path) and hand it to the async load path.
	- Cause: the backslash-to-slash retry does synchronous existence checks on the UI thread, so an unreachable host stalls for the full network timeout before the location even opens.

- ✅ Item 51. Failed thumbnails are re-decoded on every icon fetch.
	- Cause: the app records failures under its own name, which the system's "failed" flag never reads, so every failed file re-hashes and re-decodes a PNG on each fetch. In list view that is per row per draw.
	- Fix: cache the negative can-thumbnail verdict per file (thumbnail_try_ruled_out); reset on clear_info, info init and mtime change so a changed file re-attempts.

- ✅ Item 52. Content search buffers whole files into memory with no cap.
	- Fix: cap the per-file read at 16 MB (it is copied twice more downstream), so an unbounded stream can't exhaust the worker thread.
	- Cause: each candidate text file is read entirely, then copied again to validate and strip, so a multi-gigabyte file can freeze or exhaust memory.

- ✅ Item 53. The list view rebuilds and rescales each icon on every row draw.
	- Cause: the icon, emblems and a fresh surface are assembled with no caching, and thumbnails are rescaled every time, so any redraw re-does the work for every visible row.
	- Fix: cache the rendered surface on the FileEntry keyed by (column, scale, thumb-shown); reuse across draws, invalidate on file change and free. Drag-accept and cut-highlight still render live.

- ✅ Item 54. The list view re-invalidates visible thumbnails on every scroll pause.
	- Fix: drop the unused shown fetch and invalidate only on the first-in-view transition (deferred-attrs NO->YES), matching the icon-container twin.
	- Cause: an already-loaded flag is fetched and then ignored, so every visible file's thumbnail and extension info are re-read at each scroll settle.

#### Low

Terse by design; file and mechanism are in the private detail notes. All confirmed on read, minor impact or rare paths, mostly inherited.

- ✅ Item 55. Vendored-theme staging uses a fixed temp path instead of a unique one (symlink race on a shared box).
	- Fixed: staged under a unique temp directory that is cleaned up on exit.

- ✅ Item 56. One version parser in the push hook lacks the guard the others gained; correct only by token order today.
	- Fixed: the guard is in, so it can no longer match the wrong field on the same line.

- ✅ Item 57. The portable packer copies the app folder without recursion, silently dropping any subfolder's contents.
	- Fixed: the copy recurses, so subfolders keep their contents.

- ✅ Item 58. The packer passes a single unquoted string as arguments, so an output path with spaces splits.
	- Fixed: the argument is quoted, so a path with spaces stays one argument.

- ✅ Item 59. The packer's fixed grace-then-kill can truncate an exe still being written.
	- Fixed: it waits for the output to stop growing rather than a fixed grace.

- ✅ Item 60. Hand-supplied negative-offset window geometry is computed off-screen and clamped to the primary monitor.
	- Fixed: a negative position now places the window's far edge that far in from the screen edge, as it is meant to.

- ✅ Item 61. Extension menu-item setters ref a null value, so a nullable field can't be cleared and an optional widget always warns.
	- Fixed: an optional widget can be left unset, and a menu can be cleared.

- ✅ Item 62. The extension property-page dispose never chains up to the parent.
	- Fixed: dispose chains up.

- ✅ Item 63. The settings flush reads and clears the save-timer id without the lock.
	- Fixed: the timer is taken under the lock.

- ✅ Item 64. A trashed-file timestamp is formatted and parsed with a type that truncates on 64-bit Windows.
	- Fixed: the timestamp is written and read at full width, so the round-trip survives on 64-bit Windows.
	- Verified on Windows: a freshly recycled file reports a deletion date of the right shape and in this century, which a truncated one would not be.

- ✅ Item 65. An unreadable directory records a confirmed-empty file-type list instead of an unknown one.
	- Fixed: an unreadable directory records an unknown type list rather than a confirmed-empty one.

- ✅ Item 66. One removal helper dispatches to the changed path instead of the removed path.
	- Fixed: it dispatches the removal.

- ✅ Item 67. A file object leaks for each overwritten destination during a move.
	- Fixed: the reference is released.

- ✅ Item 68. The drag URI array writes its null terminator one element past the allocation.
	- Fixed: the array is one longer, so the terminator lands inside it.

- ✅ Item 69. A failed filesystem query during a desktop drag unrefs a null.
	- Fixed: guarded, and the drag falls back to no filesystem information.

- ✅ Item 70. A missing favorite name aborts the whole favorites listing rather than skipping the entry.
	- Fixed: a missing entry is skipped rather than aborting the whole listing.

- ✅ Item 71. Cancelling a favorites listing mid-batch leaks the gathered entries.
	- Fixed: the gathered entries are released on cancellation.

- ✅ Item 72. An empty favorites metadata entry reads past the split result.
	- Fixed: guarded, so a malformed entry is kept rather than read past.

- ✅ Item 73. The favorite-info free dereferences the struct before its null guard.
	- Fixed: the guard comes first.

- ✅ Item 74. Skip-all on a delete or directory copy does not mark the file skipped.
	- Fixed: skip-all marks the file skipped, so the folder is not reported as fully removed.

- ✅ Item 75. The read-only-destination path frees a null error.
	- Fixed: it no longer frees an error that was never set.

- ✅ Item 76. A D-Bus-initiated copy passes a null desktop location to an equality test.
	- Fixed: guarded.

- ✅ Item 77. The existing-ancestor walk unrefs a null for every missing level.
	- Fixed: it no longer releases something it never got.

- ✅ Item 78. A synthesized Windows file info with no icon makes the update ref a null icon.
	- Fixed: guarded, so a synthesized entry with no icon is accepted.

- ✅ Item 79. The job-queue finalize unrefs plain-malloc structs.
	- Fixed: released the way it was allocated.

- ✅ Item 80. The duplicate-job guard compares a function against user data and never fires.
	- Fixed: it compares the right thing, so a repeated job is caught.

- ✅ Item 81. Launching by URI casts a possibly-null parent window for the scale factor.
	- Fixed: guarded, with a sensible default when there is no parent window.

- ✅ Item 82. Skip-folder setup dereferences a null path for a non-native search location.
	- Fixed: guarded, so a location with no path is handled.

- ✅ Item 83. The count-based recycle-bin monitor misses same-count changes.
	- Fixed: the check now also watches total size, so a change that leaves the count the same is noticed.
	- Verified on Windows: a new test watches the bin, swaps one item for a much larger one with the main loop parked so no poll can catch the count mid-swing, and requires the watcher to be told. It sits quiet through a poll turn first, so a monitor that cried change every time would fail rather than pass. Pre-fix nothing is reported at all.
	- Learned here: rewriting a bin item's backing file does not move the reported size - Windows answers with the size recorded when the item was recycled. An item leaving and a differently-sized one arriving does move it, which is the case the fix is for.

- ✅ Item 84. A static global for the connect-server result is clobbered by concurrent dialogs.
	- Fixed: the result travels with the request, so two dialogs at once no longer clobber each other.

- ✅ Item 85. The desktop-item property page leaks the type string for other launcher kinds.
	- Fixed: released.

- ✅ Item 86. The list-model drag binder leaks the per-row path string.
	- Fixed: released.

- ✅ Item 87. A file-changed emission uses a stale iterator after bumping the model stamp.
	- Fixed: the position is taken again after the model changes.

- ✅ Item 88. Column-reorder leaks the column name array in search views.
	- Fixed: the list owns its own copies and every one of them is released.

- ✅ Item 89. The unhandled-URI dialog leaks a file reference and tolerates null poorly.
	- Fixed: released, and a file that is not in the cache is handled.

- ✅ Item 90. Launch dereferences the command line with no null check.
	- Fixed: guarded, for a program with no command line of its own.

- ✅ Item 91. Activation uses a weak parent-window pointer with no null guard for the screen and dialogs.
	- Fixed: guarded, so activation survives the tab being closed under it.

- ✅ Item 92. The pathbar leaks file objects on rename and at finalize.
	- Fixed: released on rename and at teardown.

- ✅ Item 93. An unstored post-drop timeout can fire on a destroyed sidebar.
	- Fixed: the timeout is kept and cancelled when the sidebar goes.

- ✅ Item 94. Aggregate progress percentage uses a wrong recurrence for three or more concurrent operations.
	- Fixed: a plain average, so three or more operations report honestly.

- ✅ Item 95. The properties window leaks a pending key when one is already pending for the same files.
	- Fixed: released.

- ✅ Item 96. The mount-content callback leaks its mount, cancellable and data when content detection is off.
	- Fixed: released when nothing takes them on.

- ✅ Item 97. The copy test has no assertions and can pass before the async work appears.
	- Fixed: it builds its own files, copies them, and checks the result - and was proven to fail without a working copy.

- ✅ Item 98. The editable-label test is not wired into any build, so it never runs.
	- Removed: it was an interactive demo with no build wiring and no way to run unattended.

- ✅ Item 99. The config test never makes warnings fatal, so its negative checks cannot fail.
	- Fixed: an unexpected complaint now fails the run.

- ✅ Item 100. The favorites test never removes its temp directories.
	- Fixed: the temp tree is removed.

- ✋ Item 101. Carriage returns in settings values are not escaped and are stripped on reload.
	- Deferred: fixing it means changing both halves of the vendored settings parser and with them the on-disk escaping, for a character no setting ever contains.

- ✅ Item 102. The row-under-pointer helper leaks a tree path on every call (per drag-motion).
	- Fixed: released.

- ✅ Item 103. The extension simple-button leaks a surface and can use an uninitialized size.
	- Fixed: released, and the size is seeded so an unknown icon size cannot be read before it is set.

- ✅ Item 104. Every settings save leaks a full copy of the file into the parser arena.
	- Fixed: the settings document is rebuilt from its own canonical form when it has handed out enough, so the memory comes back.

- ✅ Item 105. A move leaks the source's parent object on every non-desktop move.
	- Fixed: released.

- ✋ Item 106. Sorting by a string attribute allocates and formats both values on every comparison.
	- Deferred: doing it properly needs a per-file cache of the formatted value with its own invalidation - the same machinery as the icon render cache, for much less gain.

- ✅ Item 107. Thumbnail creation falls back to a synchronous stat on the main thread.
	- Fixed: the fallback lookup happens on the worker instead of the main loop.

- ✅ Item 108. Every mouse-motion event rewrites the whole sidebar tree store.
	- Fixed: only rows that actually change are touched.

#### Architecture and UX notes

Observations and suggestions rather than defects. Not individually reproduced.

- ✋ Item 109. Platform code is split two ways: dedicated Windows modules alongside inline platform blocks in large shared files. Worth settling on one shape.
	- Deferred: a judgement call about convention rather than a defect. Worth settling before the next platform, not during a bug sweep.

- ✅ Item 110. Two separate desktop-terminal fallbacks disagree: "Open in Terminal" honors the configured terminal, launching a terminal app does not.
	- Fixed: both paths fall back to the same scan of known terminals, so neither silently does nothing.

- ✅ Item 111. Localization is effectively dead on Windows and on relocated installs; the locale directory is baked at build time and no packaging step installs or points to it.
	- Fixed: data, translations and helper programs are found relative to the running program, with the built-in path as a fallback.

- ✅ Item 112. Windows drive roots are labeled bare, with no volume label.
	- Fixed: the volume label is shown ahead of the drive letter.
	- Verified on Windows: the sidebar reads "Windows (C:)" and "Extra (K:)" against the real volumes on this box.
	- But only the sidebar was covered - see the drive-root naming item under Bugs.

- ✅ Item 113. The README points Windows users at the wrong settings folder.
	- Fixed.

- ✅ Item 114. "Open in Terminal" on Windows is hardcoded with no setting, though the same item is configurable on Linux.
	- Fixed: the list of terminals to try is a setting, tried in order.

- ✅ Item 115. Failed Windows elevation or terminal launch is silent; the shell-execute result is ignored.
	- Fixed: a failure is reported rather than swallowed. The path is also quoted properly now, so a folder with spaces or a drive root works.

- ✅ Item 116. Selectable message-dialog text grabs focus pre-selected.
	- Fixed: the text is still selectable but no longer takes focus pre-selected.

- ✅ Item 117. The properties window never cancels scheduled owner/group changes on close.
	- Fixed: pending changes are cancelled when the window closes.

- ✅ Item 118. The Ctrl-key state for tab switching is a stale process-wide global.
	- Fixed: the state belongs to the notebook and is cleared when it loses the keyboard.

- ✅ Item 119. The public design doc's code-structure sections are empty scaffolding; the real internal architecture lives only in private notes.
	- Fixed: the code-structure, data-flow, execution, stack, UI and testing sections are written.

### Features and enhancements

- ✅ Windows: two kinds of hidden file, two options.
	- Supersedes the older item that asked for the same thing as one combined switch.
	- The premise turned out to be worse than described: Windows reports only its own hidden attribute, so dot-files were shown there whatever the setting said. Same for names ending in a tilde, which count as backups elsewhere.
	- "Show dot-files" is now a second switch, Ctrl+Shift+H, next to "Show hidden files" in the View menu and hidden on every other platform, where one switch still covers both. Default is to hide them.
	- The two are independent: revealing attribute-hidden files no longer reveals dot-files, and the listing, the tree sidebar and search all go through the same check.
	- Flipping either one re-reads the open folder, so an edit to the settings file shows up without a restart.

- ✅ Windows: choose which separator paths are shown with.
	- A "Paths" group on the Display page of Preferences, shown only on Windows: "Show separator as" picks `\` or `/`, and a checkbox below it accepts or refuses `/` in a typed location.
	- The checkbox is ticked and greyed out while `/` is the separator on screen, since refusing what is being shown would make no sense.
	- The choice reaches every surface that spells out a path: the location bar, the Location column, path tooltips, the window and tab titles, the sidebar tooltips, drive roots in the sidebar, and the Location row in properties. Breadcrumbs show names only, so there was nothing to change.
	- Changing it re-reads the open folder, so the whole window switches over at once rather than on the next visit.
	- Typed input already took both separators, so what is new is the option to turn `/` off. A location that leans on it is then refused with a beep instead of going anywhere.
	- Also fixed on the way past: the preferences dialog named a widget in a size group that no longer exists, so loading it stopped early and silently. Only an unused list model came after the break, which is why nothing looked wrong.

- 🔘 Windows: "Copy path to clipboard as [\|/]".
	- A second clipboard item, directly below the existing Copy Path one, offering whichever separator the display is not currently using.
	- Wanted in the same three menus the existing item appears in: selection, background and breadcrumb.

- 🔘 Windows: "Open with Explorer", for a single selected entry.
	- A deliberate escape hatch rather than a dependency. The standing "depend on Explorer as little as possible" rule is about core function; this is asked for by name.
	- Single entry only, so there is no question of what gets opened.

- ✅ Column widths and the Ext column, second pass. Overrides the earlier column rules where they disagree.
	- "File extension" is now just "Ext", and shows the extension without its leading dot. It sits directly right of Name, with Location next along whenever that is switched on.
	- Location, on an ordinary folder listing, grows with Name rather than stopping at a share of it: the two split whatever the other columns leave and Name takes no more than half, so Location is never the narrower of the pair and anything Name does not need goes to Location. Dragging Location by hand ends that and pins the width, as it always did.
	- Date created, Date modified and Date read keep their full width. What has to give when the window is too narrow comes off Name, Location and Type first, in proportion, and only reaches the dates once those three are down to their floors.
	- Type, and any other column with no natural length, never ends up wider than Name or Location.
	- Zooming in or out re-measures the rows. Before this the widths were thrown away and never worked out again, so one Ctrl+= left Location taking most of the row and every date cut short. Same for a column switched on that had not been on screen to be measured.
	- A small gap keeps the first and last columns off the window frame.
	- All of it watched in the running app, including the zoom case with the fix backed out.

- ✅ The preferences dialog opens larger, and big enough for the Views page to fit without a scrollbar.
	- The height is measured from the page itself rather than fixed, so a different theme, font size or translation still fits, up to what the monitor has room for.

- ✅ Ask before moving files to Trash defaults to on.
	- Already the default; confirmed rather than changed.

- ✅ Right-click properties wording: ours is plain "Properties" and sits first; the Windows sheet reads "Windows properties (Alt+Enter)" below it. Shortcuts themselves are unchanged.
	- Seen in the running app; the breadcrumb menu says "Windows properties" without the hint, since Alt+Enter acts on the selection rather than a path segment.
- ✅ New list columns.
	- "File extension", on by default, between Name and Type, dot included the way Explorer shows it. Left empty when the tail after a dot is not really an extension - folders, dot-files, too long, all digits, or not letters and digits. The refusals have a test of their own that fails with the checks taken out.
	- "Owner" now shows on Windows too and is on by default there - the platform reports the file's real owner, so the old fabricated-values reason to hide it no longer applied.
	- Windows only: "Permissions source" - Inherited, Local or Mixed, read from the file's ACL - off by default, listed after Owner. Verified against files with disabled inheritance and added grants.
	- Type now defaults to at most twice the File extension column's width.
- ✅ Column widths remember the user's hand. Overrides the earlier auto-sizing rules where they disagree.
	- A column with no natural width limit that the user resizes keeps that width as its ceiling from then on, through any window resizing in either direction, saved in settings.
	- Name still takes all remaining space - except in find mode, where Name and Location split the row one-third/two-thirds by default, and an adjusted split is remembered forever and kept as the window resizes. Supersedes the find-mode column note below.
	- Both were watched working in the running app: the dragged ceiling survives narrow-then-wide, and the find-mode split held at the adjusted ratio across sizes.

- ✅ Properties on Windows opens the one Windows itself shows, instead of ours.
	- Alt+Enter, Ctrl+I and every Properties item now hand the selection to the shell's own sheet - the same one Explorer shows, third-party tabs included. Only Windows; Linux, BSD and macOS are untouched.
	- Ours stays on a second item, "Advanced properties" (Ctrl+Enter), because the Windows sheet has nowhere to put a custom icon, an emblem, an annotation or the image details page. It is hidden everywhere else, where both items would open the same window.
	- Anything the shell cannot name falls back to ours rather than doing nothing: a virtual location, a selection spanning folders (which is what a search result set is), or an item that has gone away since it was clicked.
	- The sheet runs off the main loop, so the window behind it stays live while it is open, and it is waited out rather than abandoned - the extra threads go when it closes.
	- Verified on Windows by eye and by test: the fallback rule has checks of its own, and the two that matter go red with the rule taken back out.

- ✅ Every piece of text in the interface reads as a sentence, not as a headline - only the first word capitalised, and anything that is a name left alone.
	- Menus, buttons, tab and page titles, dialog titles, column headings, tooltips, preference labels, and the bundled actions. About 330 labels in all.
	- A mnemonic stays where it was, so the underlined letter does not move; it is simply lower case now. Keyboard shortcut text is untouched.
	- Names keep their capital: the platforms, the toolkit, Trash and the other places in the sidebar, file and disc formats, acronyms. So does a sentence that names a menu item or a tab, since the item itself is still called that.
	- Left alone on purpose: the licence text, which is quoted verbatim, and the name a new folder or document is given, which is written to disk rather than shown.
	- It is checked rather than trusted, because a label copied from upstream arrives in Title Case: `cicd/utility/lint-ui-case.py` reads every translatable string in the tree and fails the lint step on any that is not a sentence. The whole exception list lives in that one file, each entry with its reason.
	- The check found what a first pass by eye did not - the plural labels, where two spellings sit in one call, which is what had left "Copy Paths" and "Make Links" behind. Proven to go red on a label put back to Title Case.


- ✅ One setting for how much of the machine's CPU any compression may use, as a percentage of the cores it finds. Default 50% - the best balance on a hyperthreaded CPU.
	- `performance.cpu-percent`, global rather than per-format, so a later job that can be spread over cores reads the same number instead of inventing one of its own.
	- Reaches the 7z and rar create lines through a `{{THREADS}}` marker of their own, and tar.xz through the library that writes it. Zip, gzip and the built-in 7z have no such option, so they are left alone rather than handed one they would refuse.
	- It is the one marker that does not stand for a control in the Compress dialog, so a line edited past it says nothing - the program simply picks for itself.
	- Rounds up, so a single-core machine still gets one thread and the answer is never nothing.
	- Verified: each program is handed the switch it spells its own way, and both checks fail with the marker taken back out.

- ✅ Per-monitor DPI aware where the platform offers it, and DPI aware at minimum everywhere else.
	- The Windows executable now carries an application manifest, which is where this is declared and where Windows reads it before any of our code runs. Per-monitor v2 where it exists, per-monitor v1 and then system-wide on older builds.
	- Without it the whole window was stretched as a bitmap on a scaled display - blurry - and a second monitor at a different scale could not be followed at all.
	- The toolkit scales in whole steps only, so a display at 125% or 150% would come out at 100% and read smaller than every other window on that screen. Text is scaled to the monitor's real DPI on top of that, which is not restricted to whole steps, and re-reads it whenever a window moves to a monitor at another scale or a monitor is plugged in. Widgets and icons stay on the whole step.
	- Nothing was needed for Linux or BSD: X11 and Wayland desktops publish their own scaling and the toolkit already follows it.
	- The manifest also declares the run level explicitly (unchanged - what we already had by having none) and the versions of Windows we have run on, so the version APIs stop reporting Windows 8 forever.
	- Verified on this box: the running process reports per-monitor awareness and its window reports the v2 context. The scaling sum is covered by a test, which fails with the whole-step part taken back out. This box runs at 100%, so the fraction itself has been checked by arithmetic rather than by eye - worth a look on a scaled display.

- 🔘 A fractional display scale is only applied to text, so widgets, icons and spacing stay at the whole step below it.
	- Falls out of the toolkit scaling in whole numbers. At 150% the type is right and everything around it is a third too small.
	- The way out is our own stylesheet: padding, icon sizes and the like driven from the leftover fraction. Worth doing only once someone has looked at it on a scaled display.

- ✅ F2 selects the whole name, extension and all, rather than just the part before the dot. Settings tunable, for anyone who wants it the other way.
	- Both views. A folder was already selected whole; a file now is too.
	- `preferences.rename-selects-whole-name`, a file-only setting with no control in Preferences.
	- Verified in the running window: F2 on a `.md` file opens the box with the suffix inside the selection.

- ✅ List view columns use the window as it is resized, instead of being pushed off the end of it or leaving a gap.
	- Widening: columns take the new space until one can show the longest value in it, and then that one stops. Name is the only column that keeps growing without limit, so once everything else has what it needs the rest is Name's.
	- Narrowing, which is the same thing read backwards: Name gives its surplus back first, having had all of it. When every column is down to the longest value it holds and it still does not fit, Type gives next, on its own, to about three characters - it is the one least missed that short, where a date or a size that short says nothing. Only then does everything else give ground together, each in proportion to how wide it is, Name included.
	- A column whose values have no natural limit either - Type, Location, Owner, Group - stops at a third of the Name column rather than taking the window for one long value. The cap and Name's width have to agree with each other, so the answer is found rather than guessed, and it does not depend on the order the columns are in.
	- Narrower than the floors add up to and the view scrolls sideways, which is the honest answer to a window narrower than its own contents.
	- Every value that no longer fits now says so with an ellipsis instead of being cut off mid-letter. Only Name and Location did before.
	- Widths follow the contents: each row is measured as it arrives and as its details fill in, and the widest seen is what a column aims for. Measured against a five thousand item folder, it costs nothing that can be told apart from the noise.
	- A column dragged wider by hand keeps that width until the window changes shape or the folder does.
	- Refines the earlier "Name column always as large as possible" work under Done, which only made Name take the slack; this is the rule for all of them.
	- Verified by eye at half a dozen widths on two folders, and the rule itself has a test of its own that fails on the obvious ways to get it wrong.

- ✅ Twelve more icon sets, all of them asked for by name: BeautyLine, the six Simply Circles colours, Lime Numix 2021, MB Lime Suru GLOW, Material Black Pistachio Suru, Avidity Dusk Mixed Suru, FF-BlackGreen and FF-Flamengo-RJ-BR. Twenty-three sets in the picker now.
	- All SVG, all trimmed to the names a file manager asks for, and all inside the executable - the whole icon payload is 6.6 MB, so nothing needed to be a separate download after all.
	- Three new fetch shapes were needed: a repository that keeps one theme family per branch, six themes out of one sparse checkout, and two that ship the icons as a tar committed inside a repository of something else.
	- **Buuf is deliberately not included.** It is CC BY-NC-SA, and the NonCommercial term rules it out of anything shipped and out of the repository. It is worth having, so `filesystem/` explains where to drop it and gives a one-line fetch for it.
	- Three of the twelve carry no licence file upstream and are shipped on weaker evidence than the rest. Each one is named, with what it rests on, in `vendor/README.md` - worth a look before a release.

- ✅ A gallery of every icon set in the README, four icons each on a light and a dark background, plus how to drop your own in. Rendered by `cicd/utility/icon-gallery.py`; re-run it when the set list changes.
	- Each icon is rasterised on its own before being placed. Several sets colour themselves through a stylesheet keyed on a class name they all spell the same way, so pasting their markup into one sheet made six differently coloured sets come out identical - and renaming the classes apart made them all come out black.

- ✅ `filesystem/` - a tree mirroring where things land on disk, so a folder can be copied straight across. Carries the icon and widget drop-in folders, what they are called on each platform, and the two optional `index.theme` keys that tell the picker which modes a theme suits.

- ✅ Windows icon sets: one per Windows generation, all with yellow folders.
	- Luna (XP) and Aero (7) were already ours; Metro (10) and Mica (11) are new, so every bundled Windows widget theme now has icons drawn to match it. The picker pairs them automatically.
	- Folders are yellow in all four. Aero's were blue, which is not what Windows 7 shipped, and a yellow folder is the one colour that reads on a light background and a dark one alike.
	- The XP and 7 folders were too shallow to read as folders at a glance; the body is taller in every era now.
	- The folder itself is drawn per era rather than shared - chunky and outlined for XP and 7, flat and square for 10, rounded with the front panel falling away for 11. It is the icon a Windows generation is recognised by.
	- The vendored Fluent icon set is gone with them: it drew blue folders and looked nothing like Windows 11, and Mica now covers that style. The Fluent *widget* theme stays. About 390 KB and 179 files lighter.

- ✅ Every bundled SVG run through a size pass: 2.1 MB of icon art down to 1.8 MB, and nemo's own artwork from 142 KB to 50 KB.
	- Numbers in path data are rounded to a step finer than a two-thousandth of the icon, which is under a tenth of a pixel at any size one is drawn. Colours fold to their short form and unreferenced ids go.
	- Multipliers - transform matrices, gradient vectors - are deliberately left alone: rounding a scale factor moves everything it touches, which is visible where rounding a coordinate is not.
	- Checked by rendering all 983 icons before and after and comparing pixels. One icon differs at all, by an amount invisible side by side. Doing that caught a real fault first time round: an arc's two flags can be written with nothing between them, and reading path data as a plain run of numbers swallows one and silently reshapes the glyph.

- ✅ Default settings changed: folder expanders on in list view, binary size prefixes (KiB/MiB), and thumbnail visibility inherited from the parent folder.

- ✅ List columns trimmed to one row per idea.
	- Three dates, the same three everywhere: Date Created, Date Modified (on by default) and Date Read. The "- Time" twins of the first two are gone; they showed the same instant a second way. The times themselves come from whatever each OS keeps them in, so nothing here is per-platform.
	- MIME Type and Detailed Type are no longer offered - neither reads as anything but debug output beside the plain Type column. Off behind a named switch in the source rather than deleted, since the underlying values are still what the properties window and the sort menu use.

- ✅ Appearance page: picking a Style now moves the Icons choice to match it, so a Windows 11 window frame no longer comes with macOS icons. Where a style has no icon set of its own the icons stay put. The note about drop-in theme folders sits further down the page, clear of the two pickers.

- ✅ "System default" in both theme pickers now reads "Nemo Anywhere" - on the bundled targets it is the app's own look, not the platform's.

- 🔘 Session bookmarks - that allow you to jump backwards and forwards to folders and/or files

- 🔘 Search options: Flat [ ]  Hierarchical [ ]

- ✅ Settings belong where each platform keeps them: `%APPDATA%\nemo-anywhere` on Windows, `~/Library/Application Support/nemo-anywhere` on macOS. Linux and BSD keep `~/.config`. Themes stay where they were.
	- A folder left in the old place is moved across on first run, so nobody starts from defaults.
	- Covered by a test that sandboxes both roots and watches the move happen; it fails without the fix.

- ✅ The Windows executable takes too long to start. **14.2s to 3.4s**, and the executable shrank from 39.8 MB to 33.5 MB.
	- Measured first: the packed single exe reached even `--version` in 14.2s against 0.9s for the same build as a plain folder, and all of the difference is spent before our own code runs. The packer charges about 2.8 ms for every file it carries, and the bundled themes were a couple of thousand of them. The packer's own compression and mapping settings were measured and change nothing.
	- The bundled themes now ride inside the executable as one compiled-in resource instead of ~2,200 loose files. The sysroot's full Adwaita and its legacy set - 2,693 files to answer the ~180 names we ask of them, plus 33 X11 cursors that do nothing on Windows - are replaced by our own trimmed copies. The whole folder went from 4,840 files to 152.
	- Trimming Adwaita turned up three faults in the theme resolver that had been quietly costing every bundled theme icons, `emblem-symbolic-link` among them - the one every symlinked file in the view wears. All the bundled themes were rebuilt.
	- A splash appears while it starts, drawn with the platform's own toolkit because it has to be up before GTK is. It lists what startup is doing in a ten-line window that scrolls smoothly, and leaves the moment the real window has drawn.
	- The window itself is now shown at its remembered size and place as soon as it has somewhere to be, rather than after the first folder resolves. The splash goes when the folder has finished listing or a second after the view is up, whichever comes first - a big folder can take twenty seconds to list and there is no sense covering a window that is already usable.
	- Found while watching it: the app had never brought its own window to the front on Windows. Showing a window maps it without activating it, so it opened behind whatever you were looking at; on Linux the window manager focuses new windows itself, which is why it had never shown. Fixed and confirmed by eye.
	- The remaining 2.5s over a plain-folder launch is the packer's own fixed cost and would need a different packer to reach.

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
	- 🚫 Shrink the "Name" column to fit, and make the 'Location' column adjust as wide as possible as the window resizes. Then go back to the way it was, when exiting "find" mode.
		- Superseded by the column-width item near the top: in find mode Name and Location split the row one-third/two-thirds, and an adjusted split is remembered.
	- 🔘 Instead of showing a filename selected in the status bar, show the entire path.

- 🔘 When a value is longer that the column can display, allow a mouseover tooltip to show the whole value.
	- Using a reusable tooltip mechanism

- ✅ Ship with "Copy path(s)" script from current nemo install.
	- Built in rather than shipped as a script, so it needs no interpreter, no clipboard helper and no per-platform install step.
	- On the selection menu, the background menu (the folder being viewed) and a breadcrumb segment; also on the Edit menu, with Ctrl+Shift+C.
	- Copies the native path of each selected item, one per line, unquoted, with no trailing newline - the line ending being the local one, so a paste into cmd or notepad lands as separate lines.
	- Anything with no local path (a remote share) contributes its uri instead, and a recent or favorites entry resolves to the file it stands for rather than copying a virtual uri.
	- Label follows the count: "Copy Path" for one, "Copy Paths" for several. Show/hide checkboxes in Preferences like the other context-menu items.

- ✅ Right-click "Compress...": a cross-platform way to archive the selected files and folders.
	- On the selection menu, the background menu (the folder being viewed) and a breadcrumb segment; also on the Edit menu.
	- A dialog asks for the name, the format and the folder to put it in, prefilled from the selection and the folder being viewed. The name follows the format, so switching from zip to tar.xz swaps the suffix instead of stacking one on top of the other.
	- Compressing one folder - selected, or from the background menu or a breadcrumb - archives the folder itself, so opening the archive shows the folder and the contents are one level in. The archive is named after the folder and offered beside it rather than inside it, which is where a person would look for it. A drive root, having no beside, keeps itself.
	- Selecting everything in a folder and compressing that archives the contents, with no wrapping folder. That one takes the folder's name too, but is offered inside the folder, since that is where the selection was.
	- A part of a folder gets no name suggested, because there is none a person would agree with; the field starts empty and Compress waits until it is filled in.
	- "Compress each item separately" makes an archive apiece instead of one archive, each named after the item it came from and all of them written to the chosen folder. Off by default, and greyed with a single item selected, where there is nothing to separate.
	- With it on there is no name to give, so the name field is greyed - which is also how a part of a folder gets compressed without typing one.
	- Each item keeps its whole name, so "notes.rar" becomes "notes.rar.zip". Swapping the suffix would put the new archive on top of the file being read.
	- However many archives it makes, it is one job: one progress bar, one Cancel, and one question about the ones already there rather than a question apiece.
	- Formats: zip, tar, tar.gz, tar.xz and 7z are written by the built-in library, so they need nothing installed; rar is offered where the rar command is found, and 7z falls back to the 7z command for anything the library cannot write.
	- Options, each offered only where the chosen format and the programs present can honour it: compression level, password (with the option to encrypt the file names too), splitting into volumes with an editable list of the usual sizes, solid archives, storing duplicate files once, storing symlinks and junctions as links, following linked folders (off by default, so a link loop cannot pull in the whole disk), and for rar a recovery record (on by default) and locking.
	- An option nothing can honour is shown greyed rather than hidden, so the dialog does not change shape from one machine to the next.
	- Encryption and splitting are treated as requirements - if nothing installed can do them the job is refused rather than quietly writing a readable archive. Everything else is a preference, honoured where possible and dropped where not.
	- Compression runs as a normal background job: it shows in the same progress popup as copying, can be cancelled, and a cancelled or failed run leaves no half-written archive behind.

- ✅ The 7z and rar command lines are settings, not code, so a user can edit them.
	- Four lines in `settings.shcl` under `archive` - create and unpack, for each of the two programs - each with `{{PLACEHOLDER}}` markers for the parts the app fills in. Point one at a different build, add a switch we do not offer, or work around a version that spells something its own way.
	- Every switch the Compress dialog can turn on has a marker of its own, so an edited line keeps the dialog working. Leave one out and the app says which control has gone quiet.
	- Clearing a line puts the shipped one back rather than running nothing, and a line that cannot be read is refused outright rather than half-run.
	- A password is handed to the program as a value, never written into the settings file.
	- `{{LIKE_THIS}}` is now the convention for any setting that needs a placeholder. Braces because no shell or command prompt expands them, so a line can be pasted somewhere to try it out and come back unchanged.

- ✅ Right-click "Extract" for the archive formats we recognize, including shelling out to 7z or rar.
	- Three items on the selection menu and the Edit menu, shown only when everything selected is an archive: "Extract Here", "Extract Each to Its Own Folder" (singular when one is selected) and "Extract To..." with a folder chooser. Show/hide them in Preferences like the other context-menu items.
	- "Extract Here" unpacks exactly what the archive stores, so one made from a folder brings that folder with it and lands in one place. The folder-each item is the answer to an archive that would otherwise scatter its contents over the folder being viewed.
	- Reading covers far more than writing does: the tar, zip, 7z, rar, cab, lha, cpio, xar and iso families and the bare compressors all open with nothing installed.
	- A 7z or rar command is reached for when the built-in library will not open the file - a multi-volume set, or headers it cannot decrypt. Both are tried in turn, since a program being installed is no promise it can read the file.
	- A protected archive asks for its password once, and reuses it for the rest of the selection.
	- Collisions ask the same question copying asks, with the same answers - skip, duplicate, rename, replace, and applying that answer to everything after it. A folder arriving on a folder merges without asking. The prompt says which archive the incoming file came from, since several can be unpacked at once.
	- An entry whose stored path climbs out of the folder being unpacked into, or names a drive, is put back inside it.
	- Unpacking runs as a normal background job: it shows in the same progress popup as copying and can be cancelled.

- 🔘 Confirm mouse-movement-based actions that don't already ask for some kind of confirmation. (E.g. drag and drop to a new folder)
	- 🔘 A major enhancement to call out in README, e.g.: "Helps prevent one of the biggest pain points with GUI file managers: Accidental file & folder moves, sometimes without realizing it."

- 🔘 Always operate on whole rows. E.g. if when right-clicking in between columns and not on part of an existing selection, select the entire row before opening right-click menu.

- 🔘 Windows and NTFS: any directory symlink through any mechanism should also allow a junction, preferred over a symlink.
	- The hidden-files half of this item moved up to "Two kinds of hidden file, two options", which asks for the same thing as two switches rather than one.

- 🔘 New flag: `--reset`. Clears bookmarks, resets to default state. (Maybe just delete the config file?)

- 🔘 If the Windows version has never run before, the bookmarks should be cleared, and populated with only the main Windows defaults. (C:\, Desktop, Documents, Downloads, Pictures, Videos, AppData). Also, all linux-specific settings and bookmarks should be cleared on first startup.

- 🔘 Allow '~' in bookmarks to specify home dir (only if at the start and unquoted).
	- 🔘 '~' should work on Windows too.
	- 🔘 Allow environment variables in bookmarks, pathnames, command-line, etc.
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
