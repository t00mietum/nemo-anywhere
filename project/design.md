<!-- markdownlint-disable MD007 -- Unordered list indentation -->
<!-- markdownlint-disable MD010 -- No hard tabs -->
<!-- markdownlint-disable MD033 -- No inline html -->
<!-- markdownlint-disable MD055 -- Table pipe style [Expected: leading_and_trailing; Actual: leading_only; Missing trailing pipe] -->
<!-- markdownlint-disable MD041 -- First line in a file should be a top-level heading -->

<!-- TOC ignore:true -->
# nemo-anywhere design

What the project is for, and the decisions behind it. Companion to [backlog.md](backlog.md), which tracks the work itself.

<!-- TOC ignore:true -->
## Table of contents

<!-- TOC -->

- [What and why](#what-and-why)
- [Goals](#goals)
- [Fork decisions](#fork-decisions)
- [Architecture](#architecture)
	- [Software stack](#software-stack)
	- [Code layout](#code-layout)
	- [Data flow](#data-flow)
	- [Execution flow](#execution-flow)
	- [When it crashes](#when-it-crashes)
	- [Configuration and persistence](#configuration-and-persistence)
	- [File operations](#file-operations)
	- [Search](#search)
	- [UI](#ui)
	- [List view column widths](#list-view-column-widths)
	- [Appearance and themes](#appearance-and-themes)
	- [Platform integration](#platform-integration)
- [Building](#building)
	- [Linux](#linux)
	- [Windows](#windows)
	- [Open questions](#open-questions)
- [Testing](#testing)
- [Delivery](#delivery)
	- [Branches and the merge gate](#branches-and-the-merge-gate)
	- [Versions and build numbers](#versions-and-build-numbers)
	- [The pipeline](#the-pipeline)
	- [Reproducible builds](#reproducible-builds)
	- [Release artifacts and packaging](#release-artifacts-and-packaging)
	- [Installing](#installing)
	- [Dogfooding](#dogfooding)

<!-- /TOC -->

## What and why

This is a hard fork of linuxmint/nemo at its 6.6.4 release, decoupled from Cinnamon and from Linux-desktop assumptions so it runs standalone anywhere. (And it is already far ahead of Nemo 6.6.4 in terms of bug fixes and feature improvements, and new features.) Independent and divergent: no upstream contribution, no downstream sync. GPL-2.0-only.

Targets in order: Linux on any desktop or none, then Windows, then BSD, then macOS. One codebase; per-platform builds are labels, not separate projects.

Windows is the first not-Linux target because it forces the cleanest separation. Nothing Linux-specific can be assumed there, so the couplings show up as build errors rather than as things that quietly still work. A de-Cinnamon Linux build falls out of the same work.

## Goals

What the project is trying to be, roughly in priority order:

- Belong to no desktop. Nothing in the program assumes Cinnamon, GNOME, or even Linux, and it never draws or owns the desktop. It can sit beside whatever already does, original Nemo included.

- Run on any desktop OS, from one codebase. Linux on any desktop or none, then Windows, then BSD, then macOS.
	- "For Windows" are labels on builds, not separate projects.

- Keep what makes Nemo worth porting: Fast navigation, tree folder view in list mode, sane folder merging, proper bookmarks, useful and simple settings, and an extension API that still works.

- Be portable in the copy-it-and-run sense. On Windows that is one executable with the runtime inside it. On Linux it is a small folder using the GTK the distro already has.
	- If not obtained via provided installers or packages: Nothing required to be installed, nothing registered, no repository to add. Installers and distro packages exist for people who want them, but nothing depends on them.

- Make it hard to lose a file by accident. This is where the fork is willing to be less convenient than its ancestors, e.g.:
	- A drag that moves files says what it is about to do, and waits. (Because accidental mouse drag-an-drops - especially large ones across filesystems - are the bane of GUI file managers.)
	- Trash and delete jobs each write a line saying what was taken and what asked for it.
	- A copy/move/delete with no keystroke or click behind it, or one over a size threshold, asks first no matter what the preferences say.
	- A copy/move/delete over a size threshold, asks first no matter what the preferences say.

- Keep configuration in plain sight. One text file, readable and editable by hand, with no registry keys, no dconf, and no compiled schema to install. Hand-editing it behaves the same as changing the setting in the dialog.

- Fit each platform natively instead of pretending to be its file manager. Drive letters, the Recycle Bin, shortcuts, UNC paths and file associations are all done the way that platform does them.
	- Read the system's settings, don't rewrite them. File associations come out of the registry; the app's own overrides stay in the app's own config.

- Minimize dependencies. On Windows, for example, minimize depenedence on Explorer.

- Handle natively in own code (and or reliance on optionally-installed CLI tools), far more robustly than Nemo's reliance on external tools - and lack of really good tools:
	- Archive/extract.
	- Find.
	- Robust rename that surpasses Thunar Renamer and Directory Opus in functionality, simplicity, and repeatability (e.g. saveable templates). For media types, be at least as robust as "CamHauler" (formerly "Rapid Photo Downloader Pro" and may get yet another rename).
	- Smart duplicate file and directory finder and handler.

- Never follow links of any kind for deletes or moves. The copy portion presents user with options for how to handle links.

- Be tolerant of crashes. Each instance of Nemo Anywhere gets its own process (not just thread).

- Interoperate with OS-native file managers. (E.g. bidirectional copy/paste and drag-n-drop.)

- Hide or gray out what a platform cannot do, rather than failing at it. A missing runtime service should cost a menu entry, not a crash.

- Look presentable on a bare system. Icon sets and window styles are inside the program (for non-Linux OSes), so a fresh copy has no missing art and nothing to download.

- Start fast and stay small. A file manager gets launched dozens of times a day, and a slow one is noticed every time.

- Ship builds that can be checked. Reproducible from the commit they were built at, published with checksums, and cut by the same pipeline that runs on a developer's own machine.

- Stay "Nemo". Same lineage, same license (GPL-2.0-only), per-file attribution intact. Independent and divergent: nothing goes upstream and nothing is pulled back down.

- Deliberately out of scope: drawing the desktop, autorun of any kind on any platform, and migrating settings from a pre-1.0 install.

## Fork decisions

- Baseline is the 6.6.4 release tag, not master, so the starting point is known-good rather than a moving target. It was imported as a clean detached commit with no upstream history: lineage and attribution ride in [fork.md](../fork.md) and in the per-file copyright headers instead of in git ancestry.

- The name keeps "nemo" for discoverability and honest lineage, and adds "anywhere" for the portability and the belongs-to-no-desktop identity. Nemo Anywhere and the original Nemo can be installed and run on the same machine at once without conflicting, which is deliberate: separate config directory, separate settings file, app-private per-file keys.

- Version numbers start at 1.0.0 and are the fork's own, unrelated to the 6.6.4 code baseline.

- Desktop management is removed, not made optional. A file manager is not a desktop shell, and drawing the root desktop is where the deepest Cinnamon coupling lived: the `nemo-desktop` binary, the `org.Cinnamon` proxy, the per-monitor `x-nemo-desktop://` directory model. Cutting it outright was the cleanest first step and it benefits every target. Kept, despite the names: the `.desktop` launcher-file properties editor and the multi-monitor geometry helper, both ordinary file-manager features.

- xapp and cinnamon-desktop are reimplemented rather than compiled out, so the standalone build keeps favorites, thumbnails, tray feedback and the icon chooser instead of quietly losing them.
	- Favorites and the thumbnailer were adapted from their upstream implementations into `libnemo-private`, with provenance and licenses noted per file.
	- The tray icon uses GTK's own status icon. It is deprecated upstream but is still the only portable tray mechanism. Window taskbar progress was dropped: it is a Mint-only window-manager protocol with no portable equivalent.
	- The icon chooser is a file picker with an image preview. Browsing theme icons by name went with the old widget, which is an accepted simplification.

- The first runnable milestone was scoped to browse, copy, move, trash and delete. Everything else came after that worked.

## Architecture

### Software stack

- Language: C, built with meson and ninja. No C++ and no second language runtime.

- Toolkit: GTK 3, with GLib, GObject and GIO. GTK 3 rather than 4 because the fork inherits a large GTK 3 codebase and GTK 3 still has the better Windows story. The deprecated pieces still in use (the status icon, a few stock dialogs) are isolated and marked.

- Filesystem access: GIO everywhere, with native backends filling the gaps that have no portable answer - the Windows Recycle Bin, Windows network browsing, and Windows shell shortcuts.

- Other libraries: libarchive for reading and writing archives, libexif, libgsf and exempi for file property extraction, json-glib for the metadata store, and a single vendored header for the settings format. Deliberately absent: xapp, cinnamon-desktop, and GSettings for the app's own settings.

- Optional at runtime: gvfs on Linux, for network shares, trash and remote mounts. Where it is absent the affected entries hide themselves rather than fail.

Longer term the toolkit itself is the constraint. GTK 3 is no longer developed, keeps the project in C, and is weaker on Windows and macOS than the alternatives. Moving off it is a separate project ([Captain Nemo](https://github.com/t00mietum/captain-nemo)), not something this one attempts.

### Code layout

The repo root holds docs and licenses and little else. The buildable project is wrapped under `source/` with its internal GTK/meson layout intact, so meson is pointed there.

- `source/` - the meson entry point and all C sources.

- `project/` - this document and the backlog.

- `assets/` - fork-authored artwork.

- `vendor/` - third-party sources kept in tree, each with its own license and pinned origin.

- `utility/` - standalone helper scripts, actions and the cross-platform launcher.

- `filesystem/` - a tree mirroring where files land on disk, so a drop-in theme folder can be copied straight across.

- `cicd/` - the local build, release and publish automation. See [Delivery](#delivery).

- `.github/` - repo metadata. Distinct from `github/`, which is the working directory this file sits under.

Upstream kept everything at the root with decades of accumulated meta-files. The fork consolidated the build under `source/` and dropped what no longer serves a standalone cross-platform project: old changelogs, distro packaging, upstream CI.

Inside `source/` there are four layers, bottom to top, each depending only on what is below it.

- `eel/` - a small widget and utility library inherited from the fork's ancestry: string and GTK helpers, stock dialogs, the editable label and the canvas the icon view draws on. It knows nothing about files or settings, which is why the couple of desktop-integration helpers living here read the desktop's own settings directly instead of asking the config store.

- `libnemo-extension/` - the public plugin interface, and nothing else. The interfaces a third-party extension implements (menu provider, column provider, property page, info provider) and the small value types they exchange. It is a standalone shared library with its own headers, so it depends on GTK and on none of our other code.

- `libnemo-private/` - the model. Files and directories with their asynchronous attribute loading, the file operations engine, search, thumbnails, favorites, the settings store, the per-file metadata store, and the platform backends for trash, network and shell integration. No window or view lives here.

- `src/` - the application and its views. The GtkApplication, windows, tabs and slots, the icon, compact and list views, the sidebar and path bar, and the properties and preferences dialogs.

Platform-specific code is kept out of the shared files where it can be: `*-win32.c` modules for trash, network, shortcuts, clipboard, drag-and-drop and shell actions, plus a POSIX compatibility header that lets ordinary callers compile unchanged where the platform has no equivalent. Some large shared files still carry inline platform blocks. Settling on one convention is an open item.

### Data flow

A location is a URI throughout, and everything hangs off two model objects.

- `NemoDirectory` owns the list of files at one location and the machinery that loads them. Views ask for a set of attributes - names and sizes, mime types, deep counts, thumbnails - and the directory works out what is missing, issues the asynchronous requests, and reports each answer as it arrives.

- `NemoFile` is one file. Attributes arrive in stages, so a file starts with a name and fills in over time. It emits `changed` whenever anything about it moves, and every view redraws from that one signal, which is also why the caches added for redraw speed all invalidate there.

- Anything the filesystem does not store is layered on top. Per-folder view state, custom icons, emblems and favorite markers come from the app's own metadata store and are merged into the file's attributes as they load. On Linux a gvfs metadata daemon may supply the same keys; ours wins.

- Settings flow the other way. A read goes through one store to one file, a change emits a per-key signal, and the widgets bound to that key follow. An external edit to the file produces exactly the same signals as a change made in the UI.

### Execution flow

One process per window by default, one main loop each, and a firm rule that nothing slow runs on it.

- Startup registers the application, opens the settings store, and creates a window. It never hands the location to a copy already running; see [Platform integration](#platform-integration) for why.

- Directory loading, file operations, search and thumbnailing all run off the main loop: GIO asynchronous calls for anything touching a filesystem, worker threads for thumbnail generation and for the file operations engine.

- Work started off the main loop reports back on it. File operations own a progress object the UI observes, thumbnails hand back a finished image, a completed directory load emits `done_loading`. Callbacks outliving their object are the recurring hazard, so long-running work holds a reference and cancels on dispose.

- Debounce and coalesce rather than write or redraw on every event. Settings saves, metadata saves, window geometry and sidebar rebuilds all batch.

### When it crashes

A crash leaves a report. Without one there is nothing to work from: a windowed program on Windows has no stderr, so it used to disappear off the screen and that was the whole story.

- The report goes to `crash/`, beside the settings file. It gives the version and build, what killed the program and where, and the stack. The name carries when the run started, since working out the current time is not something the code can safely do at that point; the file's own timestamp is when it died.

- The same text goes to stderr, which is where a launcher log keeps it. Windows gets a message box as well, because a windowed build has no stderr for anyone to read.

- Frames are addresses, not names. The Windows build carries no debug database and the released Linux build is stripped, so a frame is only worth anything alongside the build it came from. Windows reports each frame at the address it was linked at, which is what `addr2line` takes directly. Linux writes the module, an offset in parentheses, and the address it happened to run at in brackets; the bare `+0x...` in parentheses is the one to hand over, and where a symbol name stands beside it instead the frame is already named.

- On the way out the signal is handed back, so a core file is still written and an attached debugger still stops. The frame it stops on is the reporter rather than the fault, one step further out than it would have been.

- The handler stays off the allocator, since a crash inside it is one of the cases to survive, but it cannot avoid locks altogether: reading a symbol takes the loader's on Linux, and naming a module takes it on Windows. What it does avoid is the worst of them - the Windows side walks the stack with the operating system's own unwinder rather than the symbol library, whose first act is to enumerate every loaded module.

- What is known for certain is written before the stack is collected, since walking a broken stack can fault again. A crash on a worker thread that runs out of stack is the one case that still reports nothing: the reserve that lets the handler run at all belongs to the thread that installed it.

- Reports do not pile up. The oldest are dropped at startup, and the first run after a crash notes in the log that one was left behind. On Windows that log line goes nowhere in a windowed build, which is what the message box at the time of the crash is for.

- `NEMO_NO_CRASH_HANDLER` installs nothing, and `NEMO_NO_CRASH_DIALOG` keeps the report while dropping the message box, for anything running unattended.

### Configuration and persistence

Settings are ours, in a file we own, in a format a person can read. No settings daemon, no compiled schema, no per-platform store to keep in step.

GSettings was replaced outright rather than kept as an API over a new backend. A backend would have been a fraction of the work and left every call site untouched, but it keeps a compiled schema to build, install and ship on every platform, which is the thing being got rid of. The full replacement moved about three hundred call sites, and change notification, property binding and enum mapping are ours to maintain now. Both kept the shape they had - a detailed `changed::key` signal, a `bind` with optional mappings - so the call sites read as they did before. The other accepted cost is that settings do not migrate from a pre-1.0 install, because nothing is left that can read the old store.

Four stores, each with its own lifetime.

Application settings live in `settings.shcl`, in whichever directory the platform keeps per-user configuration in: `~/.config` on Linux and BSD, `%APPDATA%` on Windows, `~/Library/Application Support` on macOS. A folder left by an older build is moved on first run.

- The file holds only what was actually chosen. A value equal to its default is dropped, so the file stays short and a later change to a default still reaches the user.

- Because of that the file alone would say nothing about what else there is, so everything unset is listed at the end, commented out, with the value in use and a one-line note wherever the name does not already explain itself. Uncommenting a line is the same as changing the setting in the dialog. Keys the app writes back itself - window size, sidebar width, the last state of a search toggle - are left off that list, since setting one by hand only gets it overwritten.

- Edits made while the app is running are picked up straight away, so hand-editing behaves like using the dialog.

- Types, defaults and allowed values live in one table in the code, and a matching schema sits beside the app so `shcl check --schema` can catch a typo in a hand-edited file. Keeping defaults central is deliberately against the config library's own per-call-site advice: with 168 settings, many read from several places, two call sites disagreeing about what a setting means when absent is a silent bug.

- A handful of settings are the desktop's to decide rather than ours: which terminal to open, whether the session remembers recent files, 12h or 24h clocks. Where a desktop publishes them we read its answer, and everywhere else our own value stands in. That is the only remaining use of the desktop settings database, it is read-only, and it never touches a schema of ours.

- A few settings are file-only, with nothing in Preferences. What a rename starts out with selected is one: the default selects the whole name, extension included, since a person pressing F2 usually means to replace the name outright and a re-typed extension is cheaper than one silently kept. The other behavior is a line to change, but it does not earn a control.

- Where a setting is a command line for another program, the parts we fill in are written `{{LIKE_THIS}}` - capitals between double braces. Braces because nothing expands them: the same line pasted into a shell or a command prompt to try it out comes back unchanged, where `%NAME%` would vanish on Windows and `${NAME}` would on Linux. Only the markers a setting declares are replaced, so anything else in braces passes through as itself and there is nothing to escape.

Bookmarks are the toolkit's own file on Linux and BSD, shared with every other GTK program there. On Windows nothing else reads that file and it sits in the local profile, so the list is kept beside the settings in the roaming one instead.

Per-folder view state - view mode, zoom, sort column, column layout - is app-owned and portable, in one file under the config directory. This replaced the Linux-only metadata service, so the behavior is now identical everywhere.

- Only a real per-folder choice is stored. A value that merely matches the current default is left out, so the folder keeps following the default if it later changes. Upstream stored it either way, which quietly pinned every folder ever opened.

- Changing a default in Settings also applies to folders already on screen. Folders not being looked at keep their own view and zoom until visited.

- Window size, position and maximized state are shared by every window and live with the application settings. They are written shortly after a move or resize settles rather than at close, so an abnormal exit does not discard them. With nothing saved yet a window opens at 1280x720 including its frame, with the side pane at about a fifth of the width.

The thumbnail cache is the fourth store and is not ours. It is the shared freedesktop cache: PNGs named by a hash of the file they were made from, under the user's cache directory, read and written by every file manager and image viewer on a Linux desktop, so a thumbnail made in one is already there in the next.

- Nothing ever removed one, so the folder only grew. It is swept now, at most once a day, on a worker thread a minute after startup, never on the path that draws a window.

- Three rules in order: a thumbnail whose file is gone, then anything unused for longer than the age allowed, then oldest-first until the rest fit the size allowed. Both limits are on the Preview page and either can be turned off. The defaults, 180 days and 512 MB, are what a GNOME or Cinnamon desktop already applies to the same folder, so on those desktops nothing changes and everywhere else something is finally minding it.

- A private database was considered and dropped. It meant a new dependency in three build environments, and on Linux it would have cost the sharing that makes the cache worth having. Growth was the actual complaint, and sweeping fixes that without giving anything up.

Settings are isolated from an upstream Nemo installed alongside: our own file, our own config directory, app-private per-file keys. A few genuinely shared per-file keys - custom icons, emblems, annotations - stay interoperable on purpose.

### File operations

Trashing and deleting are the two things a file manager cannot take back, so they are held to a higher bar than the confirmation preferences alone. This was settled after a copy of the app emptied a home folder with nothing anywhere to say why.

- Every trash and delete job writes one log line: how many items, which folder, the first item, the window, and the input event behind it - the key, the mouse button or the drop. That line is the record when something goes wrong.

- A job with no input event behind it always asks first, whatever the preference says, and the question says where it came from. A key, a click or a drop is what a person does. A timer, another program or another copy of the app is not, and the app should not act on those silently.

- A job of `confirm-many-items` or more asks even with confirmation switched off. Twenty by default, and zero turns it off. A slip that takes one file is a nuisance; one that takes a folder is a day.

- The dialogs keep the affirmative as the default button. Making Cancel the default was considered and turned down: the dialog is itself the pause, and a Cancel default is friction on every ordinary delete for people who chose to be asked.

Copying a link asks what should be at the far end. A link can stay a link or be replaced by what it points at, and neither answer is right every time, so the question is put once per operation rather than guessed. It is asked whenever the source holds a link, on every platform, including where the destination can hold none - there every option but the copy is grayed out and the dialog says why. A copy that quietly turns links into files, or files into links, is the thing being avoided.

- Windows is where this mattered most. A copy there always followed the link and left the contents behind, so a link could not be copied as a link at all. POSIX already kept symlinks by default; what is new there is being able to ask for the contents instead.

- Windows has two kinds of link where POSIX has one, and the dialog says so. A folder symlink and a junction both point at a folder, but only the symlink needs a privilege Windows normally withholds. Each row starts on the kind it found and falls back to the nearest kind that still reaches the same target, then to a plain copy. Anything the destination cannot take is grayed out rather than hidden, so the dialog does not change shape between machines.

- A link counts as one item rather than a folder to walk into. That is what POSIX always did and Windows never did, and it is what stops a copy following a link to somewhere large or unreachable.

Archives are written by libarchive, with the `7z` and `rar` commands as optional extras rather than the primary route. Linking a library needs nothing installed on the user's machine, writes the tar, zip and 7z families natively, and reports real per-file progress through the ordinary job queue. What it cannot do on the write side is why the commands are still reached for: no rar at all, and no split volumes, solid blocks, duplicate references or 7z encryption. Where an installed command can honor one of those it is used, and where nothing can the option is grayed out rather than hidden.

- Which writer gets a job follows from what was asked for, not from the format. Encryption and splitting are requirements - a backend that cannot do them is not a candidate, because quietly writing a readable archive when one was asked to be locked is the worst possible outcome. Everything else is a preference, honored where a writer can and dropped where none can, rather than failing the job.

- Where the archive goes follows the long-standing convention rather than anything invented here. One item is archived as itself and offered beside itself, so opening the archive shows the folder and the contents are one level in. Selecting a folder's whole contents instead still takes the folder's name, but is offered inside the folder with the contents at its root. A partial selection gets no suggested name at all - it is not the folder, and there is no other name a person would agree with - so the field starts empty and Compress waits until it is filled in.

- Compressing a selection separately is that convention applied per item, and deliberately one job rather than one per item: several progress bars racing for the same folder would be unreadable, and cancelling would mean cancelling each of them.

- Unpacking reaches much further than writing, so the two sides are not symmetrical. libarchive reads the tar, zip, 7z, rar, cab, lha, cpio, xar and iso families and the bare compressors, which is most of what anyone double-clicks, and it reads them entry by entry - which is what makes per-file progress, cancelling and a collision prompt possible at all. A command is reached for only when libarchive will not open the file, and only while nothing has been written yet, so handing the archive on costs nothing.

- Where an archive says an entry goes is not taken at its word. A stored path that is absolute, names a drive, or climbs out with `..` is reduced to something inside the folder the person picked. An archive must not be able to write wherever it likes on the strength of being opened.

- Following symlinked and junctioned folders is off by default, and is ours rather than the archiver's, because the tree is walked through GIO before anything reaches a writer. A link loop would otherwise pull in the whole disk, so the walk remembers directories by file id and terminates even with following switched on.

- The command lines the two programs run with are settings, not code, so a person can point one at a different build or add a switch nobody thought to offer. Every control the Compress dialog offers has a `{{MARKER}}` of its own, so an edited line keeps the dialog working; leave one out and the app says which control has gone quiet. Clearing a line puts the original back. A password is handed over as a value and never written into the line or the settings file, though it is still visible in the process list while the program runs, which is true of every archiver.

- How much of the machine a compression may use is one setting, a percentage of the cores found rather than a thread count written into each line. A percentage still means something on a machine with a different core count, and one answer covers both programs and the built-in writer. The default is 50%, because a hyperthreaded core is not a whole core and taking every logical processor slows the rest of the machine for nothing.

### Search

- Content search converts documents itself, in C, on libraries the app already links. The old helpers were a Python script, a shell script and a LibreOffice call, none of which exists on a stock Windows machine and each a dependency the install could not promise. Word, Excel and PowerPoint in both their old binary and newer zip-of-xml forms, OpenDocument and EPUB are covered. The definition-file mechanism stays, so a helper for anything else can still be dropped in.

- Results can be grouped under the folder holding them. It is a heading row per folder that actually has a match, labeled with the path under the folder searched, rather than a full tree of every folder in between - a tree puts rows on screen for folders with nothing in them, and reading that path off one row is what a person actually wants. The heading rows are built by the view rather than the model, so a folder nobody asked to open is never read, monitored or walked. Flat is still the default and switching redraws from the results in hand rather than searching again.

- On Windows the search index is used when asked, through a switch that is off by default. It answers for any folder the index covers; a folder outside it, a network location, a regular expression or a case-sensitive content match goes to the ordinary walk unchanged. Off by default because the index only knows what it has been told to watch, and a search that quietly misses a folder is worse than a slow one.

### UI

The window is a menu and toolbar, a sidebar, a path bar and a view, and the view is interchangeable.

- Three views share one interface: icon, compact and list, with an optional tree column in list view. Each reads its layout from per-folder state where the folder has any, and from the defaults where it does not.

- A window holds tabs. Each tab is a slot with its own location, history and view, and navigation, loading state and the busy cursor all belong to the slot, which is why a slow location can only block its own tab.

- The sidebar is one tree store rebuilt from bookmarks, mounts, drives and network locations. Anything that could be slow to answer, such as free space or mount state, is fetched off the main loop and folded in when it arrives.

- The list view scrolls sideways before it crushes a column, and remembers a width dragged by hand. The whole rule is under [List view column widths](#list-view-column-widths).

- The column roster earns its defaults. Ext shows by default just right of Name, without the dot, and stays blank when the tail after a dot is not really an extension. Owner shows by default on Windows too, where the platform reports a file's real owner. Permissions source - whether a file's permissions come from its folder, from the file itself, or both - is offered on Windows and off by default.

- Extensions can add context-menu items, list columns, property pages and file attributes. Nothing in the interface depends on one being present.

- Every label reads as a sentence rather than a headline. Only the first word is capitalized, and a name keeps its capital wherever it stands: the platforms, the toolkit, Trash and the other sidebar places, formats, acronyms. Mnemonics do not move and shortcut text is untouched. It is checked at lint time over every translatable string in the tree, so a label copied from upstream in Title Case is caught where it is added.

- Properties is the platform's own on Windows. Alt+Enter and Ctrl+I hand the selection to the shell property sheet, the same one Explorer shows, so a file's details read the way they do everywhere else on the machine and any tab a third-party program adds is there too. Our own window is still there under Ctrl+Enter, covering what the shell sheet has no room for - a custom icon, an emblem, an annotation, an extension page - and anything the shell cannot name falls back to it rather than doing nothing. The other platforms use our window throughout.

- A settings window opens the size of its longest page. The preferences dialog measures every page it holds and opens tall and wide enough for the largest, up to nine tenths of the screen, so no page starts out behind a scrollbar. Its floor is written for a 96dpi screen and scaled by the display's font scaling, so it means the same thing at 150% as at 100%.

- Settings that only exist on Windows sit on a page of their own, and in a `windows` group in the file rather than scattered through the others. The page carries the separator choice, the hidden-file switches, the search index and the theme controls, and it is not built into the other platforms' dialogs at all. The keys still take effect if hand-edited anywhere - it is the page that is Windows-only, not the settings.

- Hidden means two things on Windows and one thing everywhere else. Windows marks a file hidden with an attribute and treats a leading dot as an ordinary character; the rest of the world reads the dot and nothing else. So Windows gets a switch and a View menu item for each, and turning hidden files on from the menu moves the pair together, so one keystroke shows everything that was out of sight. The two can still be set apart in preferences.

- A shortcut's extension is off the listing and on in the rename box. `.lnk` and `.desktop` are both noise in a file list and both have to survive a rename, so the name on screen leaves them off while the rename box shows the whole name, and a rename that arrives without one gets it back. Without that, renaming a shortcut would turn it into an ordinary file. The Ext column still says what it is. One preference covers both, offered on every platform since `.desktop` launchers are a Linux thing.

- Scaling is the app's own job, not something done to it. The window declares itself per-monitor DPI aware, so a scaled display gets it drawn at that scale rather than drawn small and stretched, and moving it to a monitor at another scale redraws rather than restretches. The toolkit scales in whole steps, which leaves 125% or 150% short, so text is sized against the monitor's true DPI on top of that. Type comes out right at any scale; the widgets around it are still on the whole step below, which is the open item. On Linux and BSD the desktop publishes its own scaling and the toolkit follows it.

- A launch shows something at every stage. The window is put on screen at its remembered size and place as soon as it exists, before the first folder resolves, with its panes still empty. On Windows, where getting that far takes measurably longer, a small panel appears first - drawn with the platform's own toolkit, since it has to be up before GTK is - and leaves as soon as the real window has drawn.

### List view column widths

This rule has been rewritten several times and will probably move again, so the whole of it is here rather than spread between the code and a summary. This should be treated (and updated) as THE canonical, precise, complete, conflict-free definition. It describes where the behavior is going, so where the code differs it is the code that moves; the differences open today, including two places where the section is at odds with itself, are listed against the conformance item in [backlog.md](backlog.md). The arithmetic is in `nemo-column-layout.c`, which knows nothing about widgets and can be tested without a screen; the measuring that feeds it is in `nemo-list-view.c`.

- There are three "classes" of columns, for width sizing:
	- The minimum column width that overrides all minimum-width definitions below: Column header text.
	- Primary variable-width class:
		- Members: "Name", "Location".
		- Min width:
			- What will display all of the shortest N% values, as set by `list-view.column-fit-percent`, default 90.
			- Name counts every file in the folder, since every name matters.
			- Location counts each distinct value once, so a location repeated down a folder counts once rather than fifty times.
			- The share calculation: max(1, floor(count*FITPERCENT))
			- Plus ellipses for values that are too short.
			- Plus one character of air on the right.
		- Default width if room:
			- The width that shows all of the values for the column, plus one character of air on the right.
		- Max width (the width if the available horizontal space is more than all default widths combined):
			- The columns in this class expand proportionally (or either one alone if the other is not visible), so that the rightmost visible column's right edge is adjacent to the window edge.
	- Fixed-width class:
		- Columns that display values that vary in narrow bounds based on the nature of their data.
		- These columns don't resize, and can't even be manually resized.
		- Sized to fit the longest value displayed, plus one character of air on the right.
		- Examples: All date/time-related columns, octal and *nix-style permissions
	- Minor variable-width class:
		- All other columns
		- Min width:
			- Uses the same formula as "Default width if room" for [Primary variable-width class], but for the shortest 50%, plus one character of air on the right.
			- Plus ellipses for values that are too short - except for columns that are already very narrow (e.g. Ext with only 1 to 4 character extensions.)
			- Plus one character of air on the right.
		- Default width if room:
			- Default size: Same formula and % as "Default width if room" for [Primary variable-width class], plus one character of air on the right.
		- Max width:
			- The width that shows all of the values for the column, plus one character of air on the right.

- Manual resizing of columns is not persisted, unless "remember per-folder settings" is enabled.

- A width dragged by hand outranks all of the above for that column.
	- A drag is a stream of width changes, so the decision is made 350 ms after the last one rather than on each.
	- On a column with a spread it becomes a ceiling, kept in `list-view.column-max-widths` as `column:pixels`. The column still follows its contents below the ceiling, but never grows past it again however wide the window gets.
	- Name never takes a ceiling.
	- Dragging Location is what ends its claim on the surplus and pins it.

- What gets measured, and when.
	- Every row is measured as it arrives and as its details fill in, which is a handful of cells at a time rather than a walk of the folder.
	- Rows a subfolder adds count while it is open, and are forgotten when it collapses.
	- Everything is measured again when the zoom level changes the font or icon size, and when a column is switched on that was not there to be measured while it was hidden.
	- Samples are thrown away on a folder change. The names in the last folder say nothing about this one.

- A horizontal scrollbar appears, if there is not enough room because the combined min column widths exceed the displayable area.

- The columns are laid out in the view's own size allocation, for the width the tree view is about to be given, not in the tree view's own. Laying them out from the tree view's allocation draws one frame at the old widths on every step of a resize, which reads as flicker. The difference between the two allocations is learned from the previous one, so the frame where a scrollbar appears or goes is the one case still caught late.

### Appearance and themes

Two settings decide how the app looks: a light or dark mode, and the widget and icon themes to draw with. Both live under `appearance` in the settings file, both apply while the app is running, and both are offered on the Windows page of the preferences dialog. Elsewhere the desktop decides and there is nothing to ask.

- Mode is Light, Dark, or follow the system. Following means asking the platform - on Windows the `AppsUseLightTheme` personalization value, watched so the app turns with the rest of the desktop; anywhere the desktop has already told GTK, it means leaving that answer alone. An explicit Light or Dark overrides the platform everywhere.

- Themes are offered by the mode they suit. A theme states which backgrounds it was drawn for, and one that says nothing is judged by its name, which is how the convention already works: a trailing `-dark` marks the dark half of a pair, and a theme with a `-dark` sibling is the light half. Most colorful icon sets serve both, because GTK recolors the monochrome half to the foreground anyway. Choosing a theme and then changing mode swaps to its counterpart rather than leaving a dark theme on a light window.

- Targets unlikely to have GTK themes installed carry their own set: Windows and macOS. Linux and the BSDs use what the desktop provides. Each bundled icon theme is trimmed to the roughly 180 icon names a file manager actually asks for, which is what keeps one to a few hundred KB instead of tens of MB, and anything missing falls through the standard `Inherits` chain to Adwaita and then hicolor. A gap is a mismatched glyph, never a missing one.

- The four Windows icon sets are the project's own artwork. No cleanly-licensed set of any Windows generation exists, and what circulates is Microsoft's shell art extracted and repackaged, which this project will not ship - and draws blue folders besides, which Windows has never had. Every other bundled theme is an upstream open-source theme, unmodified apart from the trim, keeping its own license file and a pinned source commit.

- Themes can be dropped in on any platform by putting an ordinary GTK theme folder in `themes` or an icon theme in `icons` beside the settings file. Drop-ins are searched before the bundled set, so a same-named theme shadows it.

- The bundled set lives inside the binary rather than as files beside it. It was a couple of thousand small files, and the Windows single-file build was spending nearly all of its startup unpacking them, since the cost there is per file rather than per byte. As one compiled-in resource it costs a few MB of binary and nothing at launch. The trade is that a bundled theme cannot be edited in place, which is what the drop-in folders are for.

- The two link overlays are the app's own art rather than the theme's. A shortcut and a symlink have to read differently at a glance, and most icon themes draw the same arrow-in-a-box for a symlink that Windows draws for a shortcut. An icon added by resource path is only searched after every installed theme, so overriding one by name is not possible; both carry their own names and ship with the app. A shortcut gets the arrow, a symlink or junction a chain link.

- A shortcut to a folder wears the theme's folder icon. Everything else about a shortcut's icon comes from the shell, since only it can find a program's own artwork, but for a folder that answer is Microsoft's folder drawn among the theme's, which reads as a mistake. Whether the target is a folder comes from what the shortcut file records rather than from looking at the target, since a shortcut to a share that is not answering would otherwise stall the listing.

### Platform integration

Each window is its own process by default, and every launch is a fresh one. A crash then takes one window rather than all of them, and two versions can be open side by side, which is what trying a build next to the one in daily use needs.

- The copies still find each other. Each queues on the one bus name, so a caller from outside always reaches the oldest and the rest are read off the queue. That is how `--quit` and Close All Windows reach every copy, and how `--reset` knows one is running.

- What it costs: a tab cannot be dragged into a window belonging to another process, and dragging a tab out opens that folder in a new process and closes the tab. On Windows a new window carries the packed program's startup time rather than appearing at once. Those two are why it is a setting - turning it off puts new windows back inside one process. Launches from outside stay separate either way.

- A selection has to be sayable on a command line for another process to show it, so `--select` takes the folder around an item with the item selected. "Show in folder" from other programs goes through it.

- D-Bus needed no per-platform gating. GLib autolaunches a per-user session bus on Windows as well, shared across processes, so the two D-Bus services - the freedesktop file-manager interface and the internal file-operations one - get a real connection everywhere. The only case needing work was having no bus at all, on a headless or locked-down machine: the file-operations service, which only ever serves other processes, must simply not set itself up rather than fail.

Paths and platform behavior:

- Both `/` and `\` work in typed locations on every platform, without reserving `\`. On Windows both are already native. On POSIX `\` is a legal filename character - files created over SMB shares really do contain it - so it is not reserved and no escape syntax is introduced. Typed input is normalized by fallback instead: the literal path is tried first, and only if it does not resolve is a `\` to `/` retry attempted. Pasted Windows paths work and real backslash filenames keep working.

- Desktop settings schemas are optional at runtime. Upstream read several Cinnamon and GNOME schemas that only exist on those desktops, and a missing schema is a hard abort in GLib. The app now looks a schema up before opening it, prefers the real one wherever the session provides it, and uses its own value everywhere else. Cinnamon integration is preserved and every other environment starts clean.

- Virtual locations - network, computer, trash - are shown only where the running platform actually supports them, extending the runtime scheme check the codebase already had.

On Linux, gvfs stays an optional runtime dependency. It turned out to be desktop-agnostic rather than a Cinnamon thing, a freedesktop and GIO service present on virtually every desktop, so where it is there it provides network shares, trash, mtp and sftp, and where it is not the affected entries hide themselves. What it used to provide that is now ours everywhere is per-file metadata, which moved to the app's own store; see [Configuration and persistence](#configuration-and-persistence).

On Windows the gaps are filled natively rather than by porting gvfs:

- Deleting to the Recycle Bin, and browsing it in-app to view, restore and empty.

- Network browsing enumerates the Windows network neighborhood. UNC paths are ordinary paths and need nothing special.

- Fixed drives are first-class sidebar roots with a disk-usage bar each, replacing the single Unix filesystem root, which means nothing there. Removable, optical and network drives stay on the normal devices path, since that path carries eject and unmount.

- Per-type file icons are derived from the file's content type, because the platform's file layer reports one generic icon for nearly every file.

- "Local only" means local there too. The preferences that trade speed for detail - item counts, thumbnails - default to doing the work only for local files, and a share is native as far as the toolkit is concerned, so those defaults used to sail straight past one. A folder holding a link to a host that was not answering paid twenty to fifty seconds per link with the whole folder waiting. A share, and a link pointing at one, now count as remote.

- File associations are read from the registry and never written to it. Windows keeps the per-user default under a hash a program is not meant to set, so "Set as default" used to fail outright. The choice is kept in the settings file instead: one line per type, a command line with `%1` for the file, in the shape the registry itself uses. The map is consulted first and the registry answers for everything else, through the same query Explorer makes, so the open verb comes back rather than a print one.

- The app never starts another program itself. The single-exe build carries its whole runtime inside it, and anything it starts inherits that view of the disk along with the rest of the environment, which is not the machine the other program expects. So the desktop is asked to do the starting, for every launch rather than only the ones worked out here. The programs offered under "Open with" come from the toolkit but carry the same registry command line, so they go the same way. A store app has no command line and is left to the toolkit.

- The clipboard and outbound drags are the app's own rather than the toolkit's. The toolkit only puts its own target names into a drag, and nothing outside it reads those; the one format every Windows program does read has no name to register it under, so it cannot be added from outside. A drag now carries what Explorer's own drags carry, with the app's own formats riding alongside, so drops back into our own window behave exactly as before. One switch turns the whole thing off and puts every drag back on the toolkit's. Control copies and shift moves, following Windows, read from the keyboard directly because the toolkit reports the same suggested action either way.

- "Open in terminal" and "open elevated" map to native equivalents. On Windows that is the native console - Windows Terminal, then PowerShell, then cmd - opened at the folder, and an elevated relaunch through the ordinary UAC prompt, labeled "Open as Administrator". On Linux it is the configured terminal and a pkexec relaunch, labeled "Open as Root".

- A copy running elevated cannot be dropped on at all. Windows refuses to let an ordinary program hand anything to an elevated one and there is no way to accept it from this side. Dragging out is unaffected.

## Building

Both reference builds happen in containers rather than on a development machine, so the dependency versions are pinned and host library drift cannot quietly change the baseline.

### Linux

Stock Debian 13 is the known-good baseline, in `cicd/linux/Dockerfile.dev` (image `nemo-build-deps`, container `nemo-build`). That file is the authoritative dependency list; the packages below are the same set spelled out for anyone building on their own machine.

- Toolchain and development libraries: `meson ninja-build gcc pkg-config gobject-introspection intltool itstool python3-gi`, `libgtk-3-dev libglib2.0-dev libpango1.0-dev libatk1.0-dev libgail-3-dev`, `libjson-glib-dev libgirepository1.0-dev libgsf-1-dev libexempi-dev libexif-dev`, `libarchive-dev`, `libx11-dev libxext-dev libxrender-dev`.

- Configure and build:
	- `export SOURCE_DATE_EPOCH="$(git log -1 --format=%ct)"`, so the build is reproducible. See [Reproducible builds](#reproducible-builds).
	- `meson setup build source`
	- `ninja -C build`

- The binary is at `build/src/nemo-anywhere`. There is no second desktop-drawing binary.

- The action layout editor is a separate PyGObject script rather than part of the program, so at run time it wants `python3-gi`, `python3-gi-cairo` and `gir1.2-gtk-3.0`. Nothing else needs them, and without them only that one window is missing.

Release builds do not use this container. They are built against an older glibc, for reasons under [Release artifacts and packaging](#release-artifacts-and-packaging).

### Windows

The Windows build is native, not cross-compiled: MSYS2 with the mingw64 GTK3 toolchain, which is what both the Windows development box and the hosted release workflow use.

- `pacman -S --needed mingw-w64-x86_64-{gcc,meson,ninja,pkgconf,gtk3,json-glib,libarchive,libexif,libgsf,cppcheck,gettext} intltool git`, then `meson setup -Dxmp=false build source` and `ninja -C build`.

- Enigma Virtual Box is needed only for the single-exe artifact. Without it everything still builds, tests and stages, and only the packing step skips.

A cross-compile lane also exists, for checking a Windows build from the Linux box without Windows hardware. It is a developer convenience rather than part of the pipeline, since only Windows can pack the single exe.

- `cicd/win/fetch-sysroot.bash` resolves the dependency closure of a few root packages from the MSYS2 pacman database and unpacks each one into a sysroot. No pacman is needed, since the package database is a tarball of description files.

- `cicd/win/Dockerfile` builds the `nemo-winbuild` container: the mingw toolchain, the native GLib code generators that have to run on the build host, wine, and the baked sysroot. `cicd/win/win64.cross.txt` is the meson cross file, with wine as the exe wrapper.

Deliberately off for Windows either way: XMP and exempi, which are not packaged for mingw, and the Unix-only pieces (`gio-unix`, `x11`, SELinux, Tracker), which are guarded in meson by `host_machine.system()` and in the affected C files by `#ifdef`.

### Open questions

- How far to push a clean internal platform-abstraction boundary, against per-target `#ifdef`s in the shared files. Both conventions are in the tree today.

- Whether a fractional display scale should drive widget sizing and spacing through a stylesheet of the app's own, since the toolkit will only scale in whole steps.

## Testing

Tests are ordinary executables run by meson, and the bar for adding one is a defect that could come back.

- Each regression test is written against a specific defect and is checked by backing the fix out and watching the test fail. A test that passes either way is not evidence.

- Coverage is concentrated where the risk is: the settings parser and its bindings, the metadata store, favorites, search patterns, drag-and-drop parsing, extension objects, symlink handling, and the Windows trash and shortcut backends.

- The suite runs headless, on a virtual display where GTK needs one, and forms part of the pre-push gate along with the build, the lints and a launch smoke test. A test that cannot run on the current platform reports a skip, never a pass.

- Anything needing a real desktop - clicking a menu, driving a drag - runs in a throwaway Windows Sandbox built from the host's own image, which has its own desktop and keeps no state. A window can also be photographed without disturbing anything, since it renders off-screen even when covered.

- Interactive behavior that no assertion reaches is verified by hand against a build kept on the desktop for daily use.

## Delivery

The guiding constraint is that the git host is dumb hosting plus release storage, with as few third-party tools as possible. The whole pipeline runs locally, from `cicd/cicd.bash` on Linux and `cicd/cicd-win.ps1` on Windows.

The one deliberate exception is a release-only workflow, `.github/workflows/release-win.yml`, which builds, packs and publishes the Windows exe on a release tag. It exists because the code signing service chosen at the time would only sign artifacts from a verifiable public build. That application was refused and signing is deferred, so what the workflow earns its keep for now is being that public build, with the signing step left dormant behind a token gate.

### Branches and the merge gate

- Feature branches merge `--no-ff` into `dev`, the integration target. `main` is release-only, and merging dev into main is what cuts a release. Nothing is committed directly on either.

- The merge gate is `cicd.bash --gate` running as the `pre-push` hook, for pushes to main or dev only. It is the local stand-in for a hosted CI workflow: lints, then a container build, then the test suite, then a headless launch smoke test. Install it per clone with `cicd/hooks/install.bash`; override a run with `git push --no-verify` or `SKIP_GATE=1`.

- The same hook blocks a push to main unless `source/meson.build` is a strict version increase over what is already there.

### Versions and build numbers

- `source/meson.build` is the only place the version is written. Everything else reads it.

- The fork numbers its own releases from 1.0.0, independent of the 6.6.4 code baseline. Since 6.6.4 was never tagged or released here, that reset was a clean one-time step.

- Every build also carries a build number: minutes elapsed since the start of 2000, in lower-cased Crockford base32, which drops i, l, o and u so nothing reads as a digit by mistake. Five characters today, six from 2033. It sits beside the version in `--version`, `--about`, Help > About, the Windows splash screen and the release notes, so a bug report names not just which release but which build of it.
	- It is worked out at configure time from `SOURCE_DATE_EPOCH`, so two builds of one commit carry the same number, falling back to the clock when nothing sets it.
	- It lives in a generated header of its own rather than in `config.h`, because the number moves on every reconfigure and a change in `config.h` rebuilds the whole tree.

### The pipeline

Stages, in order, each self-skipping when unconfigured: remote sync, format, debug build, tests and lints, profiler, release build, packages, dogfood, backup and publish. Disabled on purpose today are the format stage, since there is no in-place C formatter worth running, and the engine's own release collector, because the per-platform release lanes write those artifacts themselves.

- Remote sync runs first for a reason. The publish stage pulls at the end, so without it a change merged remotely mid-run would be pushed having never been built or tested. It fast-forwards when the branch is only behind and stops the run outright when it has diverged. It is skipped in gate mode, since a pre-push hook must not rewrite the tree underneath the push that called it.

- No stage is allowed all the cores. Build parallelism is capped at half of them, so a full run leaves the machine usable.

- The publish stage refuses a dirty tree, checked once at preflight and again before it runs. It commits everything it finds, and nothing there can tell work in progress from a finished change.

- Profiling browses a generated folder tree on a private headless display while sampling every thread, then renders a flamegraph and prints the hot spots into the run log. It samples by attaching a debugger rather than using perf, because perf needs a privileged sysctl here and a profiler that cannot run without root is a profiler nobody runs. The cost is wall-clock samples, so a blocked thread reads as work; the report keeps waiting in its own bucket and gives every figure as a share of busy time as well as of total. It profiles the debug build, since the release binaries are stripped and a flamegraph with no function names says nothing.

- The last stage archives the project tree into a rotated set of backups and then commits and pushes the current branch. The archive keeps what would be painful to lose - source, docs, the pipeline, assets, release builds and their packages - and drops what a command regenerates, chiefly the staged Windows runtime snapshot, which by itself took each archive from about 1.6 MB to 36 MB.

### Reproducible builds

Nothing a build produces takes its timestamp from the clock. Every lane sets `SOURCE_DATE_EPOCH` to the commit date of what is being built, so the same commit builds to the same bytes on any box on any day and a released artifact can be checked against a rebuild of its tag.

- The Windows exe was the one that actually differed run to run. The linker writes a timestamp into the PE header, and left alone it writes the clock: two clean builds of one commit used to differ in exactly those four bytes.

- The linker, `dpkg-deb` and `rpmbuild` read the stamp themselves. `zip` has no such notion, so its input is stamped on disk and fed in sorted order, and `tar` is given the stamp and a sorted order explicitly.

- One script answers what the stamp is, and every lane calls it rather than working it out again. `docker exec` does not carry the host environment into a container, so each lane hands it over explicitly.

- A tree with uncommitted changes still gets its `HEAD` commit's date, since the alternative is the clock, but the release lanes warn, because nothing built from it can be reproduced.

- Left out on purpose: the wall clock still names log files and dated dogfood copies, which is what it is for. A signed exe can never be byte-identical anyway, since the countersignature carries the real time of signing.

### Release artifacts and packaging

The two platforms get deliberately different artifacts, because what a user already has installed is different.

Linux is a thin relocatable prefix of a couple of MB that uses the distro's own GTK3. A bundled GTK on Linux is the thing that goes stale and mismatches the desktop's theme, portals and input methods, and it would multiply the download for no gain.

- It is built in an Ubuntu 22.04 container, never the day-to-day Debian 13 one. A binary's glibc floor is whatever it was built against, so a release built on Debian 13 would refuse to start on anything older than 2025. The floor is therefore glibc 2.35 and GTK 3.24.33, which reaches Ubuntu 22.04, Debian 12, Mint 21 and Fedora 36 onward.

- What makes it relocatable: the program works out where it is and points `XDG_DATA_DIRS` and `PATH` at the folder it sits in, at startup, before anything reads them. The extension library is found through an `$ORIGIN` rpath. Everything looked up through the XDG data dirs - actions, search helpers, icons, mime info - then resolves wherever the folder was installed. There used to be a shell wrapper in `bin/` doing that with the real binary hidden in `libexec/`; two files where one would do, so it went.

- The D-Bus activation file is written at startup rather than installed, into the user's own service directory. It has to name an absolute path, and a portable copy does not have one until it runs.

- Staging leaves out what only a system install would read: mime data, polkit, man pages and the editor syntax files. Both packages install the prefix under `/opt`, where none of it is read, and the install rules still produce all of it, so a distro building `--prefix=/usr` is unaffected. Icons are compiled in except the app icon at its eight sizes, which packaging and the menu entry need as real files. Actions, search helpers and the settings schema stay as files, since those are the drop-in folders a user edits and Preferences has buttons that open them.

Windows is one self-contained `nemo-anywhere.exe` with the whole runtime packed inside it by Enigma Virtual Box, as an in-memory virtual filesystem with nothing extracted at run time. No library folder, no launcher, nothing installed or registered: an exe to copy anywhere.

- The pack source is the same flat layout the zip uses - exe and dlls at the root, `lib/`, `share/` and `etc/` beside them - and GLib-stack libraries resolve their data relative to their own dll, so that tree also runs unpacked with a bare double-click.

- Packed exes are occasionally false-flagged by antivirus, so the plain zip stays available as the fallback artifact. It is also the fallback for the release being unsigned.

Packaging builds from what the release lanes already produced and never rebuilds. The Linux tarball becomes a `.deb` and an `.rpm`, both installing the same relocatable prefix under `/opt` plus a launcher, a menu entry and icons in the shared theme. The `.deb`'s dependency versions are read off the built binaries inside the release container rather than on a development box, so the package claims the floor the binary was actually built against; `rpmbuild` derives its own from the ELF. BSD, macOS, AppImage and Flatpak wait on a toolchain.

Cutting a release tags `v<version>` from a clean main and uploads the artifacts. Release notes are the hand-written changelog section for that version, never a generated commit list, falling back to generated notes only so a release is never published blank. A version carrying a pre-release part is published as a prerelease, which matters to the installers: their stable channel resolves to the latest non-prerelease, so a beta is only reachable with `--release dev`.

### Installing

`install.bash` and `install.ps1` sit at the repo root and run as one-liners straight from a shell. They are two standalone installers rather than one script with a helper: the bash one targets bash 3.2 so stock macOS runs it, the PowerShell one covers unix itself instead of handing off. The duplication is deliberate, and buys a one-liner that works from whichever shell someone already has open.

- The app installs as a whole folder plus the two things that make it reachable: a menu entry and a name on PATH. A file manager gets launched both ways.

- A user install is the default and needs no privileges. A system-wide install is opt-in and is the only path that escalates, which it states in the plan first.

- Every run prints what it is about to do and waits for a yes. Downloads are checksum-verified before anything is unpacked, so a bad download can never replace a working install. Reinstalling replaces in place, and `--uninstall` removes exactly what was added.

- Because they read the releases page, the packaging stage has to produce exactly these names: `nemo-anywhere-<version>-<os>-<arch>.tar.gz` for unix and `.zip` for Windows, with `<os>` one of `linux` or `windows` and `<arch>` one of `x86_64` or `arm64`, plus `nemo-anywhere-<version>-sha256sums.txt` beside them in `sha256sum` format. Each archive holds one top-level folder, whose entry point is `bin/nemo-anywhere` on unix and `nemo-anywhere.exe` at the root on Windows.

### Dogfooding

Each platform's pipeline publishes one build to a shared drop folder for that platform and writes nothing else there: the whole relocatable prefix on Linux, the packed exe on Windows.

- The launcher owns the local side. It copies the drop into a pool of date-stamped versions and points a symlink at the newest, so the fixed name is the only thing anything else has to know. One script serves all three platforms, with a small wrapper per platform in the place that platform looks for commands, so it can be typed at a shell or named in a `.desktop` file.

- The pool is rotated on every launch: the newest of each finished hour, day, week, month and year, the most recent few, and the first build ever held. On top of that a budget of at most ten versions, at least five, and only as many between the two as fit in 1 GB. Nothing a running process lives inside is removed, so going back to an older build is a matter of running it rather than rebuilding it.

- A build already held is settled on its bytes, not its date. The sync layer restamps what it carries, so a date test on its own re-fetched the same build every run.
