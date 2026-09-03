<!-- markdownlint-disable MD007 -- Unordered list indentation -->
<!-- markdownlint-disable MD010 -- No hard tabs -->
<!-- markdownlint-disable MD033 -- No inline html -->
<!-- markdownlint-disable MD055 -- Table pipe style [Expected: leading_and_trailing; Actual: leading_only; Missing trailing pipe] -->
<!-- markdownlint-disable MD041 -- First line in a file should be a top-level heading -->
# Requirements

This is a product backlog just for pre-v1.0.0 release. After that, bugs, features, and enhancements will be managed in Github Issues.

<!-- TOC -->

- [Conventions](#conventions)
- [Backlog](#backlog)
	- [Bugs](#bugs)
	- [Features and enhancements](#features-and-enhancements)
	- [Done](#done)
		- [Done - Bugs](#done---bugs)
		- [Done - Features and enhancements](#done---features-and-enhancements)
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

Each item carries an `Opened:` date as its first sub-bullet, and a `Closed:` date once it is complete or canceled. Format is `YYYYmmDD-HHMMSS`. An item opened and closed on the same day records its open date as `n/a`.

## Backlog

### Bugs

- 🛠️ Startup logs a dozen pairs of "invalid (NULL) pointer instance" / `g_signal_connect_data` criticals on this host. Harmless so far - the window comes up fine - and not tied to the release build; the day-to-day container build does the same thing here.
	- Opened: 20260804-133646
	- Fixed so far: the Windows half of this was the missing resource bundle, and is gone. Whether the host case has the same cause is untested - it was investigated on Linux, where the resources were never dropped.
	- Fixed: the second signature - `g_file_get_child: assertion 'name != NULL'`, one per file listed. Cause: a file's name is not filled in until late in the same update that first applies its info, and the drive-root naming read it early, so every file in the first listing logged one. It also meant a drive root shown as a child kept the bare separator as its name until something refreshed it.
	- Note: a regression check lists a folder and fails on anything logged at warning level or worse.
	- Found: what produces that exact pair is a signal connected to a settings group that is not open yet. The group handles are NULL until the settings are read, and about seventy places connect to one. Reproduced on demand by starting with no session bus, which is what leaves the store unopened.
	- Left to find: why the store is not open that early on the host in the first place. Needs the capture from there.
	- Not reproduced in the build container. None of these produced a single critical: with and without a session bus, with and without the desktop's own settings present (the container has the full cinnamon schema set already), with a home full of bookmarks including missing and remote ones, bare launch and with a location, with and without the desktop flag.
	- Note: it depends on something only the real session has. Needs one capture from the host to place it, and the exact command is in the private notes.

### Features and enhancements

- 🔘 Use new program icon ('[repo]/assets/icon.png')
	- 🔘 Windows .exe
	- 🔘 Linux:
		- 🔘 Desktop launcher and running icon
		- 🔘 Dogfood portion of CICD scripts
		- 🔘 n8runfm bash script.

- 🔘 "Open With": Opening two text files in VSCodium, should open them in the same editor instance. (E.g. as it works when doing so from nemo-anywhere on Linux, or from Explorer on Windows.)
	- Opened: 20260831-164337

- 🔘 Better program icon, for both file .exe and running program. (All supported platforms.)
	- Opened: 20260831-164337

- 🔘 Right-clicking the breadcrumb button for the folder being viewed should offer the same items as right-clicking the empty list background.
	- Opened: 20260826-103001
	- Only that one button. The ancestor buttons keep the shorter menu.
	- Note: the first pass left out the selection and extension submenus (Open With, Copy/Move To, Rename, Duplicate, Create Link, Scripts, Actions), which are tied to the live selection.

- 🔘 Always operate on whole rows in list view.
	- Opened: 20260826-103001
	- Clicking anywhere on a row selects that row, so clicking off the text is not read as a background click - unless it is below everything listed.
	- Right-clicking off an existing selection selects first and then opens the menu, to save a step.

- 🔘 Better thumbnail cache management - a SQLite cache, background pruning, that sort of thing.
	- Opened: 20260826-103001

- 🔘 Real-Windows validation: the two paths still not exercised there.
	- Opened: 20260826-103001
	- The signing path only runs in the hosted release workflow on a tag. The repo has no secrets and no variables set at all, so the signing step is skipped and a release cut today publishes an unsigned exe - the documented fallback, working as intended, but worth knowing before announcing a build.
	- The UAC consent prompt itself has not been seen; this box elevates without prompting and the session is already elevated. What is proven is that the relaunch starts an elevated copy at the right folder, not the consent dialog.

- 🔘 A fractional display scale is only applied to text, so widgets, icons and spacing stay at the whole step below it.
	- Opened: 20260821-150232
	- Falls out of the toolkit scaling in whole numbers. At 150% the type is right and everything around it is a third too small.
	- The way out is our own stylesheet: padding, icon sizes and the like driven from the leftover fraction. Worth doing only once someone has looked at it on a scaled display.

- 🔘 Session bookmarks - that allow you to jump backwards and forwards to folders and/or files
	- Opened: 20260819-141014
	- Need to think through the UX.

- 🔘 Search options: Flat [ ]  Hierarchical [ ]
	- Opened: 20260819-141014

- 🔘 Path button bar should immediately return to buttons, any time the path defocuses, not just 'esc' hit.
	- Opened: 20260802-011216

- 🔘 Hit 'Esc' when focus is in the folder/file pane to completely remove selection. (E.g. to use menu key on background.) Esc again to return it to where it was.
	- Opened: 20260802-011216

- 🔘 In find mode, show the whole path in the status bar rather than just the selected filename.
	- Opened: 20260730-112038

- 🔘 When a value is longer that the column can display, allow a mouseover tooltip to show the whole value.
	- Opened: 20260730-112038
	- Using a reusable tooltip mechanism

- 🔘 Confirm mouse-movement-based actions that don't already ask for some kind of confirmation. (E.g. drag and drop to a new folder)
	- Opened: 20260730-112038
	- Note: a major enhancement to call out in README, e.g.: "Helps prevent one of the biggest pain points with GUI file managers: Accidental file & folder moves, sometimes without realizing it."

- 🔘 New process for each window. A crash in one shouldn't affect all others.
	- Opened: 20260722-172504

- 🔘 Allow moving tabs to other windows.
	- Opened: 20260722-172504

- 🔘 Option to always show a tab.
	- Opened: 20260722-172504

- 🔘 Tabs shouldn't take up the whole space, only what's needed for title (and a reasonable minimum width).
	- Opened: 20260723-133832
	- With a minimum and maximum width, as a percentage, in the settings file.

- 🔘 Target: BSD
	- Opened: 20260730-185314

- 🔘 Target: macOS
	- Opened: 20260730-185314

- 🔘 Advanced file/folder rename functionality.
	- Opened: 20260831-164337
	- Work in search mode too.
	- Needs design work first.
	- Use best of Directory Opus renamer and Thunar renamer.
		- Including wildcard (default) or regex.
		- With variables for various attributes, such as date/time, original name/ext, parent folder name, etc.
	- Advanced dates: Allow obtaining date from various EXIF dates, fallback to date in filename, and final optional fallback, mtime.
		- As already designed for sister camhauler project, including name templates for filenames.
	- Remove Preferences|Behavior|"Bulk rename" option.

- 🛠️ Windows: Need to figure out a way to do GUI testing and demo recording, without interrupting the live console session.
	- Opened: 20260829-071437
	- What works today: a window can be photographed without disturbing anything (it is rendered off-screen, even behind other windows), and most behaviour can be driven through the settings file, which is live-reloaded. Clicks and typing reach the app but take the mouse and the focus while they run.
	- Windows Sandbox is the way: a throwaway Windows built from the host's own image, so no second license, started from a small config file with a shared folder. A logon command inside it runs on its own desktop, which is exactly where the driving script has to be. It keeps no state and cannot reboot, so anything that spans a reboot still wants a Hyper-V guest (Hyper-V is already on; the guest would need an Enterprise evaluation image).
	- The rig is in: `cicd/win/sandbox.ps1` stages a shared folder with the packed exe, generates the config and launches the sandbox; `sandbox-agent.ps1` runs at logon in there and works through queued job scripts, writing logs and screenshots back to the share. `cicd/win/gui.ps1` is the window driver both sides use.
	- First run inside is clean: the app came up with its menus, icons and columns, and the first-run bookmark seeding worked on a profile that had never seen it.
	- Left: demo recording, and anything spanning a reboot (that still wants the Hyper-V guest).

- 🛠️ Enable the disabled pipeline stages as the build matures.
	- Opened: 20260725-153058

- 🔘 Wire the Linux release lane into the pipeline engine itself. Its collector still assumes a bare binary and Cargo-shaped versions, so `RELEASE_ENABLE` stays 0.
	- Opened: 20260804-133646

- 🔘 Linux arm64 release build. Needs an arm64 GTK3 build environment; nothing cross-compiles it today, so the installers' arm64 path has nothing to fetch.
	- Opened: 20260804-133646

- 🔘 Recorded demo of the app in use, generated by the pipeline and skippable on a quick run.
	- Opened: 20260804-230307
	- A short video showing the main features, ~twenty seconds, rendered without a visible display.
	- A looping animation of the same thing for the top of the README.
	- Everything anonymized - no real user name, no distinctive paths.
	- Re-recorded after a noticeable change to the interface or to the demo script.
	- Note: sister projects already have most of the recording machinery to copy from.
	- Follow the 'Automated demo of program use' notes kept with the shared project directives.

- 🔘 Windows code signing, and reducing AV false positives.
	- Opened: 20260804-095855
	- A paid signing service, around $10 a month for 5,000 signatures, is the option on the table now.
	- SignPath Foundation (free for open source) was applied for and refused, so releases ship an unsigned exe with the `.zip` as the fallback. The release-only workflow at `.github/workflows/release-win.yml` still builds, packs and publishes; its submission step is left dormant behind the token gate. Consequence worth remembering: that workflow existed because SignPath would only sign CI-built artifacts, so with it gone nothing forces a release into hosted CI and a local cut is viable again.
	- Options weighed (Azure Artifact Signing, Certum open source, commercial cloud, reapplying) are in `cicd/win/signing.md`.
	- Also sign the release `.zip` contents and, once it exists, the installer. Blocked on there being any signing identity at all.
	- Submit any remaining AV false positives (VirusTotal to find the flagging engines, then vendor FP forms); keep the zip as the FP-free fallback.

### Done

#### Done - Bugs

- ✅ Windows: file copy and paste to another program fails the same way "Copy path" did, and for the same reason.
	- Opened: 20260830-153000
	- Closed: 20260831-081500
	- Found: worse than reported. Over a remote desktop session a copy in nemo did nothing at all - not just for other programs, but for nemo's own paste. The toolkit only advertises a file cut or copy and hands it over when asked; the redirector asks the moment the clipboard changes, does not get an answer in time, and puts the client's own clipboard back.
	- Found as well: even with no remote session in the way, nemo published nothing Windows understands, so Explorer could never have pasted a copy made in nemo. Nor the other way round - a copy made anywhere else offered nothing nemo was looking for, so Paste did nothing.
	- Fixed: a file cut or copy is now written out up front in the formats Windows expects, alongside nemo's own. Paste falls back to the Windows one when nemo's is absent, so a copy made in any program can be pasted, and a cut from one moves rather than copies.
	- Fixed as well: only one program can have the clipboard open at a time, and on a busy machine something usually does for a moment. The call was failing outright every few tries, which read as an empty clipboard. Every use retries now. This affected the text copy too.
	- Proved on a live remote session: a copy in nemo pasted into Explorer, a copy in Explorer pasted into nemo, a cut from either moving rather than copying, and the clipboard emptied after a cut is pasted. The regression check goes red with the fix backed out.

- ✅ Windows network browsing cannot be proved to report a missing network or a refused share.
	- Opened: 20260804-230307
	- Closed: 20260831-071500
	- Fixed so far: the address building and the not-found answer, both verified against real shares. From code review 20260804.
	- Found: the missing-network half never worked. Asking Windows to list the neighborhood on a machine with no network at all succeeds and hands back an empty list, so nemo showed a blank folder that looked like it had loaded. The branch meant to catch it could not fire.
	- Fixed: when the list comes back empty, the machine is asked directly whether it has a network, and "The network is unavailable" is shown when it says no. A list with something in it is left alone - a local provider can offer entries with no network, and the remote desktop channel does.
	- Also fixed: the browse and the lookup used to word the same failure differently, so a refused share came back reading as a missing one. One place decides now, and an answer nobody wrote a case for keeps the system's own words instead of being reworded.
	- Proved on a throwaway machine with its network switched off: with the remote desktop provider present the one entry is listed and nothing is claimed; with it taken out of the order, so there really is nothing, the message appears. The same check goes red on that machine with the fix backed out.
	- Also covered: every failure code's wording, and a server name that cannot exist, which is refused in about a second rather than opening as an empty folder.

- ✅ Windows: opening a file from the released build breaks the program it opens in, unless that program is already running.
	- Opened: 20260830-141048
	- Closed: 20260830-214500
	- Reported against a symlink and VSCodium, which said "The window terminated unexpectedly". The link turned out to have nothing to do with it, and neither did the file: a plain text file does the same.
	- Cause: the single-exe packer is set to share its virtual file system with child processes, so every program opened from nemo starts with the packer's hooks inside it. A program that runs its own sandboxed child processes - anything built on Chromium, which is a lot of desktop software now - cannot start those, and reports a crash. It only shows on a cold start because a second copy of such a program hands the file to the one already running and exits before it gets that far.
	- Reproduced and controlled: a bare test program packed the same way breaks the editor every time; the identical program packed with sharing off opens it every time. The unpacked build is fine, and so is the same launch made by hand.
	- Note: sharing cannot simply be turned off. Nemo's own helpers - the document converters, the thumbnailers and two toolkit helpers - live inside that virtual file system and need it to find their libraries. The fix has to separate "our own helper" from "somebody else's program".
	- Note: a small launcher of our own does not separate them. The hooks follow the whole process tree, not just the first step - measured: a plain helper started by the packed build reports itself hooked, and so does everything it starts. Where the helper sits on disk makes no difference, and neither does building it for the other architecture. Breaking the chain needs the program to be started by something outside our own process tree.
	- Note: no launch flag or shell indirection helps either. Detaching the child, putting a hidden command prompt in the middle, `start /b` behind that, and the shell's own open verb all leave the program hooked. Only a broker outside our own process tree comes out clean.
	- Fixed: a program is now started by one of two brokers rather than by us. The desktop shell is asked first, since it carries arguments, brings the new window forward and is the ordinary way a file gets opened. When it will not do it - an elevated session refuses the call, and there may be no shell running at all - the system's management service does it instead, which keeps the caller's rights but leaves the window behind. A plain start of our own sits behind both, so a launch can still happen on a box where neither broker answers.
	- Also fixed: a file that is not there is refused before the shell is asked. The shell answers a missing file with a message box of its own and does not return until it is dismissed, which would have held nemo's own thread.
	- Measured, packed and unpacked: the six ways of starting a program ourselves all come out hooked, both brokers come out clean, and opening a file from the packed build in a throwaway machine starts the program with the hooks absent.
	- Unpacking to a real folder instead was considered and dropped - it breaks the dogfood launcher's one-file-per-build pool, and it swaps one thing security software dislikes for another.
	- Left open: the slow cold start, which belongs to the packer and is unaffected by any of this.

- ✅ Windows: the released build cannot open a file whose program is 32-bit. Nothing happens, and nothing is reported.
	- Opened: 20260830-161500
	- Closed: 20260830-214500
	- Cause: the single-exe packer is set to leave programs of the other architecture alone, and in practice it stops them starting rather than letting them run unhooked. The call reports success, so nemo has nothing to report either.
	- Measured: a 32-bit program started from a packed build never runs; the same command by hand runs fine. Allowing the other architecture does let it start, but then it carries the packer's hooks like everything else.
	- Fixed by the item above: neither broker is subject to the packer's architecture setting, so a 32-bit program starts and runs unhooked.

- ✅ Windows: a link pointing at a folder was drawn with a file icon instead of a folder icon.
	- Opened: 20260830-141048
	- Closed: 20260830-150000
	- Cause: Windows reports no type at all for a link the listing does not follow, so the toolkit handed back its plain file icon. The folder icon comes off the type, so a folder link got the document one. Both a directory symlink and a junction were affected.
	- Fixed: a link that is a folder is given the folder icon whatever the type came back as.
	- Note: a new check pins the missing-type behaviour the swap exists for, and holds a folder link to the folder icon.

- ✅ "Copy path as" left the clipboard holding whatever was in it before, instead of the path.
	- Opened: 20260830-141048
	- Closed: 20260830-152000
	- Cause: the toolkit only advertises text on the Windows clipboard and hands it over when somebody asks for it. In a remote desktop session the redirector asks straight away, does not get an answer in time, and puts the client's own clipboard back - so the copy read as having done nothing. It affected the plain "Copy path" item too.
	- Fixed: the text goes onto the clipboard up front, so there is nothing left to ask for.
	- Note: a new check reads the clipboard the way another program would, with no message loop running.

- ✅ The context-menu key did not stand in for a right-click.
	- Opened: 20260830-141048
	- Closed: 20260830-151000
	- Cause: the key did open the menu, but the menu placed itself at the mouse pointer - which, for a key press, can be anywhere, including another window or another monitor. It read as the key having done nothing.
	- Fixed: a menu asked for from the keyboard sits against whatever holds the focus. Ctrl+F10 was going the same way and now does too.

- ✅ Windows: a first start with a fresh roaming profile moved the local data folder into the settings folder.
	- Opened: 20260829-081500
	- Closed: 20260829-083500
	- Cause: the move of an old-style settings folder into its roaming home fired on any folder found at the old place. On Windows that place is also where actions, scripts and search helpers are kept, so an ordinary data folder was carried off as if it were old settings.
	- Fixed: only a folder holding a settings file is moved. The data folder stays where it is.

- ✅ Often when right-clicking on the breadcrumb buttons, the menu closes immediately and has to be right-clicked again.
	- Opened: 20260802-095853
	- Closed: 20260828-164500
	- Fixed: by the path-button menu work. The menu opens inside the press itself now, rather than after an attribute load that could finish late.
	- Verified on Windows: eight right-clicks in a row, the menu up and staying up every time.

- ✅ Dragging a file towards another application crashed the app, before it had even left the window.
	- Opened: 20260828
	- Closed: 20260828-163000
	- Reproduced: nothing to do with the other application. Any drag that passed over the empty space below the last row did it, which a drag out of the window does on its way.
	- Cause: the toolkit is asked which row sits under the pointer. Past the last row it answers "none" without filling in the row it was handed, and that leftover value was then read and released.
	- Fixed: the row is only read when the toolkit really filled it in. A new check asks the same question at a position below the rows.

- ✅ Search doesn't fully work.
	- Opened: 20260826-103001
	- Closed: 20260828-160000
	- Note: three faults, all on Windows. Searching by name already worked, and still does - substring, wildcards, a regex, and the switch that keeps the search out of subfolders.
	- Fixed: "Containing:" found nothing at all, ever. Windows calls the extension the file's type, so the test for "is this text" answered no for every file. It converts first now, and where the extension means nothing to Windows it decides from the first few kilobytes instead. A file with no extension is searched, and a binary one is left alone.
	- Fixed: pressing Enter straight after typing did nothing, and left the box outlined in red. The check that decides whether a search may run at all is on a short delay, and Enter threw it away rather than waiting for it.
	- Fixed: a search with only a "Containing:" pattern and no name crashed outright. Nothing typed in the name box means every name, which is what it now says.
	- Note: left open as its own item - nothing that needs a helper program (documents, spreadsheets, PDFs) can be searched on Windows, because none of the helpers are packaged there.

- ✅ The settings schema shipped for `shcl check` is kept in step with the key table in the code by hand, and nothing notices when it drifts.
	- Opened: 20260821-144459
	- Closed: 20260826-180755
	- Cause: two files have to be edited for every new setting. Miss the second and a hand-edited config validates against a schema that does not know the key.
	- Reproduced: turned up while adding two settings at once.
	- Fixed: a test walks both and fails on any name, type, allowed set or default that does not line up. It reads the real key table rather than the source text, so the macro-named keys and the per-platform ones are all covered.
	- Note: it found 13 real mismatches on its first run: 8 settings the schema had never heard of, two archive command lines missing the thread count, the two list-view column lists missing the extension column, and the sidebar width. All corrected.

- ✅ A leftover helper from the install folder blocks uninstall and in-place upgrade, and the message blames the app.
	- Opened: 20260818-155550
	- Closed: 20260828-134500
	- Cause: the session bus the app autolaunches lives in the install folder and outlives the window, so the in-use check still sees the folder busy. It says "Nemo Anywhere is still running", which reads as wrong to someone who just closed it.
	- Reproduced: uninstall failed on the installer round trip, then succeeded a few seconds later with nothing else changed.
	- Note: wants either a wait-and-retry, or a message that names what is actually holding the folder.
	- Fixed: both. The installer waits up to ten seconds, says what it is waiting on, and if it gives up names the executables actually holding the folder instead of the app.
	- Verified on Windows against a scratch install, which turned up three more faults, all fixed:
		- An in-place upgrade died outright. The in-use check hands back an empty list as nothing at all, and asking that for a count is an error, so every upgrade over an existing folder failed before it started. Only a first install had ever been run.
		- The check compared two spellings of the same folder and so found nothing. It long-forms the folder it was given but takes a running program's path as reported, and those two do not have to agree.
		- If the swap failed anyway, for a reason no process scan can see, the failure came out as a raw runtime error. It now says which folder is stuck and what to do.
	- Verified: the round trip is clean - install, run, close, upgrade over it, uninstall. The PATH comes back byte for byte and nothing is left behind.

- ✅ The installer leaves the user PATH very slightly different from how it found it.
	- Opened: 20260818-155550
	- Closed: 20260826-180755
	- Cause: adding then removing the entry also drops a pre-existing trailing separator, so an install/uninstall round trip is not byte-identical. Harmless - an empty trailing entry means nothing - but it is a change nobody asked for.
	- Fixed: both halves carry the trailing separator through, so what an uninstall writes back is what the install found.
	- Verified against an empty PATH, one with a trailing separator and one without.

- ✅ Listing a folder whose path is past 260 characters quietly lists a different folder instead - whichever one the program happens to be running from.
	- Opened: 20260821-150232
	- Closed: 20260828-114000
	- Found while proving the long-path manifest work. The toolkit's own directory walk is what breaks; every other call on the same path is right, which is why nothing showed up until a folder that deep was actually opened.
	- In the window it reads as an empty folder, because each name it hands back is then checked against the folder that was asked for and none of them are in it. That is the harmless case. The one to worry about is search, which walks folders itself and would follow the wrong tree.
	- Reproduced three ways: the failing call from two different working directories returns the contents of each in turn, while the platform's own call on the same path returns the right thing.
	- The walk is now done here on Windows once a path is long enough that the toolkit cannot be trusted with it. Everything shorter still goes straight to the toolkit, so the ordinary case is untouched.
	- One entry point covers the lot: the file listing, search, copy, move, delete, the deep count and the archive scan all go through it.
	- A folder 308 characters deep lists its real contents in the window now, with sizes, types and dates. New checks cover it both ways round.

- ✅ Switching the path separator to `/` does not take effect until the folder is revisited.
	- Opened: 20260826-103001
	- Closed: 20260828-124500
	- Note: the rest of the Paths group on the Display page applies straight away, so this one is the odd man out.
	- Cause: the title, the location entry and the breadcrumb are only rebuilt on a location or view change, so changing how a path is spelled never asked for one.
	- Fixed: all three now refresh as soon as the setting changes. Verified on Linux against the full-path title, which lags the same way.
	- Two more halves showed up on Windows, where the separator can really change. The breadcrumb was redrawing a step behind - it read the separator before the setting had been taken in. And the sidebar, which spells a drive root as `C:\`, was not redrawing at all.
	- Both fixed. Title, breadcrumb, location bar and sidebar now all move together the moment the setting changes, with no navigation. A new check covers the ordering.

- ✅ "Show the full path in the title bar and tab bars" does nothing.
	- Opened: 20260826-103001
	- Closed: 20260826-180755
	- Note: on the Display page, under Windows and Tab Titles. Turning it on leaves the window title and the tabs showing the folder name only.
	- Two causes. The title is only recomputed on a location or view change, so the setting did nothing until the next navigation. And the home folder answered "Home" before the setting was ever read, so in the one place most people would try it, it did nothing at all.
	- Both fixed. The home folder now gives way to the setting, and the title, the tabs and the location widgets all refresh the moment it changes.

- ✅ The first folder listed after launch is still slow when the start location is full of links.
	- Opened: 20260828-083458
	- Closed: 20260828-090000
	- Note: same folder and same symptom as the item below, which fixed only the first of two causes. The listing itself now appears in about three seconds; what came after it took another ~minute.
	- Cause: two things chased each link off the machine, one at a time, with the folder waiting: counting a folder's items, and asking what a share is mounted under. One link to a host that is not answering cost a lot of time per query.
	- Fixed: the preference for item counts already says "local only" by default, but a share is native as far as the toolkit is concerned, so nothing ever held it back. Both questions are now skipped for anything on a share, or any link pointing at one.
	- Fixed: the mount question is skipped outright there. A share is not a mount on Windows, so the answer was never of use.
	- Verified against a host that really was not answering: over a minute before, about three seconds after. The item count for such a folder now reads "--", which is what the preference has always meant.

- ✅ The first folder listed after launch takes a very long time when the start location is full of links.
	- Opened: 20260827-183930
	- Closed: 20260827-193152
	- Seen launching straight into a folder of shortcuts and junctions. Folders opened after that are normal, so it is the first listing that pays.
	- Measured: one link pointing at a share that is not answering costs ~20s, and the whole listing waits for it. The folder in question has one, and took over a minute to show anything at all. It is only slow the first time because Windows remembers the failure for a while afterwards.
	- Cause: listing a folder asked for each child's details with links followed, so every reparse point was chased to whatever it pointed at, over the network if that is where it led.
	- Windows puts the directory bit on the link itself, so the type still comes out right without the trip. The listing no longer follows them there, and the same folder now appears in about a second.
	- Trade: a link to a file reports the link's own size rather than the target's, and a link whose target is gone no longer shows as broken until it is opened.
	- The drive-root test was reading the real config while it ran, so it failed on any machine where the forward slash had been chosen. It gets its own throwaway config now.

- ✅ Deleting to the trash puts the progress popup on top of the confirmation prompt.
	- Opened: 20260827-183930
	- Closed: 20260827-191128
	- Note: the yes/no dialog is behind it, so the delete reads as stuck until the popup is dragged out of the way.
	- Cause: the prompt was the Windows shell's, not ours. GLib's trash call leaves the shell confirmation switched on, so every file was asked about twice and the second dialog was not one we could place.
	- Fixed: a delete goes to the Recycle Bin through the shell directly with the confirmations off, so our own prompt is the only one and nothing covers it. Verified on Windows end to end.
	- Note: the trash test drops its private copy of the same code and calls the shipped one, and its timeout goes to ten minutes - a full recycle bin can take four and a half.

- ✅ The Win32 argument quoting check depends on what is installed on the machine.
	- Opened: 20260828-083458
	- Closed: 20260828-090000
	- Cause: it split `wt.exe` and expected the name back unchanged, but a box with Windows Terminal installed resolves it to a full path, so the check failed there and nowhere else.
	- Fixed: it uses a name that cannot be on the path. The case where a program is found is still covered, by the check below it that looks one up first.

- ✅ The config schema check goes red on a fresh Windows checkout.
	- Opened: 20260828-083458
	- Closed: 20260828-090000
	- Cause: git checks the schema out with Windows line endings, and the check split it on newlines only, so every field name carried a stray carriage return and matched nothing. It then reported all 169 settings as missing from the schema.
	- Note: the config parser itself was never affected - it treats a carriage return as whitespace. Only the check's own reader did.

- ✅ In dark mode the breadcrumb bar and the checked view buttons kept a light background, unreadable against everything around them.
	- Opened: n/a
	- Closed: 20260819-141014
	- Cause: a bundled theme is loaded as a stylesheet of our own, but the theme *name* was left pointing at it. GTK cannot resolve a name it has never seen on disk, falls back to its packaged sheet, and drops the dark half while doing so - so the layer under ours was the light one. Anything our sheet did not itself paint showed it through.
	- Fixed: the name now points at a theme GTK really has, so the base follows light/dark while our sheet sits on top. Verified against both the light and the dark base.
	- Also fixed alongside: choosing a theme that cannot be found left the previous one on screen, so a bad name looked like nothing had happened.

- ✅ The three view buttons at the bottom left drew as broken-image placeholders.
	- Opened: n/a
	- Closed: 20260819-141014
	- Cause: none of the app's own artwork was in the Windows bundle at all. Only the toolkit's icons were packaged, so every one of our own icon names missed - the location button in the toolbar was the same failure.
	- Fixed: the app's artwork now rides inside the executable, the same way the bundled themes do. Costs no extra files, so nothing is added to startup time, and it works on every platform including a relocated install.

- ✅ The theme picker offered "macOS" and "Windows 10" twice in dark mode, and one of each was the light theme.
	- Opened: n/a
	- Closed: 20260819-141014
	- Cause: those two themes ship a dark sheet of their own upstream *and* have a separately drawn dark half that we also bundle, so both halves claimed dark.
	- Fixed: where a light/dark pair is named, the pair wins and the redundant sheet is dropped. A theme that states which modes it suits is no longer second-guessed either, so a hand-dropped theme cannot bring the fault back.

- ✅ On Windows a drive root is named `\` everywhere except the sidebar - the window title reads `\` and the breadcrumb reads `(C:) Windows` while the sidebar has `Windows (C:)`. Seen on this box browsing `C:\`.
	- Opened: n/a
	- Closed: 20260818-155550
	- The volume-label work only ever covered the sidebar, and it built its own name there. Everywhere else falls back to what Windows reports for a drive root, which is a bare separator.
	- Three different sources were in play: the basename, which is `\` for every drive alike; the volume monitor, which says `(C:) Windows`; and the sidebar's own string.
	- Fixed: a drive root is `C:\` everywhere - title, breadcrumb and sidebar all ask the same helper. The volume label moved to the sidebar tooltip, where it cannot be mistaken for the path.
	- Verified on Windows: a new test covers the naming, including that the first folder inside a drive keeps its own name; and all three surfaces agree.

- ✅ "Set as default" in the Open With tab did nothing on Windows, and said nothing either.
	- Opened: n/a
	- Closed: 20260818-155550
	- Cause: Windows keeps the per-user default behind a hash it will not let a program write, so the call fails outright - and the result was thrown away along with the error.
	- Fixed: the failure is reported. The choice still cannot be made on Windows; the difference is the user is told rather than left thinking it worked.
	- Verified on Windows: the underlying call refuses with "Setting default applications not supported yet". Looking a default up still works, but only by extension - asking by mime type answers nothing.

- ✅ The action layout editor never opens: the app spawns it as `nemo-action-layout-editor`, but the binary installs under the app slug as `nemo-anywhere-action-layout-editor`. One missed rename from the rebrand.
	- Opened: 20260804-133646
	- Closed: 20260818-103142
	- Fixed: it is spawned under the app slug, out of the folder the app itself was started from, and a failure to start now says so instead of doing nothing.
	- Also found and fixed alongside: the Restart button in extension settings was quitting and starting whichever upstream Nemo happened to be installed, not this app.

- ✅ The Windows build shipped without its compiled-in resources, so it had no menu bar at all and every `.ui`, `.glade` and `.css` lookup failed.
	- Opened: n/a
	- Closed: 20260818-155550
	- Cause: the resource bundle is attached to the extension library. On Linux that is a shared library and the whole thing loads, so the resources register themselves. On Windows it is a static one, and the linker keeps only the members that resolve a symbol - the resources register from a constructor nothing calls by name, so the object was dropped.
	- Nobody noticed because the app still starts and browses: the missing menu bar reads as a design choice, and the fallout was a wall of criticals that had been written off as noise.
	- Fixed: on Windows the resources go straight into the executable. Linux keeps them in the shared library as before.
	- Verified on Windows: the menu bar is back, and startup criticals went from 40 to 9 - none of the remainder about resources or widgets.

- ✅ The Windows executable was not marked long-path aware, so anything past the old 260-character limit was out of reach even with long paths switched on.
	- Opened: 20260818-142740
	- Closed: 20260826-103001
	- Cause: the exe carried no application manifest, which is where that is declared.
	- Fixed: the manifest arrived with the DPI work. Measured on a 427-character folder holding a 462-character file: without the manifest every call failed outright; with it, reading the file, asking for its details, testing that it exists and walking into the folder all work.
	- Note: listing such a folder is still wrong, and worse than a failure - it is its own bug, still open.

- ✅ Code review 20260815.
	- Opened: 20260815-154746
	- Closed: 20260817-210917
	- Full code, security and performance review of the whole tree, first-party and inherited. Everything below was re-checked before it went in, worst first. Technical detail is kept out of this file. Numbers are continuous and match the private detail notes.
	- ✅ High.
		- ✅ Item 1. Windows trash acts on file paths from the address with no check that they belong to the recycle bin.
			- Cause: delete, move and read take the path straight from a `trash:///` address, so a crafted address can read or permanently delete any file.
			- Cause: the one place that does check compares the text as it was typed, so a `..` inside a bin item's path walks back out of the bin.
			- Fixed: a path from a trash address is resolved to its real form and has to name something the recycle bin actually holds before it is read, moved or deleted.
			- Verified on Windows: an address aimed at a file outside the bin, and one walked back out of the bin, are each refused for delete, move and read, and the file is left where it was.

		- ✅ Item 2. Reading dragged icon-list data can walk off the end of the buffer.
			- Cause: one branch of the parser skips the length bookkeeping every other branch does, and its end-of-data guard tests something that can never be empty, so the scan runs past the buffer.
			- Fixed: the length is kept up to date on that branch too, and the guard tests the data rather than the pointer.
			- Note: inherited from upstream. Covered by a new check.

		- ✅ Item 3. "Open in Terminal" crashes when no known terminal is installed.
			- Cause: with nothing found the command prefix is left empty and used anyway. Likely on a minimal or KDE-only box, which is exactly the de-Cinnamon target.
			- Fixed: with nothing found the caller declines instead of going ahead. Covered by a new check.

		- ✅ Item 4. Freeing an extension column object corrupts the heap.
			- Cause: teardown frees memory the type system owns. Latent only because built columns are kept for the life of the process; any extension that discards one hits it.
			- Fixed: it no longer frees what it does not own. Covered by a new check.

		- ✅ Item 5. An unreadable settings file is treated as empty, and a queued save can then erase it.
			- Cause: any read failure - a sync, antivirus or editor lock, or the delete half of someone else's non-atomic save - loads defaults into memory, and a save already queued then writes that near-empty document over the real file.
			- Fixed: a failed read keeps what is already in memory. Only a file that is genuinely absent goes back to defaults. Covered by a new check.

		- ✅ Item 6. A NUL byte anywhere in the settings file truncates it on the next save.
			- Cause: the file is written by text length, which stops at the first NUL and drops every setting after it. The check that follows the write is fooled the same way, so the loss goes unnoticed.
			- Fixed: the write and the check both count bytes. Covered by a new check.

		- ✅ Item 7. The thumbnail enable-check reads the disabled-types list without its lock.
			- Cause: one reader skips the lock the writers and the other reader take, so a settings change on another thread can free the list mid-read.
			- Fixed: that reader takes the lock too, and the inner call now says it expects its caller to hold it.
			- Note: a threading race, so there is no check that would fail reliably.

		- ✅ Item 8. Replacing a folder deletes through directory symlinks inside it.
			- Cause: the recursive remove never checks what each child is, so a link to another folder is followed and its contents deleted, outside the folder that was agreed to.
			- Fixed: only real folders are recursed into; everything else is removed as itself.
			- Note: the premise has a check of its own. The code path itself sits behind a modal Replace dialog.

		- ✅ Item 9. An invalid filename search pattern crashes the search.
			- Cause: a pattern that fails to compile leaves nothing to match with, but the search runs anyway and then releases an uninitialised result for every file. Reachable by pressing Enter before the typing check catches up.
			- Fixed: a pattern that will not compile matches nothing rather than running on. Covered by a new check.

		- ✅ Item 10. Restoring an item from the Windows trash drops its file extension.
			- Cause: the original name is taken from the shell display name, which hides known extensions by default, and that shortened name is what restore writes.
			- Fixed: the real extension is taken from the backing file, so the listed name and the restored name both keep it.
			- Verified on Windows: a recycled file is found under its full name, reports the original location it came from, and restores to it.
			- Note: the cause does not reproduce on Windows 11. With "hide extensions for known file types" switched on, the recycle bin still reports full names, in this app and at any setting. So the repair is kept for older Windows rather than being load-bearing here.
			- Note: corrected while checking - it used to give up on any name containing a dot, so `report.2026.txt` would have been repaired to `report.2026`.

		- ✅ Item 11. Opening certain images can crash if the tab is closed first.
			- Cause: the image-viewer sort path reads the tab it came from with no check, and that is cleared when the tab closes mid-open. This is the ordinary double-click-an-image path on Mint-family setups.
			- Fixed: guarded. The image still opens, without the wrap-around through the rest of the folder.
			- Note: an asynchronous path through the interface, so no check of its own.

		- ✅ Item 12. The places sidebar keeps reacting to settings after it is destroyed.
			- Cause: two preference handlers are left connected at teardown, so a later settings change - including a live edit of the settings file - fires on freed memory. Triggered by hiding the sidebar or switching to the tree sidebar.
			- Fixed: both are disconnected at teardown.

		- ✅ Item 13. New Folder in the tree sidebar aborts the app when creation fails.
			- Cause: the callback ignores the failure and passes nothing on, which aborts. A permission race or a dismissed error dialog triggers it.
			- Fixed: it gives up on failure, the way the twin in the folder view already did.

		- ✅ Item 14. Jumping more than one step forward corrupts the history lists.
			- Cause: the transfer loop reads one list but edits the other two, so the back and forward lists end up sharing entries, and a later navigation frees ones still in use.
			- Fixed: the entry is taken off the forward list and put on the back list, mirroring how going back already worked.

		- ✅ Item 15. On Windows every file reports as changed on every refresh.
			- Cause: the per-type icon is compared against the plain system icon, which never matches, so each refresh marks the whole folder changed and re-sorts, redraws and re-checks thumbnails, with a registry lookup per file on top.
			- Fixed: the icon is judged on where it ends up rather than mid-update, so a refresh no longer reports every file as changed.
			- Verified on Windows: five refreshes over real files of several types, with everything after the first sighting reporting nothing changed, and a real change still coming through.

		- ✅ Item 16. Sidebar rebuilds block the whole window on filesystem queries.
			- Cause: free-space and drive-type checks run on the interface thread for every drive and mount, on every rebuild. A slow or hung mount freezes the window, and a mount change is often what triggers the rebuild.
			- Fixed: the free-space answer is cached per sidebar, and the rebuild only ever reads the cache, so it never waits. A missing or stale entry starts a query off the interface thread that fills the cache and asks for one rebuild afterwards. A hung mount leaves one entry pending and blocks nothing.
	- ✅ Medium.
		- ✅ Item 17. The code-signing password is passed on the command line, visible to other local processes.
			- Fixed: the certificate is imported and signed by fingerprint, so the password never appears on a command line another process can read.

		- ✅ Item 18. The Windows sysroot packages are downloaded and unpacked with no integrity check, and those libraries ship in the release.
			- Cause: neither the database signature nor the per-package checksum is verified, though the checksum sits in data the fetcher already parses.
			- Fixed: every package is checked against the checksum the database already carries, and a mismatch stops the build.

		- ✅ Item 19. A malformed D-Bus Open hint from any local process crashes the running app.
			- Cause: a hint with no `=` in it yields nothing, and that is parsed without a check.
			- Fixed: guarded.

		- ✅ Item 20. A pathological settings file can kill the app during parse.
			- Cause: the file is read with no size cap, the parser keeps every decoded byte for the document's life, and a failed allocation ends the whole process from inside the library.
			- Fixed: an 8 MiB read cap. An oversized file is refused and what is already in memory is kept. Covered by a new check.

		- ✅ Item 21. In the Windows pipeline, an abort between stash and pop strands the working changes, and a rerun can commit conflict markers.
			- Fixed: a conflicting restore stops and says where the work is and how to get it back, instead of leaving a rerun to commit a half-merged tree.

		- ✅ Item 22. In cicd.bash, a remote-sync stash-pop conflict aborts with no guidance and the stash still held.
			- Note: the natural rerun with sync off then builds and publishes a tree missing the stashed changes.
			- Fixed: same as above - it stops with the stash named and the two ways out spelled out.

		- ✅ Item 23. The version-bump guard blocks the beta-to-final release push.
			- Cause: version sort puts `1.0.0` before `1.0.0-beta2`, the reverse of release order, so cutting final over the current beta fails the guard. That exact transition is next.
			- Fixed: a prerelease is made to sort below its release, the way the packaging script already did it.

		- ✅ Item 24. Accessibility paste reads a freed stack value.
			- Cause: a stack value is handed to a clipboard callback that runs after the function has returned.
			- Fixed: it is allocated to last, and released in the callback.

		- ✅ Item 25. install.bash deletes the existing install before the replacement is in place.
			- Cause: a cross-filesystem move that fails partway leaves nothing installed, and the temporary copy is then wiped on abort.
			- Fixed: the replacement is staged beside the existing install and swapped in, with a rollback on failure, so the old one is only dropped once the new one is there.

		- ✅ Item 26. install.ps1 can half-delete a running install.
			- Cause: a process whose path cannot be read is treated as not running, so the delete goes ahead against a locked copy and throws partway.
			- Fixed: the same stage-beside-then-swap as the bash installer, and the in-use check reads paths in a way that covers protected and cross-session processes.
			- Note: Windows file-locking edge cases still want the real-Windows pass.

		- ✅ Item 27. A partial extension crashes every location load.
			- Cause: one provider dispatch skips the guard its siblings have, so an extension that leaves the function unset is called through nothing.
			- Fixed: guarded, the way the column provider already was.

		- ✅ Item 28. An action's exec condition decides on an uninitialized value when the spawn fails.
			- Cause: a missing program or a parse error leaves the result unset, so menu visibility is decided by whatever happened to be on the stack.
			- Fixed: the result is seeded, and a failed spawn answers no.

		- ✅ Item 29. Actions stored in a path with spaces run the wrong command.
			- Cause: the action directory is put in front of the command unquoted, and the whole thing is then split on whitespace. Normal on Windows, and on Linux homes with spaces.
			- Fixed: the directory and its separator are quoted as one word, which also settles the Windows separator.

		- ✅ Item 30. Any drag-and-drop clears a pending cut or copy.
			- Cause: the collision check compares the dragged list against itself, so it always matches and always clears the clipboard.
			- Fixed: it searches the clipboard instead.

		- ✅ Item 31. The settings-groups table is read from worker threads and grown on the main thread with no lock.
			- Cause: a lazy insert can grow the table while a worker thread is reading it. A narrow window, but memory-unsafe.
			- Fixed: the lookup and the insert are both under the lock. Announcing a change still happens outside it, so a handler can come back in.

		- ✅ Item 32. The favorites change-timer id is touched from worker threads without a lock.
			- Cause: a worker can remove a timer the main thread has already reused, silently killing an unrelated one.
			- Fixed: the timer is taken under the lock that already covers the rest of that structure.

		- ✅ Item 33. Two favorites with the same name in same-named parents collide.
			- Cause: disambiguation appends only the parent's name, and the displayed name is the favorite's identity, so an operation on one can hit the other.
			- Fixed: the parent path is used, shortened, with a counter behind it so the name is always unique.

		- ✅ Item 34. Trashing a file drops favorites of unrelated sibling paths.
			- Cause: the removal matches by raw prefix with no path boundary, so trashing `ab` also drops the favorite for `abc.txt`.
			- Fixed: the match has to land on a separator or be exact.

		- ✅ Item 35. The mount lookup matches sibling paths by prefix.
			- Cause: the same missing boundary check, so a path can be matched to the wrong mount and then called local when it is not.
			- Fixed: the same boundary guard.

		- ✅ Item 36. Successful direct-save drops are reported as failed.
			- Cause: the success branch repeats the test the fallback branch makes, so it can never run and a saved file is reported as a failed drop.
			- Fixed: it tests for success.

		- ✅ Item 37. A failed metadata save is silent and throws away the pending metadata.
			- Cause: the write error is ignored and the data marked saved, so it is never written again and is lost on restart.
			- Fixed: the write is checked, it is only marked saved when it worked, and a failure is reported.

		- ✅ Item 38. Large-zoom images render blurry on Windows.
			- Cause: the can-load check misses the content-type conversion the rest of the code does, so the full-resolution path never runs.
			- Fixed: the check converts the type first, so the full-resolution path runs.
			- Verified on Windows: real images are accepted by the internal-thumbnail check and text is still refused.
			- Note: the stored type for a `.png` on Windows really is ".png", which is why the conversion is needed at all.

		- ✅ Item 39. A trashed folder whose status can't be read is shown as a healthy file.
			- Cause: the fallback invents a regular-file entry without looking at the error, and an item deleted behind the app's back still lists as existing until the next full refresh.
			- Fixed: a folder is shown as a folder, and something that has gone is no longer presented as readable.
			- Verified on Windows: an item removed from the bin behind the backend's back comes back saying outright that it cannot be read.
			- Note: the folder half is covered only by a live trashed folder listing as a folder. Forcing a folder that is present but unreadable was not attempted.

		- ✅ Item 40. Freshly trashed items get a wrong parent until the next poll.
			- Cause: the top-level check does not refresh on a miss, unlike the sibling lookup, so an item not yet seen is filed under a parent that is not in the bin at all.
			- Fixed: the top-level check refreshes on a miss, like the sibling lookup.
			- Verified on Windows: a freshly recycled file reports the bin root as its parent.

		- ✅ Item 41. The bookmarks window's no-selection guard never fires and can abort.
			- Cause: an unsigned row number holds what should be a -1, so the guard is dead and an assert or a wrapped index is reachable.
			- Fixed: the row number is signed, so the guard works.

		- ✅ Item 42. A failed or empty drop on the .desktop launcher editor crashes.
			- Cause: both drag handlers split the data and read the first piece with no length check.
			- Fixed: an empty or failed drop is guarded in both.

		- ✅ Item 43. Rename-pending activation relies on a garbage return value and leaks the selection each tick.
			- Cause: a function that returns nothing is installed as a repeating timer, and the still-renaming early return does not free the selection it fetched.
			- Fixed: a proper wrapper decides whether to repeat, and the selection is freed on that path.

		- ✅ Item 44. Two invalid search patterns warn fatally and show the wrong message.
			- Cause: the content check is handed an error the filename check already set.
			- Fixed: the error is cleared between the two.

		- ✅ Item 45. Tree-sidebar Paste races a freed file and holds a stale view pointer.
			- Cause: the clipboard request keeps no hold on the view, and an idle can free the target first. Paste from another program degrades to nothing, and a closed sidebar leaves a dangling pointer.
			- Fixed: the view is held for the length of the request, and the reply guards against the target having gone.

		- ✅ Item 46. The script debug log reads a path after freeing it.
			- Cause: the path is freed just before the line that prints it. Fires with the folder-view debug output turned on.
			- Fixed: it is freed after.

		- ✅ Item 47. The failed-home fallback reopens the failing location instead of root.
			- Cause: the root fallback is built and never used, so an unreadable home retries itself in a loop. The hardcoded root also resolves to the current drive on Windows.
			- Fixed: it opens root, so an undisplayable home stops retrying.

		- ✅ Item 48. The Windows trash test writes past a buffer.
			- Cause: a 64-bit length is written through a 32-bit pointer, so half of it is stack garbage that then sizes and indexes a buffer.
			- Fixed: the length is taken at the right width.
			- Note: the trash test used to report itself skipped on this box whatever it had done. It works the recycle bin directly now and reports a real result, so this code runs natively on every run.

		- ✅ Item 49. The dogfood launcher mangles pass-through arguments containing quotes or trailing backslashes.
			- Cause: the launcher joins its arguments with a plain space and the target splits them again, so quotes, backslashes and even plainly spaced arguments were lost.
			- Fixed: every argument is quoted the way the Windows runtime expects, and the shell round trip passes them through untouched.
			- Verified on Linux: arguments carrying spaces, quotes and a trailing backslash all arrive as written.

		- ✅ Item 50. Typing a UNC path blocks the whole window on a network probe.
			- Cause: the backslash-to-slash retry does its existence checks on the interface thread, so an unreachable host stalls for the whole network timeout before the location even opens.
			- Fixed: input that is structurally a `\\host\share` skips the check and goes straight to the asynchronous load.

		- ✅ Item 51. Failed thumbnails are re-decoded on every icon fetch.
			- Cause: the app records a failure under its own name, which the system's failed flag never reads, so every failed file re-reads and re-decodes an image on each fetch. In list view that is once per row per draw.
			- Fixed: the negative answer is cached per file, and cleared when the file changes so a changed file is tried again.

		- ✅ Item 52. Content search buffers whole files into memory with no cap.
			- Cause: each candidate text file is read whole, then copied again to check and strip, so a multi-gigabyte file can freeze the search or exhaust memory.
			- Fixed: the per-file read is capped at 16 MB.

		- ✅ Item 53. The list view rebuilds and rescales each icon on every row draw.
			- Cause: the icon, its emblems and a fresh surface are assembled with no caching, and thumbnails are rescaled every time, so any redraw re-does the work for every visible row.
			- Fixed: the rendered row is cached and reused across draws, and dropped when the file changes. Drag and cut highlighting still draw live.

		- ✅ Item 54. The list view re-invalidates visible thumbnails on every scroll pause.
			- Cause: an already-loaded flag is read and then ignored, so every visible file's thumbnail and extension details are re-read at each scroll settle.
			- Fixed: the work happens once, when a row first comes into view, matching the icon view's twin.
	- ✅ Low.
		- Terse by design; file and mechanism are in the private detail notes. All confirmed on read, minor impact or rare paths, mostly inherited.
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
			- Verified on Windows: swapping one bin item for a much larger one is reported, while a quiet spell is not.
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
			- Fixed: it builds its own files, copies them, and checks the result.

		- ✅ Item 98. The editable-label test is not wired into any build, so it never runs.
			- Removed: it was an interactive demo with no build wiring behind it.

		- ✅ Item 99. The config test never makes warnings fatal, so its negative checks cannot fail.
			- Fixed: an unexpected complaint now fails the run.

		- ✅ Item 100. The favorites test never removes its temp directories.
			- Fixed: the temp tree is removed.

		- ✅ Item 102. The row-under-pointer helper leaks a tree path on every call (per drag-motion).
			- Fixed: released.

		- ✅ Item 103. The extension simple-button leaks a surface and can use an uninitialized size.
			- Fixed: released, and the size is seeded so an unknown icon size cannot be read before it is set.

		- ✅ Item 104. Every settings save leaks a full copy of the file into the parser arena.
			- Fixed: the settings document is rebuilt from its own canonical form when it has handed out enough, so the memory comes back.

		- ✅ Item 105. A move leaks the source's parent object on every non-desktop move.
			- Fixed: released.

		- ✅ Item 107. Thumbnail creation falls back to a synchronous stat on the main thread.
			- Fixed: the fallback lookup happens on the worker instead of the main loop.

		- ✅ Item 108. Every mouse-motion event rewrites the whole sidebar tree store.
			- Fixed: only rows that actually change are touched.

- ✅ Code review 20260804.
	- Opened: 20260804-230307
	- Closed: 20260817-210917
	- Full review of everything written or changed since the fork point. Ordered roughly most serious first. Technical detail is kept out of this file.
	- ✅ Item 1. Every dropdown and radio choice in Settings saved the wrong value.
		- Cause: the settings layer stored the choice by number, but the dialog only ever supplied the name, leaving the number at zero. Whatever was picked, the first option was saved.
		- Note: worst case was "Executable text files", where the first option is "run it" - so any visit to that setting quietly armed scripts to run on double click.
		- Fixed: choices are now saved by name. Regression test added, and confirmed to fail before the fix.

	- ✅ Item 2. The settings file grew a duplicate comment line on every write.
		- Cause: setting a comment appends a line rather than replacing one, and the comment was re-applied on every save.
		- Note: the window size is saved shortly after every move or resize, so a session of dragging the window added dozens of identical lines, and they survived restarts.
		- Fixed: the comment is written only when a setting first appears in the file.

	- ✅ Item 3. Hand-editing a setting that was already in the file did nothing until restart.
		- Cause: the live-reload comparison could only see a setting appear or disappear, never change, so nothing was announced to the app.
		- Fixed: the comparison now reads the values themselves.

	- ✅ Item 4. "Make Link" on Windows can destroy an existing file, and can crash.
		- Cause: the shortcut is saved over whatever is already there instead of reporting the clash, so the usual "another link to..." renaming never happens.
		- Cause: dropping a link onto a location that is not a real folder returns a failure with no message attached, and reading that message crashes.
		- Fixed: creating a shortcut now refuses to write over anything already at that name and reports the clash, so the existing renaming retry takes over.
		- Fixed: a link dropped somewhere with no real folder behind it now says so instead of failing silently into a crash.
		- Verified: a file sitting at the name a new shortcut would take survives, and the clash is reported.
		- Note: the shortcut test was failing two checks before any of this, on a correct product - it compared a short-form temporary path against the long form the system reports. Fixed alongside.

	- ✅ Item 5. Repairing the thumbnail cache as an administrator can change ownership of unrelated files.
		- Cause: the repair walks symbolic links instead of skipping them, and changes ownership of whatever they point at.
		- Note: the app itself suggests running this with administrator rights, so an unprivileged process could aim it at system files.
		- Fixed: the repair acts on the link itself instead of following it, so a link planted in the cache can no longer hand away the file it points at.

	- ✅ Item 6. Favorites can hang the app or read freed memory.
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
		- Verified: the listing always finishes, concurrent reads are safe, and a missing target is skipped rather than ending the listing.
		- Note: settings written by older versions keep working - only the write order changed, and both are read.

	- ✅ Item 7. Favorites and thumbnails keep working after the object they belong to is gone.
		- Cause: both release a shared settings object they never owned.
		- Cause: change handlers and a queued callback are left connected at teardown.
		- Fixed: neither releases the shared settings any more - it belongs to the settings store and outlives them both.
		- Fixed: teardown now cancels the queued callback and disconnects the change handlers before anything else goes.
		- Fixed: the favorites file also stopped taking a hold on the settings it never gave back, and three error paths no longer walk away still holding a lock.
		- Verified: the shared settings object outlives both, and a change after teardown reaches nothing.

	- ✅ Item 8. A stuck thumbnail helper is never given up on.
		- Cause: there is no time limit on an external thumbnail program, so one hung file permanently costs a worker slot until restart.
		- Cause: a failed reload of the thumbnail helper list reads the entry it just freed.
		- Cause: a very long, very thin image produces no thumbnail and a warning instead of a graceful fallback.
		- Fixed: a helper that has not finished in 30 seconds is stopped, logged and moved on from, so the slot comes back. Thumbnailing on a one-thread machine no longer ends for the session.
		- Fixed: the reload walk stops at the entry it removed instead of stepping off it.
		- Fixed: a thumbnail is never asked for at zero pixels wide or tall, so a 5000x1 image thumbnails instead of failing.
		- Verified: a hung helper gives up its slot inside the time limit, and a very thin image thumbnails. The freed-entry read is invisible at runtime, so it rests on reading the code.

	- ✅ Item 9. Emptying the Windows trash fails whenever it holds a folder.
		- Cause: trashed folders are reported as folders but refuse to list their contents, and the delete path needs to list them.
		- Note: this affects both "Empty Trash" and permanently deleting a single item.
		- Fixed: a trashed folder now goes with everything inside it, so emptying the trash gets through a bin holding folders.
		- Fixed: a trashed folder lists its contents. Permanently deleting one counts what is in it first, and that count used to fail before the delete even started - a second, separate stopping point.
		- Fixed: with that, a trashed folder can be opened and browsed rather than showing an error page. Its contents carry no original location or deletion date of their own, which is correct - only the folder was trashed.
		- Verified on Windows against a real recycled folder. The old failures were "not a directory" on the listing and "directory not empty" on the delete.
		- Note: a link or junction inside a trashed folder is deleted as the link it is, never followed out of the bin.

	- ✅ Item 10. Windows trash items can go missing, and restore can aim at the wrong place.
		- Cause: items the shell describes in a form the code does not expect are skipped silently, while the item count still includes them.
		- Cause: a long original location is cut short, and the shortened path is what a restore would use.
		- Note: only reproducible on real Windows. Belongs with the real-Windows validation pass.
		- Fixed: an item the shell describes in an unexpected form is now reported rather than silently dropped, and the original location is read at full length so a restore aims at the right place.
		- Note: written and cross-built here, exercised only under wine. Belongs to the real-Windows validation pass.

	- ✅ Item 11. The Windows trash monitor can freeze the app.
		- Cause: it announces changes while still holding its own lock, so a listener that closes or opens a trash view deadlocks.
		- Fixed: the announcement is made after the lock is released, so a listener that opens or closes a trash view cannot deadlock it.
		- Note: written and cross-built here, exercised only under wine. Belongs to the real-Windows validation pass.

	- ✅ Item 12. Windows network browsing builds wrong addresses and cannot report a failure.
		- Cause: a share's address is joined to its server without a separator, so shares get malformed addresses and two servers can collide.
		- Cause: no network, or access denied, looks exactly like an empty network - no message either way.
		- Cause: any typed network address is presented as a valid empty folder rather than "not found".
		- Cause: nothing limits how deep the enumeration recurses.
		- Fixed: a share's address is joined with a separator, no-network and access-denied are reported instead of reading as an empty folder, an address that cannot be reached comes back as not found, and the enumeration is depth-limited.
		- Verified on Windows: new test covers the address building - a share now lands under its server, and two server/share pairs that used to run together into one address stay apart.
		- Also verified against real shares: this box serves four of its own, and the test now browses them for real - each comes back as a link to its UNC path, and each is opened to prove the link goes somewhere. The one that does not open is an empty optical drive, which the test names rather than counting against the backend.
		- Still open: the no-network and access-denied halves. Both need a machine that fails in those specific ways, which this one does not.

	- ✅ Item 13. Windows context-menu actions break on ordinary paths.
		- Cause: "Open as Administrator" passes the folder unquoted, so anything with a space arrives as two separate locations.
		- Cause: "Open in Terminal" at a drive root passes a trailing backslash that swallows the closing quote.
		- Fixed: both paths quote properly, so a folder with spaces and a drive root each work.
		- Verified on Windows: drive roots, UNC roots, spaces and embedded quotes all come out right. The two hand-offs themselves are not covered - one raises a UAC prompt and the other opens a console.

	- ✅ Item 14. Opening a Windows shortcut can truncate its target or hang the app.
		- Cause: targets past the old length limit are silently cut short and then opened, wrongly.
		- Cause: a shortcut pointing at itself, or at a loop of shortcuts, recurses until the app runs out of stack.
		- Fixed: the target is read at full length, a chain of shortcuts is followed to its end with a loop guard, and a failed read leaves an error behind.
		- Verified on Windows: new test creates and reads back a shortcut, including one aimed past the old length limit.
		- Also found and fixed while checking it: Windows itself refuses to store a target that long, and we were not looking at the answer - so "Make Link" wrote a shortcut pointing at nothing and called it a success. It now refuses and says why, and leaves no file behind.

	- ✅ Item 15. A duplicated line in the settings file empties a list instead of falling back.
		- Cause: an unreadable list is treated as a deliberately empty one. Only lists behave this way; single values fall back correctly.
		- Note: a duplicated column list opens the list view with no columns at all. Hand-editing is a supported way to use this file, so this is easy to hit.
		- Fixed: a setting listed twice, or holding the wrong kind of value, falls back to its default and says so instead of coming back empty.

	- ✅ Item 16. An external edit arriving mid-change throws the change away.
		- Cause: settings are written a couple of seconds after they are changed, and a file reload in that window replaces the pending change with no warning.
		- Fixed: a change made in the app inside the save delay is carried across the reload instead of being replaced by what is still on disk.

	- ✅ Item 17. Settings changes can be announced from a background thread.
		- Cause: deleting files updates favorites from a worker thread, and the change is announced on that same thread.
		- Note: the previous settings system always announced on the main thread, which is what every listener assumes. Nothing fires today, so this is a trap for the next listener added.
		- Fixed: change notifications are always delivered on the main thread, which is what every handler assumes.

	- ✅ Item 18. A damaged per-folder settings file is discarded without a word, then overwritten.
		- Cause: a parse failure leaves an empty store, and the next change writes that empty store over the file.
		- Note: costs every folder's saved view, zoom, sort and layout. A failed save is likewise ignored.
		- Fixed: an unreadable per-folder settings file is reported and kept aside, so the next change cannot overwrite the only copy.

	- ✅ Item 19. Setting the thumbnail size limit above two gigabytes breaks thumbnails.
		- Cause: the limit is stored in a smaller number than the dialog offers, so the large choices wrap. Eight gigabytes turns every thumbnail off; two and four turn the limit off entirely.
		- Fixed: the size limit is read at full width, so the large choices work instead of turning thumbnails off or on wholesale.

	- ✅ Item 20. Opening a folder on an unresponsive drive freezes the whole window.
		- Cause: the fallback added for unreadable folders asks for the listing in a way that blocks until the system gives up.
		- Cause: it also treats any general failure as that same case, so a passing glitch is remembered as a made-up folder with no way to tell.
		- Cause: a folder with many unreadable entries stops partway and shows an error over a half-listed folder.
		- Fixed: the fallback for an unreadable folder no longer blocks the window, the skip allowance counts a run rather than a total, and a folder that could not be read is recorded as unknown rather than confirmed empty.

	- ✅ Item 21. Right-clicking a path segment can offer actions the folder will not allow.
		- Cause: the menu is now built before the folder's details have loaded, and the unknown state reads as "everything is permitted", so Delete and New Folder appear on read-only places.
		- Fixed: a path segment whose details have not loaded no longer offers actions the folder may not allow.

	- ✅ Item 22. Changing the default zoom discards a zoom deliberately set in another tab.
		- Cause: every open view reacts, not just the visible one, so background tabs lose their own setting.
		- Fixed: only the folder in front of you gives up its pinned zoom when the default changes.

	- ✅ Item 23. The "treat root as a normal user" preference is read before settings are open.
		- Cause: it is consulted while handling the command line, which happens first, so it is answered wrongly and then remembered.
		- Fixed: the preference is no longer answered and remembered before settings are open.

	- ✅ Item 24. Folder listing and file moves do more per-file work than they used to.
		- Cause: every file now builds an address and takes a shared lock to check the per-folder store, where before there was a cheap early exit.
		- Cause: every moved file scans the whole store, so a large move gets slower the more is stored.
		- Cause: on Windows the per-type icon is rebuilt for every file on every update, not just when the type changes.
		- Cause: the store is rewritten whole on every save and never pruned.
		- Fixed: an empty store costs nothing per file, a move only scans when there is something to re-key, and the Windows per-type icon is derived once per type instead of once per file.

	- ✅ Item 25. Reading a setting costs more than it should, and text settings grow memory.
		- Cause: every read searches the whole settings table from the start.
		- Cause: reads of text, list and choice settings allocate inside the settings document and never give it back, and one of them runs on every icon the mouse passes over.
		- Fixed: settings are looked up directly rather than searched from the start, and the memory the settings document hands out is reclaimed instead of growing for the life of the run.

	- ✅ Item 26. The Windows recycle bin is rescanned far more than needed.
		- Cause: a full scan runs every few seconds for the life of the app, twice more on every look at the trash folder, and once more for every item not already known.
		- Fixed: a look at the trash folder scans once instead of twice, and the periodic check notices a change that leaves the count the same.

	- ✅ Item 27. The release checksums file can be written wrong.
		- Cause: an empty release folder still writes a bogus line, and any artifact name with a space would be split in two.
		- Note: this is the file both installers verify a download against.
		- Fixed: null-separated, and it no longer runs the checksum tool at all when there is nothing to check.

	- ✅ Item 28. Assorted unsafe or non-portable paths in the pipeline and installer scripts.
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

	- ✅ Item 29. Script style and speed debt.
		- Fixed: the backup rotation, the dogfood pruning and the argument parsing all use builtins where they used to start a program per item.
		- Fixed: the unused function is gone.
		- Fixed: the output helpers now live in one file that the helper scripts share, instead of each carrying its own lesser copy.
		- Fixed: the Windows installer gained proper built-in help, so `Get-Help` and `-?` work.
		- Note: the review said three scripts had diverged output helpers; only one actually had. The others define a single matching helper, which is fine.

	- ✅ Item 30. A Windows-only test reports a pass when it did not run.
		- Cause: the trash test exits successfully unless it detects the compatibility layer used for development, so on real Windows it silently skips.
		- Note: that is exactly where items 9 and 10 would have been caught.
		- Fixed: it reports a skip instead of a pass when it cannot run.

- ✅ Setting list view to 66% doesn't affect current list view. It should.
	- Opened: 20260724-135703
	- Closed: 20260725-184516
	- Also, setting default view to List mode, should affect current view immediately as well.
	- Cause: a folder stored its own view and zoom the first time it was opened, even when that just matched the default, so it was pinned to whatever the default was that day and later changes to the default never reached it. Nothing was watching the default view setting at all.
	- Fixed: a setting that only matches the default is no longer stored, so folders keep following it. Changing a default now also applies to the folders already on screen, and folders you deliberately set to their own view or zoom keep it.

- ✅ Settings don't seem to be persisting.
	- Opened: 20260724-091054
	- Closed: 20260725-172648
	- Verified: settings do persist, on both Linux and Windows. Checked the menus, the Settings dialog, per-folder view state, and window size, each set in one run and read back in the next.
	- Cause: the Settings dialog was crashing the whole app at the time this was filed, so nothing set in that session was kept. That crash is fixed.
	- Fixed as well: window size and position were only written when a window was closed cleanly, so a crash - or the wine launcher replacing the running copy - threw them away. They are now saved shortly after a move or resize settles.

- ✅ Windows via Wine: error message at startup. 'The folder contents could not be displayed.', 'Sorry, could not display all the contents of "<username>": Input/output error.' Mouse cursor also stuck at "busy spinner".
	- Opened: n/a
	- Closed: 20260725-153058
	- Reproduced: opening a home folder containing a unix symlink.
	- Cause: one unreadable child failed the whole folder listing. The aborted load also left the busy cursor on.
	- Fixed: the unreadable child is skipped and the rest of the folder lists. The load completes and the cursor clears.

- ✅ Windows via Wine: cursor seems stuck on the "busy" mouse icon.
	- Opened: n/a
	- Closed: 20260725-153058
	- Cause: same as the startup error above. The folder load never finished, so the busy cursor never cleared.

- ✅ Icons don't match OG nemo.
	- Opened: 20260724-091054
	- Closed: 20260725-153058
	- Cause: two gaps. Windows reports one generic icon for every file type, and the Windows dependency snapshot was missing its image-loader cache, so no symbolic (SVG) icons rendered.
	- Fixed: per-type icons now derived from the file type on Windows. The loader cache is generated when the snapshot is built.
	- Note: the app's own bundled PNG icons didn't resolve on Windows either. That was a separate item, since done.

- ✅ Windows: dot-name folders don't say "Folder". Regular folders say "Program", not "Folder".
	- Opened: 20260724-135703
	- Closed: 20260725-153058
	- Cause: folder type was guessed from the name whenever size read as zero, which every Windows folder does.
	- Fixed: folders always report the folder type, never guessed.

- ✅ Portable fallbacks for the remaining Mint-flavored theme icon names.
	- Opened: n/a
	- Closed: 20260725-153058
	- Cause: menus and toolbars referenced icon names only Mint themes ship. Pre-existing gap on non-Mint, cosmetic only.
	- Fixed: all names mapped to standard freedesktop names (mostly a straight prefix strip; the non-standard ones got closest equivalents).
	- Verified: every mapped name present in both the Linux and Windows icon themes.

#### Done - Features and enhancements

- ✅ Copying and pasting objects that includes symlinks or junctions, should open up an option dialog. (All OSes.)
	- Opened: 20260831-164337
	- Closed: 20260902-170000
	- Ask whenever the source holds links, on any platform, so a user always knows what they are getting. The dialog names what the source holds, with a row per kind:
		- File symlinks as: symlinks, or copies.
		- Folder symlinks as: symlinks, junctions [Windows], or copies.
		- Folder junctions [Windows] as: junctions, symlinks, or copies.
	- Anything the destination cannot take is greyed out. Junction-related options not shown for non-Windows OSes. Each row starts on the same kind if that is possible, otherwise the nearest kind that still points at the original target, otherwise copies.
	- Done, and on every platform. The dialog names only the kinds the source actually holds, and greys out anything the destination cannot take - including the case where it can take none, where it says why and only the copy is left.
	- Windows had the real gap: a copy always followed the link and left the contents behind, so a link could not be copied at all. POSIX already kept symlinks; what is new there is being able to ask for the contents instead.
	- The kind of a Windows link comes from the reparse tag. Nothing else tells a junction from a folder symlink, and it also keeps cloud placeholders and store app aliases - which are reparse points too - from being read as links.
	- A link keeps its own spelling, so a relative one still points where it pointed. Asking for a junction is the exception: those can only name a full path, so a relative target is resolved first.
	- A link now counts as one item in the copy rather than a folder to walk into, which is what POSIX always did and Windows never did.

- ✅ Drag and drop a file to a program should work. (E.g. a '.md' or '.txt' file to VSCodium or Notepad.)
	- Opened: 20260831-164337
	- Closed: 20260902-000000
	- Windows only. Linux drags already reach any program, GTK or not.
	- The toolkit does drive a drag on Windows, but the file formats other programs read were never filled in on its side, and there is no way to add them from outside it. So the drag is ours now, the way the clipboard is.
	- Done. A drag out of either view carries what Explorer's own drags carry, so other programs read it. Checked in the running app both ways: dropping on Explorer, dropping a text file on an editor, and dragging inside nemo, which still moves files as before.
	- A move out to another program now removes the original, unless that program moved it itself or the drop came back into nemo. Control copies and shift moves, the way Windows does it.
	- Drops coming the other way, from another file manager into nemo, copy and move too. They always copied before: nothing is known about a file dragged in from elsewhere, so nemo could not tell whether it was on the same drive and fell back to copying every time.
	- Checked against Directory Opus in both directions, and between two nemo windows: a plain drag moves within a drive, control copies, shift moves.

- ✅ Move all Windows-related options to a "Windows" preferences pane; and in the config file, to a grouped section.
	- Opened: 20260831-164337
	- Closed: 20260901-183000
	- Some cannot move. Such as "Owner" in list columns.
	- The page carries Light and dark, Theme, Paths, Hidden files and Search, and takes the slot Appearance used to have. It is hidden everywhere else, so Linux no longer offers theme or light/dark settings - those come from the desktop there. The keys still work if hand-edited.
	- "Shortcuts" stayed on Display: .desktop launchers hide their extension too, so it is not a Windows-only setting.
	- Hidden files is a new group with two switches, for the native hidden attribute and for dot names. Both stay on the View menu on Windows, where Show Hidden Files moves the pair together; elsewhere the menu keeps the one meaning it has always had.
	- Config keys moved to a `windows` group: path-separator, allow-slash-input, show-dot-files, use-search-index, associations, terminal-candidates. Existing settings files lose those values, which is accepted before 1.0.

- ✅ If "Show path in tab" option is enabled, don't show the path twice - shorten it. For example:
	- Opened: 20260831-164337
	- Closed: 20260831-201500
	- Current: "github - C:\opt\0-0\users\collierjr\data\prs\dev\github.com\t00mietum\nemo-anywhere\github"
	- Better: "C:\opt\0-0\users\collierjr\...\nemo-anywhere\github"
	- The folder name in front of the path is gone - the path already ends with it.
	- A path too long for a title keeps its root and its last two folders, with the middle left out. The root says which drive or share it is on, the end is what tells one tab from another.

- ✅ .Lnk folder icons should use the same folder icons as the theme, but with an overlay.
	- Opened: 20260831-164337
	- Closed: 20260831-193000
	- The shell hands back its own folder art for a shortcut to a folder, which looks nothing like the folders around it. The theme's folder icon is used instead, with the shortcut overlay on top.
	- Whether the target is a folder is read from what the .lnk itself records, not by looking at the target - a shortcut to a share that is not answering would otherwise cost about twenty seconds on the draw path.

- ✅ All .lnk files should have a .lnk overlay (similar to how Explorer does it).
	- Opened: 20260831-164337
	- Closed: 20260831-193000
	- New overlay, drawn as an arrow in a white box the way the shell does it.
	- .desktop launchers get the same one, on every platform.

- ✅ All symlinks and junctions should have an overlay, but different from .lnk.
	- Opened: 20260831-164337
	- Closed: 20260831-193000
	- Ditto for Linux symlinks, and .desktop files.
		- Like .lnk files, don't show ".desktop" in listings, except when renaming.
			- When renaming, show both .lnk and .desktop extensions.
			- Still show both extensions in the "Ext" column.
		- .desktop files can use the same overlay as Windows .lnk, if necessary/convenient.
	- Symlinks and junctions get a chain-link overlay, so the two never read the same.
	- Both overlays are ours rather than the theme's. An icon added by resource path is only searched after every theme, so a theme that carries its own symlink emblem would always win - and most of them draw the same arrow the shell uses for a shortcut.
	- .desktop now behaves like .lnk: the extension is off the listing, on in the rename box, and still in the "Ext" column. One preference covers both, and it is no longer hidden on Linux.
	- Checked on a junction. A real symlink could not be made without Developer Mode, but both are reparse points and read the same way.

- ✅ Preferences|Context menus still has a "Desktop" group with a "Customize" box in it. Nothing is behind it. Remove.
	- Opened: 20260831-170000
	- Closed: 20260831-172500
	- Found while taking out the desktop tooltip box.
	- The action it was meant to show never existed in this fork, so the box could only ever hide something that was not there. Gone, along with its setting.

- ✅ Remove bottom-left buttons, and bottom-right zoom bar in list view.
	- Opened: 20260831-164337
	- Closed: 20260831-170000
	- Make bottom bar vertically thinner (since don't need room for the buttons on bottom-left any more; just room for status text, and zoom slider in icon view).
	- The four sidebar buttons are gone from the bottom bar. Switching between places and tree, and hiding the sidebar, are still on the View menu and on F9.
	- The zoom slider now shows only where it does anything useful - icon and compact views. List view sizes itself off its columns.
	- The bar is about half its old height, since nothing in it needs button room any more.

- ✅ Preferences|Preview: "Show tooltips on the desktop" (and related checkboxes) have no meaning. Remove.
	- Opened: 20260831-164337
	- Closed: 20260831-170000
	- Removed, along with the setting behind it. Nothing read it once the desktop shell went.
	- The icon-view and list-view tooltip boxes stay - those still do something. So do the boxes choosing what a tooltip shows.

- ✅ By default, disable all checkboxes related to Preferences|Behavior|Media handling.
	- Opened: 20260831-164337
	- Closed: 20260831-170000
	- Automount, automatic open of a mounted disk, and content detection all start off now. The fourth box in that group was already off.

- ✅ Gray out "Open as Administrator", if already running as such.
	- Opened: 20260831-164337
	- Closed: 20260831-170000
	- Windows has no root account to test for, so the item used to stay live in an already elevated copy and a second prompt bought nothing. The process token is asked instead.

- ✅ Windows: show the icon the shell would show for .lnk files (without requiring Explorer to run).
	- Opened: 20260827-090000
	- Closed: 20260829-103000
	- Split out of the done item for opening a shortcut the way Explorer does. A shortcut showed a generic icon rather than its target's - the toolkit reports one flat icon for every file on Windows.
	- Fixed: a shortcut is drawn with the shell's own icon for it, at each of the shell's sizes rather than scaled from one, and cached. No Explorer process is involved.
	- Note: the shell's icon for a registered file type (a .docx drawn as Word's) is deliberately not used for ordinary files - it would fight the icon themes. The lookup is by path and could be widened later.

- ✅ Windows: edit a `.lnk`'s target from a properties view - the analog of the `.desktop` launcher editor.
	- Opened: 20260826-103001
	- Closed: 20260829-100000
	- Added: Properties on a shortcut shows Target, Arguments, Start in and Comment below the name, each saved as it is edited. A file dropped on Target or Start in fills it in. A shortcut with no file target still opens for editing.

- ✅ Windows: no shell coupling for file associations - read them from the registry (system defaults only), layered under an override map of our own.
	- Opened: 20260730-203115
	- Closed: 20260829-093000
	- Overrides launch directly. All settings and overrides live in the settings file, never written to the registry.
	- Fixed: the default for a type is the override when one is set, else what the shell itself would open it with, asked the way Explorer asks. The toolkit's own answer could be a print command, and its Open With list carried print entries too; those are gone.
	- Fixed: "Set as default" in Open With records the choice in the settings file, one line per type in the registry's own `%1` shape, and Reset takes it away again.
	- Note: a program is shown under its own description (Notepad, VSCodium), the way Explorer names it.

- ✅ Bookmarks are kept in the toolkit's own file, not ours.
	- Opened: 20260828
	- Closed: 20260829-090000
	- Only relevant on Windows. The toolkit's file sits in the local profile while the settings are in the roaming one, so a roaming profile carried the settings and left the bookmarks behind.
	- Fixed: on Windows the list lives beside the settings. A list an older version kept in the toolkit's file is copied across the first time, and a reset clears both so the old list cannot come back.

- ✅ Windows: content search cannot read documents, because the search helpers are not packaged there.
	- Opened: 20260828-160000
	- Closed: 20260829-083000
	- On Linux a helper turned a document into text so "Containing:" could search it. The Windows layout carried the executable and the toolkit and nothing else, and three of the helpers were a Python script, a shell script and a LibreOffice call.
	- Fixed: the converters are plain C and ship on every platform. Word, Excel and PowerPoint in both the old binary and the newer zip-of-xml forms, OpenDocument and EPUB. The scripts and their dependencies are gone.
	- Fixed: a helper is looked for beside the main program before the search path, so a name that has to gain `.exe` is found all the same.
	- Fixed: a helper's `Priority` is honoured. One helper runs per file; the next is only tried when it cannot read the file at all.
	- Added: a switch in Preferences to answer searches from the Windows Search index for folders it covers. Off by default. Folders outside the index, and content searches by pattern or by case, are still searched directly.

- ✅ Windows and NTFS: any directory symlink through any mechanism should also allow a junction, preferred over a symlink.
	- Opened: 20260823-142431
	- Closed: 20260828-151500
	- The hidden-files half of this item became "Two kinds of hidden file, two options", now done - it asks for the same thing as two switches rather than one.
	- A link to a folder is now a junction. One place decides it, so every route into "Make symlink" gets the same answer, and a symlink is still the fallback for anything a junction cannot hold - a file, a share, a relative target.
	- The point of preferring one: a junction needs no privilege. Making a folder link no longer wants Developer Mode or an elevated run, and the menu item stops greying out for a folder on a machine that has neither.
	- Verified: a folder link made from the menu reads back as a mount point rather than a symlink, and a new check covers it.

- ✅ New flag: `--reset`. Clears bookmarks, resets to default state. (Maybe just delete the config file?)
	- Opened: 20260730-112038
	- Closed: 20260828
	- Every stored setting is dropped and the settings file itself is removed, so anything hand-written that nemo does not recognize goes too. Bookmarks and their side file go with it.
	- It refuses while a copy is running, and says so. That copy holds the settings in memory and would write them straight back.
	- The first-run marker is cleared along with everything else, so the next start puts the platform defaults back.

- ✅ If the Windows version has never run before, the bookmarks should be cleared, and populated with only the main Windows defaults. (C:\, Desktop, Documents, Downloads, Pictures, Videos, AppData). Also, all linux-specific settings and bookmarks should be cleared on first startup.
	- Opened: 20260722-172504
	- Closed: 20260828
	- On the first start the drive root and the user's own folders go in, taken from what Windows reports rather than spelled out, so a machine on another drive or in another language gets the right names.
	- A bookmark that can only be a path from a POSIX machine is dropped, and so is any setting whose value is one. A set someone already curated on Windows is kept rather than replaced - that matters for anyone upgrading from a build without the marker.
	- Marked by `state.first-run-done` in the settings file. Clearing that line by hand puts the defaults back on the next start.

- ✅ Allow '~' in bookmarks to specify home dir (only if at the start and unquoted).
	- Opened: 20260722-201512
	- Closed: 20260828
	- `~` at the start, and `%NAME%` or `$NAME` anywhere. Both variable spellings work on both platforms so a path can be carried between them.
	- The literal text still wins: a folder really named with a `%` in it opens as itself, and only a name that is actually set in the environment is ever substituted. Verified both ways.
	- Reaches the location bar, the bookmark editor and the command line.
	- ✋ Not done: storing the shorthand *in* the bookmarks file so it follows the home folder around. That needs the file to keep an unexpanded form and re-expand on load, which is a bigger change than the input side.

- ✅ Windows: an option to leave the `.lnk` off a shortcut's name.
	- Opened: 20260828-083458
	- Closed: 20260828-090000
	- The shell never shows it, so nor do we unless the new switch on the Display page is turned on. Off by default.
	- Only the name shown loses the extension. The Ext column still says `lnk`, and a rename typed as the shown name puts the extension back, the same way a renamed `.desktop` file keeps its own - without that a rename would quietly turn the shortcut into an ordinary file.

- ✅ Let the Type column take the width it needs when there is room for it.
	- Opened: 20260828-083458
	- Closed: 20260828-090000
	- It was held to twice the width of the Ext column, so it read "Folde" and "Link t" in a window with plenty of room to spare.
	- That ceiling is gone. Type still gives its width back first when the window is too narrow, and still stops at a share of Name so one long value cannot take the row.

- ✅ The preferences dialog opens too short for the Display page, and does not follow a fractional display scale.
	- Opened: 20260828-083458
	- Closed: 20260828-090000
	- It sized itself to the Views page alone, so every longer page opened behind a scrollbar. Measured: Views 690, Display 832, Behavior 1045, against an opening height of 700.
	- It now measures every page and takes the longest, and the width the widest page needs, both still capped at nine tenths of the screen.
	- The minimum size it will not go below is written in pixels for a 96dpi screen, so at 150% it quietly meant two thirds of what it said. It is scaled by the same font size Windows hands the toolkit.
	- A check now compares the page list the sizing walks against the pages the dialog actually holds. It found one missing on its first run - Document templates, whose page had no name at all.

- ✅ The two command fields on the Behavior page crowd their labels and run past the section.
	- Opened: 20260828-083458
	- Closed: 20260828-090000
	- Four pixels between the label and the field, and the field itself pushed past the right-hand margin every other section keeps.
	- Twelve pixels now, the field stops where the rest of the page does, and the two fields start at the same place as each other.

- ✅ Move the Ext column between Size and Type in the default order.
	- Opened: 20260827-183930
	- Closed: 20260827-194220
	- It sat between Name and Size. It stays on by default either way.
	- Both platform defaults moved, and the schema with them.

- ✅ Name and Location split the search row evenly.
	- Opened: 20260827-183930
	- Closed: 20260827-194220
	- The default was a third to Name and the rest to Location.
	- A split dragged by hand still stands from then on.

- ✅ Location is off by default outside search.
	- Opened: 20260827-183930
	- Closed: 20260827-194220
	- It belongs in the search results list and nowhere else, unless it is turned on by hand.
	- Already the case: it is in the default column order but not the default visible list, and a run against a clean config confirmed it does not appear. Wherever it was seen, it had been turned on for that folder and remembered.

- ✅ A preference for which terminal "Open in Terminal" runs.
	- Opened: 20260827-183930
	- Closed: 20260827-194220
	- One field on the Behavior page, holding the command line. Anything the program needs beyond its own name is typed in by hand.
	- Left empty it means the platform default, which is what happened before: the desktop's own choice on Linux, the first of the known shells found on PATH on Windows. Filled in, it wins over both.
	- Splitting one field into a program and its arguments has three rules, in order: a quoted first word, then a string that names a program on its own (so an unquoted path with spaces still works), then the first space. Tested.
	- Verified on Windows: a terminal named with an argument is launched exactly as written.

- ✅ Make link is on by default, and Windows tells a shortcut from a symlink.
	- Opened: 20260827-183930
	- Closed: 20260827-195422
	- The menu item shipped turned off.
	- Renamed "Make symlink" on every platform, since a symlink is what it makes.
	- Windows gained a second item, "Make shortcut", for the .lnk the shell understands. Both are on by default and share one switch in Context menus - two toggles for nearly the same thing would only be confusing.
	- Windows allows a symlink only with Developer Mode on or when running elevated, so the item goes grey when neither holds. The check is made once by making a throwaway symlink and deleting it, which is a plainer answer than reading a token and a registry key.
	- A drag with the link modifier still makes a shortcut on Windows, which is what Explorer does.
	- Both verified on Windows against a real folder. Not covered: the greyed-out state, which needs a box without Developer Mode; and an undo-then-redo of a symlink remakes it as a shortcut, since both share one undo record.

- ✅ Update the vendored SHCL to the current release.
	- Opened: 20260826-103001
	- Closed: 20260827-075015
	- It manages its own file creation and updating now, which is one of the rough edges hit here.
	- Note: Fetch it from the source.
	- Moved from 1.2.0 to 2.0.0. Nothing in the settings layer had to change: none of the calls made here changed shape, and neither of the two breaking changes is reachable from plain key names.
	- What comes with it: parsing holds roughly half the memory it did and loads faster, number handling no longer follows the host locale (under a comma-decimal locale every float read used to fail and the canonical output diverged), and a line that is malformed but still placeable is now kept and written back instead of dropped.
	- Its new file tier was deliberately compiled out at first. The writer reaches Windows through the ANSI calls, which are the system codepage unless the exe asks for UTF-8, so a config under a non-ASCII user name would fail to save.
	- Taken on 20260828, once the manifest asked for UTF-8. Settings now save through it: a temp file beside the target, flushed to disk before it is published, and on Windows a replace that carries the old file's permissions, attributes and alternate streams onto the new one. The previous writer published a brand-new file and left all of that behind.
	- Reading stays where it was. The library reads a file with no size limit, and its allocator ends the process rather than failing, so the cap in front of it is worth keeping; the reader also hands back the exact bytes the "was this our own write" check compares against.
	- The trap: the library names its temp file by splitting the path on a forward slash and nothing else, so a Windows path spelled with backslashes puts the temp somewhere impossible and every save fails. The path is handed over spelled with slashes. The existing config checks caught this immediately.

- ✅ Ask for UTF-8 as the process codepage in the Windows manifest.
	- Opened: 20260827-075015
	- Closed: 20260828-142000
	- Windows 10 1903 and later read `activeCodePage` and make every narrow call UTF-8. Without it a narrow call anywhere in the process is at the mercy of whatever codepage the machine is set to, which is how a non-ASCII user name breaks things that otherwise look fine.
	- Not free: it changes the codepage for everything in the process, not just our own calls, and it does nothing on the older versions the manifest still claims. Wants a look at what else narrows before it goes in.
	- Looked. Nothing of ours narrows: every Windows call in the tree is the wide form, and the only conversions are explicit UTF-8 ones. What the change reaches is the libraries underneath and the C runtime, which is the point of it.
	- In: the code page reads 65001 with the manifest and 1252 without. The app was run with its config under a folder named in German and Japanese, and read, wrote and live-reloaded it. Suite unchanged.
	- Follow-on, taken: the config engine now saves through its library's own writer. See the SHCL item above.

- ✅ The whole `desktop` group of settings is dead weight.
	- Opened: 20260827-075015
	- Closed: 20260827-081500
	- Fifteen keys left behind when the desktop shell came out. They still ship in the schema and still appear in a generated starter config, so a user can set them and nothing happens.
	- Two of the fifteen are not clearly dead on a quick look - one leaf name is shared with a live setting in another group - so this wants checking key by key rather than deleting the group.
	- Checked key by key. Twelve had no reader anywhere and are gone from the table, the schema and the preference names. Three still have live readers and stay: the deprecated manage-the-desktop switch, the grid switch, and the desktop text ellipsis limit, which shares its leaf name with the icon view's own.

- ✅ Windows: open a `.lnk` the way Explorer does, by what it points at.
	- Opened: 20260826-103001
	- Closed: 20260827-090000
	- A shortcut to a file opens in the file's associated program.
	- A shortcut to a program runs it.
	- A shortcut to a folder goes to that path in the current tab.
	- Use an appropriate icon.
	- Note: following a shortcut through to its target already works. What is missing is treating each kind of target differently.
	- A shortcut to a folder still opens in the current tab, which is the one case worth doing differently from the shell. Everything else is now handed to the shell as the shortcut, not as its target.
	- That is what fixes the program case. A shortcut carries a command line, a working directory and a window state, and none of them survive being reduced to a target path - a shortcut to a shell with arguments used to open a bare shell. Shortcuts to virtual items (Recycle Bin, a control panel page) now open too, having no path to reduce to in the first place.
	- Verified: a launched shortcut's arguments and working directory both arrive.
	- Icon split off below - it is a bigger piece than the rest of this and applies to more than shortcuts.

- ✅ Show a build number in `--version`, `--about`, Help > About, the Windows splash screen, and the release notes.
	- Opened: 20260826-103001
	- Closed: 20260827-100000
	- The build number is the minutes elapsed since the start of 2000, Crockford base32 encoded, lower case.
	- General format: "<program name> v<version> build <build>" [copyright ...]
	- Five characters at the moment. It comes off the same commit date the reproducible builds already use, so two builds of one commit agree; a build outside the release lanes falls back to the clock.
	- `--about` is new, and prints the version line, the copyright, the project home and the license. Help > About gained the copyright and a link to the project, which it had never shown.
	- Checked on both platforms: the two command line outputs, the About dialog, and the splash.

- ✅ Ctrl+H toggles dot-files and Windows hidden files together.
	- Opened: 20260826-103001
	- Closed: 20260827-110000
	- If the two are out of step, take the Windows hidden setting as the current value and match the other to it.
	- Ctrl+Shift+H stays as it is, Windows only.
	- Both the setting and the dot-file menu item move with it now. The item had to be ticked directly - neither of the two toggles watches for a change made anywhere else, they are only read when the menus are built, which is a wider gap worth closing on its own some time.
	- Nothing changes off Windows, where one switch already covered both.

- ✅ Ctrl+, opens Preferences.
	- Opened: 20260826-103001
	- Closed: 20260827-110000

- ✅ Build timestamps come from the commit being built, not the clock, so a release can be reproduced.
	- Opened: n/a
	- Closed: 20260826-152000
	- The Windows exe was the one that really varied: the linker writes a timestamp into the PE header, and two clean builds of the same commit differed in exactly those four bytes. Everything else was already close.
	- Every lane now sets `SOURCE_DATE_EPOCH` to the commit date and hands it to whatever stamps a time. `zip` has no notion of it, so the staged tree gets the date set on disk and is packed in sorted order; `tar` is told explicitly; the rpm spec has to ask for it before rpm will read it.
	- Verified by building each artifact twice from scratch: the Windows exe, the Linux tarball, the .deb, the .rpm and the Windows zip all came out byte-identical. A build with the stamp removed differed, which is the check that the mechanism is what did it.
	- Also fixed on the way through: the Linux release lane had been failing since the staging script gained a safety guard on its destination name, which no longer matched what the release script passed it.
	- Not covered, and cannot be: a signed exe, since the countersignature carries the real time of signing.

- ✅ Windows: two kinds of hidden file, two options.
	- Opened: n/a
	- Closed: 20260823-142431
	- Supersedes the older item that asked for the same thing as one combined switch.
	- The premise turned out to be worse than described: Windows reports only its own hidden attribute, so dot-files were shown there whatever the setting said. Same for names ending in a tilde, which count as backups elsewhere.
	- "Show dot-files" is now a second switch, Ctrl+Shift+H, next to "Show hidden files" in the View menu and hidden on every other platform, where one switch still covers both. Default is to hide them.
	- The two are independent: revealing attribute-hidden files no longer reveals dot-files, and the listing, the tree sidebar and search all go through the same check.
	- Flipping either one re-reads the open folder, so an edit to the settings file shows up without a restart.

- ✅ Windows: choose which separator paths are shown with.
	- Opened: n/a
	- Closed: 20260823-144331
	- A "Paths" group on the Display page of Preferences, shown only on Windows: "Show separator as" picks `\` or `/`, and a checkbox below it accepts or refuses `/` in a typed location.
	- The checkbox is ticked and greyed out while `/` is the separator on screen, since refusing what is being shown would make no sense.
	- The choice reaches every surface that spells out a path: the location bar, the Location column, path tooltips, the window and tab titles, the sidebar tooltips, drive roots in the sidebar, and the Location row in properties. Breadcrumbs show names only, so there was nothing to change.
	- Changing it re-reads the open folder, so the whole window switches over at once rather than on the next visit.
	- Typed input already took both separators, so what is new is the option to turn `/` off. A location that leans on it is then refused with a beep instead of going anywhere.
	- Also fixed on the way past: the preferences dialog named a widget in a size group that no longer exists, so loading it stopped early and silently. Only an unused list model came after the break, which is why nothing looked wrong.

- ✅ Windows: "Copy path as [\|/]".
	- Opened: n/a
	- Closed: 20260823-145852
	- A second clipboard item directly below the existing Copy Path one, spelling out whichever separator the paths are not currently shown with. It follows the same show/hide setting as the first, so the pair travels together.
	- In all four places the first one appears: the Edit menu, the selection menu, the background menu and the breadcrumb menu. Hidden on every other platform.
	- The existing Copy Path now follows the display setting too, so the pair is always "what you see" and "the other one". A remote location still contributes its uri untouched, since a uri's slashes were never separators.

- ✅ Windows: "Open with Explorer", for a single selected entry.
	- Opened: n/a
	- Closed: 20260823-145852
	- On the selection menu, below Open With. Shown only when exactly one thing is selected and it has a local path, since a remote location gives Explorer nothing to open.
	- A folder opens in Explorer; anything else is revealed and picked out inside its own folder.
	- A deliberate escape hatch rather than a dependency. The standing "depend on Explorer as little as possible" rule is about core function; this one says Explorer on the label.
	- Two routes, because one is not enough: the shell item API, which handles any name, falling back to a command line when that is refused. Running elevated is when it gets refused, and nemo can be running elevated - "Open as administrator" puts it there.
	- Both routes verified. The item opens a window, so it sits behind `NEMO_PROBE_EXPLORER` rather than running on every pass of the suite.

- ✅ Column widths and the Ext column, second pass. Overrides the earlier column rules where they disagree.
	- Opened: n/a
	- Closed: 20260823-134341
	- "File extension" is now just "Ext", and shows the extension without its leading dot. It sits directly right of Name, with Location next along whenever that is switched on.
	- Location, on an ordinary folder listing, grows with Name rather than stopping at a share of it: the two split whatever the other columns leave and Name takes no more than half, so Location is never the narrower of the pair and anything Name does not need goes to Location. Dragging Location by hand ends that and pins the width, as it always did.
	- Date created, Date modified and Date read keep their full width. What has to give when the window is too narrow comes off Name, Location and Type first, in proportion, and only reaches the dates once those three are down to their floors.
	- Type, and any other column with no natural length, never ends up wider than Name or Location.
	- Zooming in or out re-measures the rows. Before this the widths were thrown away and never worked out again, so one Ctrl+= left Location taking most of the row and every date cut short. Same for a column switched on that had not been on screen to be measured.
	- A small gap keeps the first and last columns off the window frame.
	- All of it verified in the running app, the zoom case included.

- ✅ The preferences dialog opens larger, and big enough for the Views page to fit without a scrollbar.
	- Opened: n/a
	- Closed: 20260823-134341
	- The height is measured from the page itself rather than fixed, so a different theme, font size or translation still fits, up to what the monitor has room for.

- ✅ Ask before moving files to Trash defaults to on.
	- Opened: n/a
	- Closed: 20260823-134341
	- Already the default; confirmed rather than changed.

- ✅ Right-click properties wording: ours is plain "Properties" and sits first; the Windows sheet reads "Windows properties (Alt+Enter)" below it. Shortcuts themselves are unchanged.
	- Opened: n/a
	- Closed: 20260822-075741
	- Seen in the running app; the breadcrumb menu says "Windows properties" without the hint, since Alt+Enter acts on the selection rather than a path segment.

- ✅ New list columns.
	- Opened: n/a
	- Closed: 20260822-075741
	- "File extension", on by default, between Name and Type, dot included the way Explorer shows it. Left empty when the tail after a dot is not really an extension - folders, dot-files, too long, all digits, or not letters and digits. The refusals have a test of their own that fails with the checks taken out.
	- "Owner" now shows on Windows too and is on by default there - the platform reports the file's real owner, so the old fabricated-values reason to hide it no longer applied.
	- Windows only: "Permissions source" - Inherited, Local or Mixed, read from the file's ACL - off by default, listed after Owner. Verified against files with disabled inheritance and added grants.
	- Type now defaults to at most twice the File extension column's width.

- ✅ Column widths remember the user's hand. Overrides the earlier auto-sizing rules where they disagree.
	- Opened: n/a
	- Closed: 20260822-075741
	- A column with no natural width limit that the user resizes keeps that width as its ceiling from then on, through any window resizing in either direction, saved in settings.
	- Name still takes all remaining space - except in find mode, where Name and Location split the row one-third/two-thirds by default, and an adjusted split is remembered forever and kept as the window resizes. Supersedes the find-mode column note, now canceled.
	- Both verified in the running app: the dragged ceiling survives narrow-then-wide, and the find-mode split holds at the adjusted ratio across sizes.

- ✅ Properties on Windows opens the one Windows itself shows, instead of ours.
	- Opened: n/a
	- Closed: 20260821-204030
	- Alt+Enter, Ctrl+I and every Properties item now hand the selection to the shell's own sheet - the same one Explorer shows, third-party tabs included. Only Windows; Linux, BSD and macOS are untouched.
	- Ours stays on a second item, "Advanced properties" (Ctrl+Enter), because the Windows sheet has nowhere to put a custom icon, an emblem, an annotation or the image details page. It is hidden everywhere else, where both items would open the same window.
	- Anything the shell cannot name falls back to ours rather than doing nothing: a virtual location, a selection spanning folders (which is what a search result set is), or an item that has gone away since it was clicked.
	- The sheet runs off the main loop, so the window behind it stays live while it is open, and it is waited out rather than abandoned - the extra threads go when it closes.
	- Verified on Windows, and the fallback rule has checks of its own.

- ✅ Every piece of text in the interface reads as a sentence, not as a headline - only the first word capitalised, and anything that is a name left alone.
	- Opened: n/a
	- Closed: 20260821-211359
	- Menus, buttons, tab and page titles, dialog titles, column headings, tooltips, preference labels, and the bundled actions. About 330 labels in all.
	- A mnemonic stays where it was, so the underlined letter does not move; it is simply lower case now. Keyboard shortcut text is untouched.
	- Names keep their capital: the platforms, the toolkit, Trash and the other places in the sidebar, file and disc formats, acronyms. So does a sentence that names a menu item or a tab, since the item itself is still called that.
	- Left alone on purpose: the licence text, which is quoted verbatim, and the name a new folder or document is given, which is written to disk rather than shown.
	- It is checked rather than trusted, because a label copied from upstream arrives in Title Case: `cicd/utility/lint-ui-case.py` reads every translatable string in the tree and fails the lint step on any that is not a sentence. The whole exception list lives in that one file, each entry with its reason.
	- The check found what a first pass by eye did not - the plural labels, where two spellings sit in one call, which is what had left "Copy Paths" and "Make Links" behind.

- ✅ One setting for how much of the machine's CPU any compression may use, as a percentage of the cores it finds. Default 50% - the best balance on a hyperthreaded CPU.
	- Opened: n/a
	- Closed: 20260821-144459
	- `performance.cpu-percent`, global rather than per-format, so a later job that can be spread over cores reads the same number instead of inventing one of its own.
	- Reaches the 7z and rar create lines through a `{{THREADS}}` marker of their own, and tar.xz through the library that writes it. Zip, gzip and the built-in 7z have no such option, so they are left alone rather than handed one they would refuse.
	- It is the one marker that does not stand for a control in the Compress dialog, so a line edited past it says nothing - the program simply picks for itself.
	- Rounds up, so a single-core machine still gets one thread and the answer is never nothing.
	- Verified: each program is handed the switch it spells its own way.

- ✅ Per-monitor DPI aware where the platform offers it, and DPI aware at minimum everywhere else.
	- Opened: n/a
	- Closed: 20260821-150232
	- The Windows executable now carries an application manifest, which is where this is declared and where Windows reads it before any of our code runs. Per-monitor v2 where it exists, per-monitor v1 and then system-wide on older builds.
	- Without it the whole window was stretched as a bitmap on a scaled display - blurry - and a second monitor at a different scale could not be followed at all.
	- The toolkit scales in whole steps only, so a display at 125% or 150% would come out at 100% and read smaller than every other window on that screen. Text is scaled to the monitor's real DPI on top of that, which is not restricted to whole steps, and re-reads it whenever a window moves to a monitor at another scale or a monitor is plugged in. Widgets and icons stay on the whole step.
	- Nothing was needed for Linux or BSD: X11 and Wayland desktops publish their own scaling and the toolkit already follows it.
	- The manifest also declares the run level explicitly (unchanged - what we already had by having none) and the versions of Windows we have run on, so the version APIs stop reporting Windows 8 forever.
	- Verified on this box: the running process reports per-monitor awareness and its window reports the v2 context. The scaling sum is covered by a test. This box runs at 100%, so the fraction itself rests on arithmetic - worth a look on a scaled display.

- ✅ F2 selects the whole name, extension and all, rather than just the part before the dot. Settings tunable, for anyone who wants it the other way.
	- Opened: n/a
	- Closed: 20260821-150232
	- Both views. A folder was already selected whole; a file now is too.
	- `preferences.rename-selects-whole-name`, a file-only setting with no control in Preferences.
	- Verified in the running window: F2 on a `.md` file opens the box with the suffix inside the selection.

- ✅ List view columns use the window as it is resized, instead of being pushed off the end of it or leaving a gap.
	- Opened: n/a
	- Closed: 20260821-153301
	- Widening: columns take the new space until one can show the longest value in it, and then that one stops. Name is the only column that keeps growing without limit, so once everything else has what it needs the rest is Name's.
	- Narrowing, which is the same thing read backwards: Name gives its surplus back first, having had all of it. When every column is down to the longest value it holds and it still does not fit, Type gives next, on its own, to about three characters - it is the one least missed that short, where a date or a size that short says nothing. Only then does everything else give ground together, each in proportion to how wide it is, Name included.
	- A column whose values have no natural limit either - Type, Location, Owner, Group - stops at a third of the Name column rather than taking the window for one long value. The cap and Name's width have to agree with each other, so the answer is found rather than guessed, and it does not depend on the order the columns are in.
	- Narrower than the floors add up to and the view scrolls sideways, which is the honest answer to a window narrower than its own contents.
	- Every value that no longer fits now says so with an ellipsis instead of being cut off mid-letter. Only Name and Location did before.
	- Widths follow the contents: each row is measured as it arrives and as its details fill in, and the widest seen is what a column aims for. Measured against a five thousand item folder, it costs nothing that can be told apart from the noise.
	- A column dragged wider by hand keeps that width until the window changes shape or the folder does.
	- Refines the earlier "Name column always as large as possible" work under Done, which only made Name take the slack; this is the rule for all of them.
	- Verified at half a dozen widths on two folders, and the rule itself has a test of its own.

- ✅ Twelve more icon sets, all of them asked for by name: BeautyLine, the six Simply Circles colours, Lime Numix 2021, MB Lime Suru GLOW, Material Black Pistachio Suru, Avidity Dusk Mixed Suru, FF-BlackGreen and FF-Flamengo-RJ-BR. Twenty-three sets in the picker now.
	- Opened: n/a
	- Closed: 20260819-160351
	- All SVG, all trimmed to the names a file manager asks for, and all inside the executable - the whole icon payload is 6.6 MB, so nothing needed to be a separate download after all.
	- Three new fetch shapes were needed: a repository that keeps one theme family per branch, six themes out of one sparse checkout, and two that ship the icons as a tar committed inside a repository of something else.
	- Buuf is deliberately not included. It is CC BY-NC-SA, and the NonCommercial term rules it out of anything shipped and out of the repository. It is worth having, so `filesystem/` explains where to drop it and gives a one-line fetch for it.
	- Three of the twelve carry no licence file upstream and are shipped on weaker evidence than the rest. Each one is named, with what it rests on, in `vendor/README.md` - worth a look before a release.

- ✅ A gallery of every icon set in the README, four icons each on a light and a dark background, plus how to drop your own in. Rendered by `cicd/utility/icon-gallery.py`; re-run it when the set list changes.
	- Opened: n/a
	- Closed: 20260819-160351
	- Each icon is rasterised on its own before being placed. Several sets colour themselves through a stylesheet keyed on a class name they all spell the same way, so pasting their markup into one sheet made six differently coloured sets come out identical - and renaming the classes apart made them all come out black.

- ✅ `filesystem/` - a tree mirroring where things land on disk, so a folder can be copied straight across. Carries the icon and widget drop-in folders, what they are called on each platform, and the two optional `index.theme` keys that tell the picker which modes a theme suits.
	- Opened: n/a
	- Closed: 20260819-160351

- ✅ Windows icon sets: one per Windows generation, all with yellow folders.
	- Opened: n/a
	- Closed: 20260819-145557
	- Luna (XP) and Aero (7) were already ours; Metro (10) and Mica (11) are new, so every bundled Windows widget theme now has icons drawn to match it. The picker pairs them automatically.
	- Folders are yellow in all four. Aero's were blue, which is not what Windows 7 shipped, and a yellow folder is the one colour that reads on a light background and a dark one alike.
	- The XP and 7 folders were too shallow to read as folders at a glance; the body is taller in every era now.
	- The folder itself is drawn per era rather than shared - chunky and outlined for XP and 7, flat and square for 10, rounded with the front panel falling away for 11. It is the icon a Windows generation is recognised by.
	- The vendored Fluent icon set is gone with them: it drew blue folders and looked nothing like Windows 11, and Mica now covers that style. The Fluent *widget* theme stays. About 390 KB and 179 files lighter.

- ✅ Every bundled SVG run through a size pass: 2.1 MB of icon art down to 1.8 MB, and nemo's own artwork from 142 KB to 50 KB.
	- Opened: n/a
	- Closed: 20260819-145557
	- Numbers in path data are rounded to a step finer than a two-thousandth of the icon, which is under a tenth of a pixel at any size one is drawn. Colours fold to their short form and unreferenced ids go.
	- Multipliers - transform matrices, gradient vectors - are deliberately left alone: rounding a scale factor moves everything it touches, which is visible where rounding a coordinate is not.
	- All 983 icons were compared before and after. One differs at all, by an amount invisible side by side. Checking caught a real fault first time round: an arc's two flags can be written with nothing between them, and reading path data as a plain run of numbers swallows one and silently reshapes the glyph.

- ✅ Default settings changed: folder expanders on in list view, binary size prefixes (KiB/MiB), and thumbnail visibility inherited from the parent folder.
	- Opened: n/a
	- Closed: 20260819-141014

- ✅ List columns trimmed to one row per idea.
	- Opened: n/a
	- Closed: 20260819-141014
	- Three dates, the same three everywhere: Date Created, Date Modified (on by default) and Date Read. The "- Time" twins of the first two are gone; they showed the same instant a second way. The times themselves come from whatever each OS keeps them in, so nothing here is per-platform.
	- MIME Type and Detailed Type are no longer offered - neither reads as anything but debug output beside the plain Type column. Off behind a named switch in the source rather than deleted, since the underlying values are still what the properties window and the sort menu use.

- ✅ Appearance page: picking a Style now moves the Icons choice to match it, so a Windows 11 window frame no longer comes with macOS icons. Where a style has no icon set of its own the icons stay put. The note about drop-in theme folders sits further down the page, clear of the two pickers.
	- Opened: n/a
	- Closed: 20260819-141014

- ✅ "System default" in both theme pickers now reads "Nemo Anywhere" - on the bundled targets it is the app's own look, not the platform's.
	- Opened: n/a
	- Closed: 20260819-141014

- ✅ Settings belong where each platform keeps them: `%APPDATA%\nemo-anywhere` on Windows, `~/Library/Application Support/nemo-anywhere` on macOS. Linux and BSD keep `~/.config`. Themes stay where they were.
	- Opened: n/a
	- Closed: 20260819-105607
	- A folder left in the old place is moved across on first run, so nobody starts from defaults.
	- Covered by a test over both roots.

- ✅ The Windows executable takes too long to start. 14.2s down to 3.4s, and the executable from 39.8 MB to 33.5 MB.
	- Opened: n/a
	- Closed: 20260819-122828
	- Measured first: the packed single exe reached even `--version` in 14.2s against 0.9s for the same build as a plain folder, and all of the difference is spent before our own code runs. The packer charges about 2.8 ms for every file it carries, and the bundled themes were a couple of thousand of them. The packer's own compression and mapping settings were measured and change nothing.
	- The bundled themes now ride inside the executable as one compiled-in resource instead of ~2,200 loose files. The sysroot's full Adwaita and its legacy set - 2,693 files to answer the ~180 names we ask of them, plus 33 X11 cursors that do nothing on Windows - are replaced by our own trimmed copies. The whole folder went from 4,840 files to 152.
	- Trimming Adwaita turned up three faults in the theme resolver that had been quietly costing every bundled theme icons, `emblem-symbolic-link` among them - the one every symlinked file in the view wears. All the bundled themes were rebuilt.
	- A splash appears while it starts, drawn with the platform's own toolkit because it has to be up before GTK is. It lists what startup is doing in a ten-line window that scrolls smoothly, and leaves the moment the real window has drawn.
	- The window itself is now shown at its remembered size and place as soon as it has somewhere to be, rather than after the first folder resolves. The splash goes when the folder has finished listing or a second after the view is up, whichever comes first - a big folder can take twenty seconds to list and there is no sense covering a window that is already usable.
	- Found on the way: the app had never brought its own window to the front on Windows. Showing a window maps it without activating it, so it opened behind whatever you were looking at; on Linux the window manager focuses new windows itself, which is why it had never shown. Fixed.
	- The remaining 2.5s over a plain-folder launch is the packer's own fixed cost and would need a different packer to reach.

- ✅ Dimmer highlight of mouseover line. It can easily get confused with line selection.
	- Opened: n/a
	- Closed: 20260802-015402
	- The hover tint on a file-pane or tree row is dimmed to well under half what the theme sets, and only on rows that are not selected.

- ✅ Drag and drop onto a path button, and a fuller right-click menu on one.
	- Opened: 20260802-011216
	- Closed: 20260826-103001
	- Dropping onto a path button already worked, and still does.
	- The right-click menu was the short location one. It gained Open, Open in Terminal, Open as Admin and New Folder, so a path segment behaves like the folder it names.
	- New Folder is only offered on the segment for the folder being viewed, and creates inside it. On any other segment it is greyed.

- ✅ Ship with "Copy path(s)" script from current nemo install.
	- Opened: 20260724-091054
	- Closed: 20260820-055722
	- Built in rather than shipped as a script, so it needs no interpreter, no clipboard helper and no per-platform install step.
	- On the selection menu, the background menu (the folder being viewed) and a breadcrumb segment; also on the Edit menu, with Ctrl+Shift+C.
	- Copies the native path of each selected item, one per line, unquoted, with no trailing newline - the line ending being the local one, so a paste into cmd or notepad lands as separate lines.
	- Anything with no local path (a remote share) contributes its uri instead, and a recent or favorites entry resolves to the file it stands for rather than copying a virtual uri.
	- Label follows the count: "Copy Path" for one, "Copy Paths" for several. Show/hide checkboxes in Preferences like the other context-menu items.

- ✅ Right-click "Compress...": a cross-platform way to archive the selected files and folders.
	- Opened: n/a
	- Closed: 20260820-153813
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
	- Opened: n/a
	- Closed: 20260821-133318
	- Four lines in `settings.shcl` under `archive` - create and unpack, for each of the two programs - each with `{{PLACEHOLDER}}` markers for the parts the app fills in. Point one at a different build, add a switch we do not offer, or work around a version that spells something its own way.
	- Every switch the Compress dialog can turn on has a marker of its own, so an edited line keeps the dialog working. Leave one out and the app says which control has gone quiet.
	- Clearing a line puts the shipped one back rather than running nothing, and a line that cannot be read is refused outright rather than half-run.
	- A password is handed to the program as a value, never written into the settings file.
	- `{{LIKE_THIS}}` is now the convention for any setting that needs a placeholder. Braces because no shell or command prompt expands them, so a line can be pasted somewhere to try it out and come back unchanged.

- ✅ Right-click "Extract" for the archive formats we recognize, including shelling out to 7z or rar.
	- Opened: n/a
	- Closed: 20260821-064823
	- Three items on the selection menu and the Edit menu, shown only when everything selected is an archive: "Extract Here", "Extract Each to Its Own Folder" (singular when one is selected) and "Extract To..." with a folder chooser. Show/hide them in Preferences like the other context-menu items.
	- "Extract Here" unpacks exactly what the archive stores, so one made from a folder brings that folder with it and lands in one place. The folder-each item is the answer to an archive that would otherwise scatter its contents over the folder being viewed.
	- Reading covers far more than writing does: the tar, zip, 7z, rar, cab, lha, cpio, xar and iso families and the bare compressors all open with nothing installed.
	- A 7z or rar command is reached for when the built-in library will not open the file - a multi-volume set, or headers it cannot decrypt. Both are tried in turn, since a program being installed is no promise it can read the file.
	- A protected archive asks for its password once, and reuses it for the rest of the selection.
	- Collisions ask the same question copying asks, with the same answers - skip, duplicate, rename, replace, and applying that answer to everything after it. A folder arriving on a folder merges without asking. The prompt says which archive the incoming file came from, since several can be unpacked at once.
	- An entry whose stored path climbs out of the folder being unpacked into, or names a drive, is put back inside it.
	- Unpacking runs as a normal background job: it shows in the same progress popup as copying and can be cancelled.

- ✅ Depend on Explorer as little as possible.
	- Opened: n/a
	- Closed: 20260818-155550
	- Audited every place the Windows build reaches into the shell. The only one that handed work to Explorer was a "show this file in the file manager" call, which asked Windows for the default handler for a folder - Explorer, by definition.
	- It was already unreachable: the only caller sits behind a desktop-view check that went permanently false when the desktop shell was removed. On Windows it would also have been asking for a handler that Windows does not answer for - nothing is registered for a folder as a type.
	- Removed, along with its declaration. Nothing in the tree launches Explorer now.
	- What remains is in-process and unavoidable: the recycle bin and `.lnk` files are shell APIs called inside our own process, with no Explorer involved. Two `ShellExecute` calls stay for good reasons - one launches the terminal the user chose (found on PATH, not via the shell's associations), the other relaunches our own executable elevated, which is the only way to ask for elevation.

- ✅ Code review 20260815 - architecture and UX notes.
	- Opened: 20260815-154746
	- Closed: 20260817-210917
	- Observations and suggestions rather than defects. Not individually reproduced.
	- ✅ Item 110. Two separate desktop-terminal fallbacks disagree: "Open in Terminal" honors the configured terminal, launching a terminal app does not.
		- Fixed: both paths fall back to the same scan of known terminals, so neither silently does nothing.

	- ✅ Item 111. Localization is effectively dead on Windows and on relocated installs; the locale directory is baked at build time and no packaging step installs or points to it.
		- Fixed: data, translations and helper programs are found relative to the running program, with the built-in path as a fallback.

	- ✅ Item 112. Windows drive roots are labeled bare, with no volume label.
		- Fixed: the volume label is shown ahead of the drive letter.
		- Verified on Windows: the sidebar reads "Windows (C:)" and "Extra (K:)" against the real volumes on this box.
		- But only the sidebar was covered - see the drive-root naming item under Done - Bugs.

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

- ✅ Ultra-portable Windows: a single self-contained executable.
	- Opened: 20260730-203115
	- Closed: 20260802-144622
	- ✅ No separate library folder - pack the runtime into one `.exe` (in-memory virtual FS, e.g. Enigma Virtual Box).
		- A pack step flattens the staged bundle into the same layout the release zip uses, which double-click-runs on its own, then packs the lot into one executable. It is stage five of the Windows pipeline.
		- The font-rendering settings are set inside the executable now, so no launcher is needed for the native text look.
		- Verified: a 167 MB bundle packs to one 38.7 MB executable, which runs on a bare PATH and stays responsive.
		- Fixed the leftover console window on launch. The executables were linked as console programs, which is the default, so Windows opened a terminal before the window appeared. They are graphical programs now, and the version output still works when piped.
	- ✅ One binary only - Windows builds just the one executable. The connect-server and open-with dialogs already ran inside it, and the extensions lister has nothing to list with no plugins, so all three helpers are Linux-only now.
		- The extension library went in with them. With no external plugins on Windows the executable was its only reader, so there is no separate library beside it any more. Linux keeps it shared, for third-party extensions.
	- ✅ No external plugin loading on Windows (a bad plugin must never hang the app); keep the extension-management UI in-exe.
		- The plugin folder is never read on Windows, so a stray library cannot load and hang the app. The plugins tab in Settings still appears, listing nothing.

- ✅ Windows look: make it feel native even though it isn't Explorer.
	- Opened: 20260730-203115
	- Closed: 20260818-201822
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
	- Opened: n/a
	- Closed: 20260804-205711
	- Done: GSettings replaced outright rather than kept over a SHCL backend, so no compiled schema is installed or shipped. All 168 settings, ~300 call sites, 84 change handlers and 16 property binds moved over.
	- Done: the file holds only non-default values, carries each key's description as a comment, and is re-read while running so a hand-edit applies immediately.
	- Done: a schema file ships beside the app so `shcl check --schema` validates a hand-edited config (catches typos and bad values).
	- Done: the `compat.*` fallback schemas are gone; desktop-owned settings (terminal, recent files, 12/24h clock) are read from the desktop where it publishes them, ours otherwise.
	- Note: settings do not carry over from a pre-1.0 install - nothing left can read the old store. Fresh defaults on first run after upgrading.
	- Note: nemo actions can still name any GSettings schema in a condition; that reads other programs' settings and is unaffected.

- ✅ No autorun, ever, on any platform - not even an option. Notice a new drive; never run anything off it. Remove the autorun-software helper and its media-autorun path.
	- Opened: 20260730-203115
	- Closed: 20260802-002013
	- Done: the autorun-software helper, its menu entry, and the "prompt or autorun programs" preference are gone.
	- Done: the inserted-media bar never offers to run software from media. Other media notices (audio CD, photos) unchanged, and automount / auto-open still work - drives are noticed, nothing runs.

- ✅ Native Windows shortcuts: create `.lnk` files, the Windows analog of `.desktop` launchers.
	- Opened: 20260725-153058
	- Closed: 20260803-135051
	- ✅ Create: "Make Link" and the drag "_Link Here" now write a `.lnk` shell shortcut on Windows (via `IShellLinkW`), in place of the POSIX symlink the win32 file layer can't make. Round-trip verified by a test that loads the shortcut back through the shell.
	- ✅ Follow on open: opening a `.lnk` now follows through to its target - a folder navigates in place, a file opens as if the target were double-clicked. Reading the target round-trips through the shell (test-verified).

- ✅ Ship the app's own icons and data files on Windows.
	- Opened: 20260725-153058
	- Closed: 20260826-103001
	- Cause: the data dir was a compile-time absolute Unix path, so the sort-menu icons, the eject icon and the emblem art did not resolve on Windows.
	- Fixed: the artwork rides inside the executable as a compiled-in resource, and the data, translation and helper-program folders are found relative to the running program, with the built-in path as a fallback.

- ✅ Real-Windows validation pass.
	- Opened: 20260724-140849
	- Closed: 20260826-103001
	- Covers: trash, network browsing, single-instance, default-app setting, the Windows half of the installer, elevated relaunch (UAC prompt), keyboard shortcuts.
	- Note: moving a file to the trash raises a Windows confirmation dialog of its own on this box, on top of ours. Worth deciding whether ours should stand down there. The test that hit it now skips that step unless asked for it.
	- Done on real Windows: the recycle bin end to end, network browsing against this box's own shares, single instance and location forwarding, the installer's install/reinstall/uninstall round trip, and elevated relaunch. Each of the code-review items was re-checked here.
	- Found doing it, and fixed: the whole compiled-resource bundle was missing from the Windows build, so there was no menu bar at all; a drive root was named three different ways; "Set as default" failed silently forever; the installer read a prerelease version as the release it precedes.

- ✅ Get release binaries onto the host, plus an optimized buildtype.
	- Opened: 20260730-185314
	- Closed: 20260804-133646
	- ✅ Done: host dogfood path proven. Release build staged in the container, copied out to a self-contained folder, launched via a small wrapper.
	- ✅ Done: Linux release lane at `cicd/linux/release.bash` - optimized stripped build on an Ubuntu 22.04 box (the glibc floor is what the binary is built against), staged into a relocatable prefix, packed as the tarball plus the sums file.
	- ✅ Done: artifacts come out under the names the installers look for, and the artifact dir is wired in `config.bash` so `utility/release.bash` verifies and attaches them.

- ✅ Single-exe packaging stage in `cicd-win.ps1` - pack the staged DLL closure into one portable `.exe`.
	- Opened: 20260730-203115
	- Closed: 20260804-095855
	- Done: `cicd/win/pack-portable.ps1` flattens the bundle and packs it with Enigma Virtual Box into one self-contained exe; wired as cicd-win stage 5.

- ✅ Windows exe signing groundwork.
	- Opened: n/a
	- Closed: 20260804-095855
	- ✅ Embedded VERSIONINFO in the exe (real publisher/version metadata; a blank-metadata binary scores worse with AV heuristics and looks unfinished in Properties).
	- ✅ Local `signtool` signing scaffold in cicd-win stage 5 - env-driven, no-op until a cert is configured (fits a token/store cert: Certum OSS, Azure Trusted Signing, or a commercial EV).

- ✅ Publish the Windows `.zip` alongside the single exe. `install.ps1` only ever looks for the contract-named zip, so on Windows the one-liner installer had nothing to fetch even though the release carried a working exe.
	- Opened: n/a
	- Closed: 20260804-232326
	- Done: `cicd/win/pack-zip.bash` builds it from the cross build, and it ships from `v1.0.0-beta2` on.

- ✅ Don't continuously spam stdout/stderr with meaningless debug messages.
	- Opened: n/a
	- Closed: 20260802-012711
	- Cause: on Windows, any file type without a registry MIME mapping fell through a wildcard and got a doomed image-thumbnail attempt - two warnings per file, every folder browsed. A few one-shot startup notices added to the noise.
	- Fixed: unknown types are no longer treated as thumbnailable, the image loader gets a real MIME type, and the per-file / startup notices are debug-level now (visible with G_MESSAGES_DEBUG when wanted).
	- Verified: browsing a mixed folder of images and non-images runs silent; image thumbnails unaffected.

- ✅ Add a C formatter/linter gate and wire it into the format/lint stages.
	- Opened: 20260725-153058
	- Closed: 20260802-011216
	- Done: check-only cppcheck over the changed C files only, wired into both pipelines (Windows stage 1 + gate, Linux lint stage). No in-place formatter - a full-tree reformat of the inherited code would bury history in churn.
	- Done: a box without cppcheck skips with a warning instead of blocking a push.

- ✅ Change default settings:
	- Opened: 20260724-091054
	- Closed: 20260802-004759
	- ✅ List view, 66% size.
	- ✅ Ask before moving items to trash.
	- ✅ Date display in ISO format.
	- ✅ Showing owner, group, and perms.
	- Done: new out-of-the-box defaults - list view at 66%, trash moves ask first, ISO dates, owner/group/permissions columns visible. Existing installs that changed a setting keep their value.

- ✅ Remove features:
	- Opened: 20260724-091054
	- Closed: 20260802-004759
	- Option to display date in monospace font.
	- Done: the date font style option, its setting, and the mono-font matching are gone. Dates use the regular font.

- ✅ Allow select and copy of error message dialogs.
	- Opened: 20260724-102941
	- Closed: 20260802-004759
	- Done: the message text in the stock error/question dialogs is selectable, so it can be copied. The expandable details text already was.

- ✅ "Name" column should always be as large as possible, the other columns don't auto-adjust. When window grows or shrinks, the Name column does too to as wide as possible without pushing other columns off.
	- Opened: 20260724-091054
	- Closed: 20260727-001739
	- Cause: the Name cell asked for a 40-character width, which acted as a floor the column could never shrink past, so a narrowing window pushed the trailing columns off instead.
	- Fixed: dropped that request, so Name now gives space back down to its existing minimum. Long names ellipsize as before.
	- Verified: at 600px wide all four columns fit where Date Modified used to be cut off; at 1500px Name still takes all the slack; shrinking back from wide re-fits correctly.

- ✅ Wine launcher.
	- Opened: 20260724-091054
	- Closed: 20260725-153058
	- Fixed: launches detached, so the script exits and returns immediately.
	- Fixed: initial directory is the user's home if it exists, falling back to the drive root, then C:\.

- ✅ Installer script(s) - one-liner install from a shell, for every target.
	- Opened: n/a
	- Closed: 20260723-133832
	- Done: two standalone installers. The bash one covers Linux, BSD, WSL, and macOS; the PowerShell one covers all of those plus Windows.
	- Done: both take channel, target, and architecture options; print the plan and wait for a yes; verify the download checksum before unpacking; replace an existing install in place; and reverse themselves with an uninstall option.
	- Done: installs as a folder plus a menu entry and a name on PATH. User install is the default; only the system-wide install escalates, and says so in the plan.
	- Done: README gained an Installation section. The release-asset naming the installers depend on is in design.md under Delivery.
	- Verified: end to end on the unix side against a stand-in releases service - channel and asset resolution, checksum pass and tamper-fail, install, reinstall, uninstall, prompt accept and decline, and both installers leaving identical results.
	- Note: the Windows half still needs the real-Windows validation pass.

- ✅ Dogfood launcher script.
	- Opened: n/a
	- Closed: 20260725-153058
	- Done: keeps date-stamped copies of the latest build in a local pool, prunes aged-out copies not in use, launches the newest with args passed through.
	- Done: one cross-platform PowerShell script for Linux and Windows. Working copy deployed to the common util dir.
	- Done: launches detached and returns immediately. App output goes to a log in the target dir, so it never holds the calling console open.
	- Done: a source on a network share is written off after a moment rather than blocking the launch while the network gives up in its own time.
	- Done: a launch with nothing to copy went from nine seconds to one. Working out which programs are running was the whole cost on Windows, and it was being done twice.
	- Fixed: the newest copy could age out and be re-fetched on every run whenever the source build was itself older than the pruning cutoff.
	- Fixed: copies left by the pre-single-exe layout were invisible to the pruning and sat there for good.

- ✅ Adopt the local-only delivery model: dev = integration target, main = release-only (dev to main = release cut). Feature branches merge --no-ff into dev.
	- Opened: n/a
	- Closed: 20260718-195609
	- Note: copied as high-level concepts (not language tooling) from the sibling project.

- ✅ Stand up the local pipeline: engine, config, git backup+publish, release helper, and a pre-push merge gate.
	- Opened: n/a
	- Closed: 20260725-153058
	- Verified: container build + smoke test, and backup+publish, all pass.

- ✅ dbus / single-instance handling.
	- Opened: 20260718-155447
	- Closed: 20260725-153058
	- Verified: single-instance works unchanged on Windows. A second launch hands its arguments to the first. No per-platform gating needed. Details in design.md, "Decisions along the way".
	- Fixed: a bus-less environment (headless or minimal system) crashed the internal file-operations service. It now skips setup cleanly. Regression test added, passes on both platforms.

- ✅ Context-menu actions: open in terminal, open elevated, launchers.
	- Opened: 20260718-155447
	- Closed: 20260724-143335
	- Done: on Windows, "open in terminal" opens the native console at the folder, and "open elevated" relaunches the app through the normal elevation prompt. Linux paths unchanged. Menu labels are per-platform.
	- Note: `.desktop` launcher files already degrade cleanly on Windows. Native `.lnk` creation is its own item, since done.

- ✅ Thumbnails, icon theme, and default-app association per platform.
	- Opened: 20260718-155447
	- Closed: 20260724-150328
	- Done: the portable file-and-app layer already carries most of this. The real gaps were the two icon bugs (see Done - Bugs) and packaging the thumbnailer tools with the Windows runtime.
	- Verified: default-app lookup, launch, and set-default work on Windows through the portable layer. Image thumbnails render.
	- Note: on Windows 10/11 the per-user default-app choice may not stick. Not worked around.

- ✅ gvfs replacement or scope-out (mounts, network, trash).
	- Opened: 20260718-155447
	- Closed: 20260725-153058
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
	- Opened: 20260718-155447
	- Closed: 20260722-201512
	- Verified: the existing operations engine drives all core operations correctly on Windows - copy, conflict, overwrite, recursive folder copy, move, rename, delete. No porting needed. Probe test added, runs on both platforms.
	- Fixed: link-creation options are hidden on Windows (no symlink support there). The permissions tab, columns, and change-permissions paths are hidden too, since Windows fabricates the mode bits.

- ✅ File monitoring via portable backends.
	- Opened: n/a
	- Closed: 20260725-153058
	- Verified: change events deliver through the native monitor backends on both platforms. Nothing to port.

- ✅ Choose and stand up the Windows toolchain.
	- Opened: n/a
	- Closed: 20260725-153058
	- Done: cross-compile from Linux with mingw-w64, smoke-test under wine, in a dedicated container. Details in design.md, "Building (Windows cross)".

- ✅ Get GTK3 + GLib/GIO building on the chosen toolchain.
	- Opened: 20260718-155447
	- Closed: 20260725-153058
	- Done: cross configure comes up clean with all deps resolved. Unix-only deps guarded out per platform.

- ✅ Compile on Windows, stubbing/excluding hard platform deps.
	- Opened: 20260718-155447
	- Closed: 20260725-153058
	- Done: the app, its helpers, and the extension library all build and link clean, and run under wine. Linux stays green.
	- Done: POSIX gaps closed via a shared compatibility header plus per-site guards.

- ✅ Launch on Windows and browse the local filesystem.
	- Opened: 20260718-155447
	- Closed: 20260725-153058
	- Done: the GUI comes up under wine and browses the local drive - sidebar, icon view, per-type icons, item count, free space.
	- Fixed: startup abort caused by desktop settings schemas that only exist on Cinnamon/GNOME. Bundled neutral fallbacks now cover them (see design.md, "Decisions along the way").
	- Done: headless GUI smoke test scripted.

- ✅ Map drive letters / roots into the location model.
	- Opened: 20260718-155447
	- Closed: 20260725-153058
	- Done: on Windows, each fixed drive is a first-class sidebar root with a disk-usage bar, replacing the single Unix filesystem root (meaningless on Windows). Removable and network drives keep the normal devices path, which carries eject.
	- Verified: drives show as roots and open to their contents.

- ✅ Remove desktop management entirely (Nemo Anywhere is a file manager, not a desktop shell).
	- Opened: n/a
	- Closed: 20260719-181630
	- Done: the desktop binary, desktop windows, and the Cinnamon session coupling all deleted. Kept the launcher-file editor and the monitor-geometry helper, both real file-manager features.

- ✅ Isolate xapp / cinnamon-desktop coupling (reimplement portably, not just disable).
	- Opened: n/a
	- Closed: 20260719-190803
	- Done: favorites, thumbnails, tray icon, and the icon chooser all reimplemented portably. Details in design.md, "Decisions along the way".

- ✅ Prove a de-Cinnamon Linux build that runs standalone (no xapp, no cinnamon-desktop) on any desktop or none.
	- Opened: 20260718-155447
	- Closed: 20260719-190803
	- Verified: builds and links with neither library. Favorites and thumbnails work on the standalone build.

- ✅ Isolate per-file view metadata keys so the two builds don't share view state on the same files.
	- Opened: 20260718-174619
	- Closed: 20260722-172504
	- Done: view/layout keys and the favorite markers carry the app name. Keys other file managers also read (custom icon, emblems, annotation, backgrounds) stay shared on purpose.

- ✅ Build upstream as-is on Linux (meson) to confirm a known-good reference.
	- Opened: n/a
	- Closed: 20260718-155447
	- Done: builds and runs clean on stock Debian 13, in a container (this dev box has newer mixed libs).

- ✅ Note the exact dependency set and versions that produce a working build.
	- Opened: n/a
	- Closed: 20260718-155447
	- Done: recorded in the build notes outside the repo.

- ✅ Reorganize into a clean project structure; build consolidated under `source/`, root kept lean.
	- Opened: n/a
	- Closed: 20260718-161018
	- Done: meson project moved under `source/` with its internal layout intact. Builds and runs green.

- ✅ Rebrand to "Nemo Anywhere" / `nemo-anywhere` so it co-installs and runs alongside upstream Nemo without conflict.
	- Opened: n/a
	- Closed: 20260718-174619
	- Done: renamed the installed identity only (binaries, service names, settings schema, config/data dirs, menu entries, icons). Internal code identifiers left as-is; no clash.
	- Done: settings fully isolated from upstream Nemo. Doesn't claim the freedesktop file-manager service when upstream holds it.
	- Verified: staged install has no filename collisions with upstream. Window runs headless.

- ✅ Install nemo-anywhere and upstream Nemo into separate prefixes and confirm both run simultaneously without conflict (real side-by-side runtime proof).
	- Opened: 20260718-191700
	- Closed: 20260719-181454

- ✅ Clean detached baseline from linuxmint/nemo 6.6.4 (no upstream commit history).
	- Opened: n/a
	- Closed: 20260718-155447

- ✅ Fork branding + provenance (README, fork.md), GPL-2.0-only.
	- Opened: n/a
	- Closed: 20260718-155447

- ✅ Name chosen: nemo-anywhere.
	- Opened: n/a
	- Closed: 20260718-155447

- ✅ Create the GitHub repo and push.
	- Opened: n/a
	- Closed: 20260725-153058
	- Done: created public.

- ✅ Strip upstream CI - keep the repo clear of unrelated automation.
	- Opened: n/a
	- Closed: 20260725-153058
	- Done: workflows and issue templates removed in the fork-setup commit.

### Future and/or deferred

- ✋ Make the CICD test gate resilient to a down or absent docker daemon.
	- Opened: 20260721-222522
	- Done: build and smoke steps go through a wrapper that probes the daemon first.
	- Done: an environmental miss (docker absent, daemon down, container gone) skips with a warning instead of blocking the push. A real build or test failure still gates. A strict mode turns a miss back into a hard failure.
	- Note: the daemon needs root to start, so the unattended hook never auto-starts it. The skip message shows the manual command.
	- Verified: gate passes normally, and skips cleanly when docker is unreachable.
	- ✋ Revisit whether one container-Linux smoke test is a meaningful gate once Windows/cross lanes exist.

- ✋ Code review 20260815.
	- Opened: 20260815-154746
	- ✋ Item 101. Carriage returns in settings values are not escaped and are stripped on reload.
		- Deferred: fixing it means changing both halves of the vendored settings parser and with them the on-disk escaping, for a character no setting ever contains.
	- ✋ Item 106. Sorting by a string attribute allocates and formats both values on every comparison.
		- Deferred: doing it properly needs a per-file cache of the formatted value with its own invalidation - the same machinery as the icon render cache, for much less gain.
	- ✋ Item 109. Platform code is split two ways: dedicated Windows modules alongside inline platform blocks in large shared files.
		- Deferred: Need to decide whether this is "convention" or a "bug".

- ✋ Windows: "Open in terminal" should refer to an ordered list of shells and terminals in settings (if there's not a standard Windows way). At install time - and at launch in a background thread once the UI renders and settles:
	- Opened: 20260802-095853
	- Check for a hardcoded list of terminals. For each that exist, add them to config. (Add nonexistent ones too, commented out.) For each, prefer to launch in what's installed, in this order of preference: SilkTerm, Windows Terminal, conhost. User can override which terminal is opened, for each shell.
		- Powershell 7
		- WSL2 distros
		- WSL1 distros
		- NuShell
		- PyCmd
		- CMD.exe
		- Powershell 5

### Canceled

- 🚫 Keyboard shortcuts do nothing in the Windows build when run under wine.
	- Opened: 20260725-172648
	- Closed: 20260802-001535
	- Cause: wine has no keyboard layout DLL, so GTK can't turn a keypress into a key value and no shortcut ever matches. Plain keys (arrows, typing) still work, and so do the menus and mouse.
	- Note: a wine limitation, not our code. Expected to work on real Windows - added to the real-Windows validation pass.

- 🚫 Launching `app\nemo-anywhere.exe` straight from the dogfood folder throws missing-dll dialogs (libcairo-goobject-2 and friends) - the exe has to go through the root `nemo-anywhere.vbs`, which wires the dll path. Punted: the single-exe work removes the whole launcher/dll-folder arrangement.
	- Opened: n/a
	- Closed: 20260802-101032

- 🚫 In find mode, shrink the Name column to fit and let Location grow with the window, then put it back on leaving find mode.
	- Opened: 20260730-112038
	- Closed: 20260822-075741
	- Superseded by the column-width work: in find mode Name and Location split the row one-third/two-thirds, and an adjusted split is remembered.
