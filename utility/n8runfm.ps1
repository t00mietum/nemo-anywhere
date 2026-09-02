#!/usr/bin/env pwsh

##	Purpose:
##		- Cross-platform (PowerShell 7) dogfood launcher for Nemo Anywhere, same
##		  concept as silkterm's n8runterm: keep a small pool of date-stamped
##		  release copies in a local target dir and launch the newest, passing
##		  through any arguments. Independent of the cicd pipeline.
##		- A copy is stamped 'nemofmdf_<YYYYMMDD-HHMMSS>_<tag>' (+ '.exe' on Windows),
##		  where the stamp is the source build's mtime - a given build is copied once,
##		  and a running copy never blocks the copy. On Windows the copy is a single
##		  self-contained exe (a file); on Linux it's the whole prefix dir (a tree).
##		- Each run, in order: delete idle copies over 7 days old (in use = a running
##		  process that IS the copy exe, or on Linux whose image lives inside the copy
##		  dir); probe every source and refresh from whichever holds the newest build;
##		  launch the newest copy. Each step says what it found and what it decided.
##		- Every source is probed, not just the first that answers - the newest build
##		  wins wherever it happens to be sitting. In listing order:
##			local build  this box's own repo build under cicd/artifacts, if the clone is here
##			b23          the Linux box across the network (a UNC path from Windows; its
##			             own local path when the launcher is running on b23 itself)
##			dogfood      the synced by-self drop, whatever the sync layer last brought in
##		  A source on a network share only gets a moment to answer - an unreachable
##		  one must not hold up a launch a held copy can serve. Two sources holding
##		  the same build are reported once, not counted twice.
##		- The tag in the copy name is the platform the build is for:
##			lin  the dogfood prefix nemo-anywhere.app (Linux)
##			win  the packed single exe from the native build (Windows)
##		- On Linux the launcher wires the runtime env itself (loader path, schemas,
##		  data dirs) at the stamped copy; on Windows the packed exe carries its whole
##		  runtime, so nothing is wired. Either way it starts the app detached and
##		  exits - on unix the app's own output goes to a log in the target dir, since
##		  it no longer has the caller's console.
##		- Opens at a configured startup location so a plain launch lands somewhere
##		  useful. A path or URI given on the command line wins over it, and if the
##		  configured place isn't there the app opens wherever it would have anyway.
##		- If no copy is held and the source is unreachable, falls back to the
##		  first installed known file manager.
##		- On Windows, runs the WHOLE launcher elevated - it self-elevates via a UAC
##		  prompt, so the copy, the log and the launched app all get admin rights. A
##		  shortcut click behaves like running from an elevated shell instead of
##		  silently launching a stale build, and the app gets
##		  SeCreateSymbolicLinkPrivilege, which a filtered token drops. '--no-admin'
##		  opts out. Not offered on unix, where a file manager running as root is a
##		  footgun.
##		- Reports a failure, a rejected argument or a skipped copy in a dialog when
##		  launched from a shortcut (or with '--gui'), since a click's console just
##		  flashes shut. '--admin'/'--no-admin'/'--gui' are consumed here; the rest is
##		  checked against the app's own options and then forwarded.
##	History: At bottom of script.


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Configuration

## Where builds come from. Every entry is probed each run and the newest build
## wins - a source is not a fallback for the one above it. Each entry lists one or
## more roots because a clone or a sync root sits in a different place per box;
## every root is probed and the newest build among them is that source's build.
## Nothing here has to exist.
##
## The 'main binary' relative path below turns a root into the thing actually
## looked at: the reachability probe, the build stamp (its mtime) and the size.
## On Windows the root IS the packed exe, so there is no sub-path.

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT
if ($IsWindows) {
	## Single self-contained exe (extension + whole GTK runtime packed in), so a
	## copy is one file, not a prefix tree - the same shape as silkterm's n8runterm.
	$Sources = @(
		@{
			## This box's own build, straight out of the repo - what cicd-win.ps1
			## packs. C:\opt\0-0 is a junction to C:\0-0 here, but another box may
			## have only one of them, so both are listed and the duplicate is
			## reported once rather than counted as a second build.
			Label = "local build"
			Roots = @(
				"C:\opt\0-0\users\collierjr\data\prs\dev\github.com\t00mietum\nemo-anywhere\github\cicd\artifacts\win-portable\nemo-anywhere.exe"
				"C:\0-0\users\collierjr\data\prs\dev\github.com\t00mietum\nemo-anywhere\github\cicd\artifacts\win-portable\nemo-anywhere.exe"
			)
		}
		@{
			## The Linux box's own drop, read straight off its share. Same file the
			## sync layer eventually brings here, but reachable now rather than
			## whenever the sync client next gets round to it - which is the whole
			## point of probing it separately from the local copy below.
			Label = "b23"
			Roots = @(
				"\\b23\home-collierjr\synced\0-0\common\exec\util\mswin\gui\by-self\win64\nemo-anywhere.exe"
			)
		}
		@{
			## The synced by-self drop on this box, wherever it came from.
			Label = "dogfood"
			Roots = @(
				"C:\opt\0-0\common\exec\synced\util\mswin\gui\by-self\win64\nemo-anywhere.exe"
			)
		}
	)
	$SourceMainBin = ""            # the source IS the exe (single file, no sub-path)
	$SourceTag     = "win"
	$CopyIsFile    = $true         # a held copy is one .exe file, not a dir tree
	$CopyExt       = ".exe"
	## The pool sits in the LOCAL (not synced) by-self folder, beside n8runterm's -
	## stamped copies are per-box churn and must not ride the sync. Created if absent.
	$TargetDir     = "C:\opt\0-0\common\exec\local\util\mswin\gui\by-self\win64"
} else {
	$Sources = @(
		@{
			## A host-side prefix staged out of the repo by cicd/linux/stage-prefix.bash.
			## The Linux dogfood stage is still disabled, so this is normally absent
			## and warn-skips; the slot is here so it just works once it is enabled.
			Label = "local build"
			Roots = @(
				"/opt/0-0/users/collierjr/data/prs/dev/github.com/t00mietum/nemo-anywhere/github/cicd/artifacts/dogfood/nemo-anywhere"
				(Join-Path $HOME "data/prs/dev/github.com/t00mietum/nemo-anywhere/github/cicd/artifacts/dogfood/nemo-anywhere")
			)
		}
		@{
			## b23 IS the Linux box, so from here its drop is a local path; the /mnt
			## spelling covers running from some other unix box with the share mounted.
			Label = "b23"
			Roots = @(
				(Join-Path $HOME "synced/0-0/common/exec/util/linux/nemo-anywhere.app")
				"/mnt/b23/home-collierjr/synced/0-0/common/exec/util/linux/nemo-anywhere.app"
			)
		}
		@{
			Label = "dogfood"
			Roots = @(
				(Join-Path $HOME ".synced/Dropbox/0-0/common/exec/util/linux/nemo-anywhere.app")
			)
		}
	)
	$SourceMainBin = "bin/nemo-anywhere"
	$SourceTag     = "lin"
	$CopyIsFile    = $false        # a held copy is the whole prefix dir
	$CopyExt       = ""
	$TargetDir     = Join-Path $HOME ".local/share/nemo-anywhere-dogfood"
}

## How long a source on a network share gets to answer the probe. An unreachable
## share otherwise wedges the query for the SMB stack's own timeout, tens of
## seconds of nothing while a held copy sits ready to launch.
$ProbeTimeoutMs = 1500

## What the probe asks of a path: is it there, when was it built, how big is it.
## Held as text because the network case runs it in its own runspace, and the two
## cases must not be allowed to drift apart.
$ProbeScript = @'
param($p)
$item = Get-Item -LiteralPath $p -ErrorAction SilentlyContinue
if (-not $item) { return $null }
$len = 0
if ($item -is [System.IO.FileInfo]) { $len = $item.Length }
[pscustomobject]@{ Stamp = $item.LastWriteTime; Length = [int64]$len }
'@

## Filled in by fProbeBuild when a probe comes back empty for a reason worth
## saying out loud (it timed out rather than simply not being there).
$ProbeNote = ""

## How far apart two same-sized builds' stamps may be and still be the same build
## (see fSameBuild). Wide enough to absorb the sync layer's rounding, far too
## narrow to swallow a real rebuild.
$SameBuildSlackSec = 2


## Get-ChildItem item-type for the pool: files on Windows (single exe), dirs on Linux.
$CopyGciType = if ($CopyIsFile) { @{ File = $true } } else { @{ Directory = $true } }

## Prefix for the date-stamped copy dirs.
$DogfoodPrefix = "nemofmdf"

## Delete idle stamped copies older than this many days.
$MaxAgeDays = 7

## Launch elevated (as administrator). On by default on Windows; the '--no-admin'
## arg (consumed at the entry point below, never forwarded) turns it off. RunAs pops
## a UAC consent unless the calling session is already elevated. Windows only - a
## file manager running as root on unix is a footgun, not a feature. Set from the
## flag at the entry point, so this initial value is not the default.
$RunAsAdmin = $false

## Options the app itself accepts, so a typo is refused here instead of forwarded.
## The packed Windows exe is GUI-subsystem: its "Could not parse arguments" goes to
## a stderr nobody is attached to, and it takes over ten seconds to get that far, so
## an unknown flag reads as the launcher doing nothing at all. Anything not starting
## with '-' is a path or URI and passes untouched.
##
## First list is nemo's own (source/src/nemo-main-application.c, the GOptionEntry
## table); second is the GTK option group it adds. Keep them in step with that table.
$KnownAppOptions = @(
	"--check", "-c", "--browser", "--version", "--geometry", "-g",
	"--no-default-window", "-n", "--no-desktop", "--tabs", "-t",
	"--existing-window", "--fix-cache", "--debug", "--quit", "-q",
	"--help", "--help-all", "-h", "-?",
	"--display", "--screen", "--class", "--name", "--sync", "--gtk-module",
	"--g-fatal-warnings", "--gdk-debug", "--gdk-no-debug", "--gtk-debug",
	"--gtk-no-debug", "--help-gtk", "--help-gtk-1", "--help-gdk"
)

## Fallback file managers, tried in order when no copy is held and the source is
## unreachable. Launched plainly (generic managers accept a path arg at most).
$FallbackManagers = if ($IsWindows) {
	@("explorer.exe")
} else {
	@("nemo", "nautilus", "pcmanfm", "thunar", "dolphin")
}

## Where the app opens when the caller didn't name a location. First candidate that
## exists wins - C:\opt\0-0 is a junction to C:\0-0 here, but another box may have
## only one of the two. Nothing here has to exist; if none does, the app just opens
## wherever it would have on its own.
$StartupLocations = if ($IsWindows) {
	@(
		"C:\opt\0-0\users\$env:USERNAME\0_links"
		"C:\0-0\users\$env:USERNAME\0_links"
	)
} else {
	@(
		(Join-Path $HOME "0-0/0_links")
	)
}

## Options whose value arrives as its own token. That token is a value, not a
## location, even though it doesn't lead with '-'.
$ValueAppOptions = @(
	"--geometry", "-g", "--display", "--screen", "--class", "--name",
	"--gtk-module", "--gdk-debug", "--gdk-no-debug", "--gtk-debug", "--gtk-no-debug"
)

## Per-run decision log, kept in the target dir, so a closed console can't lose
## the copy/skip reasons behind a launch.
$RunLog = Join-Path $TargetDir "n8runfm.log"

## Unix only: where the detached app's own output goes (GTK/GLib gripes and any
## crash message), since it no longer has the caller's console. Appended to and
## trimmed like the run log.
$AppLog = Join-Path $TargetDir "n8runfm-app.log"

## Stamp format shared by the copy name and every date comparison below.
$StampFormat = "yyyyMMdd-HHmmss"

## Same instant, spelled for a person - the report says when a build was made, the
## copy name carries $StampFormat.
$StampDisplay = "yyyy-MM-dd HH:mm:ss"

## Running-process image paths, filled in on first use (see fRunningExePaths).
$RunningPaths = $null


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Functions

## Entry point.
function fMain {
	param([string[]]$PassArgs)

	if (-not (Test-Path -LiteralPath $TargetDir)) {
		New-Item -ItemType Directory -Path $TargetDir -Force | Out-Null
	}

	fTrimLog $RunLog
	if (-not $IsWindows) { fTrimLog $AppLog }
	## Log the args too: a launch that dies on one of them leaves no other trace.
	fLog ("=== run: PS {0}, script {1}, admin {2}, args [{3}] ===" -f `
		$PSVersionTable.PSVersion, $PSCommandPath, $RunAsAdmin, ($PassArgs -join " "))

	fBanner "n8runfm - Nemo Anywhere dogfood launcher"

	## 1. Sweep stale partial copies, stale idle copies, anything left by the old
	##    layout, and (Windows) a synced-on mark-of-the-web that would get a later
	##    click policy-blocked.
	fStep "Housekeeping"
	if ($IsWindows) { fSelfHealMotw }
	fDeleteStaleTmp
	fDeleteOldBuilds
	fRetireLegacyCopies
	if (-not $script:StepRows) { fItem "-" "" "nothing to clean up" }

	## 2. Ask every source what it is holding.
	fStep "Sources"
	$sources = fProbeSources

	## 3. Take a copy from whichever source has the newest build.
	fStep "Build in hand"
	fCopyIfNewer -Rows $sources

	## 4. Launch the newest copy. The Process goes nowhere - it's there for a
	##    test harness, and letting it reach the output stream would dump a
	##    process table on the way out.
	fStep "Launch"
	$PassArgs = fAddStartupLocation -PassArgs $PassArgs
	$copy = fNewestCopy
	if ($copy) {
		$null = fLaunchNemo -CopyPath $copy.File.FullName -PassArgs $PassArgs
		return
	}

	## 5. Nothing held and no source reachable - fall back to any file manager.
	fWarn -Gui "no dogfood copy held and no source reachable; trying fallbacks"
	$null = fLaunchFallback -PassArgs $PassArgs
}




## Refuse an option the app will reject, rather than forwarding it into a silent
## death (see $KnownAppOptions). Only '-'-leading tokens are checked - anything else
## is a path or URI. '--opt=value' is checked on the name; a value that follows as
## its own token doesn't lead with '-', so it passes as a path would. '--' ends the
## options, and a single-dash run of known short flags ('-tn') is accepted bundled.
function fCheckPassArgs {
	param([string[]]$PassArgs)

	if (-not $PassArgs) { return }
	$shorts = ($KnownAppOptions | Where-Object { $_ -match '^-[^-]$' } |
		ForEach-Object { $_.Substring(1) }) -join ""

	foreach ($arg in $PassArgs) {
		if ($arg -eq "--") { return }
		if ($arg -notmatch '^-') { continue }

		$name = ($arg -split "=", 2)[0]
		if ($KnownAppOptions -contains $name) { continue }
		## .Contains, not -like: '?' and '*' are real short flags, and -like would
		## read them as wildcards and wave anything through.
		if ($name -match '^-[^-]+$' -and
			-not ($name.Substring(1).ToCharArray() | Where-Object { -not $shorts.Contains($_) })) { continue }

		fFail ("the app doesn't accept '$name'" +
			$(if ($name -ieq "-admin") { " - did you mean '--admin' (elevate)?" } else { "" }) +
			"`n`nLauncher flags: --admin, --no-admin, --gui" +
			"`nApp options:    " + (($KnownAppOptions | Where-Object { $_ -match '^--' }) -join " "))
	}
}


## Append the configured startup location, so a plain launch opens somewhere useful
## instead of the app's own default. A location named on the command line wins.
function fAddStartupLocation {
	param([string[]]$PassArgs)

	$out = @()
	if ($PassArgs) { $out += $PassArgs }
	if (fHasLocationArg -PassArgs $PassArgs) { return $out }

	$loc = $StartupLocations | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
	if (-not $loc) {
		if ($StartupLocations) {
			fItem "skip" "location" ("none of these are there: " + ($StartupLocations -join ", "))
		}
		return $out
	}

	fItem "-" "location" $loc
	return $out + $loc
}


## True when the caller already named where to open. Anything after '--' is a path
## or URI; before it, so is any token that doesn't lead with '-', unless it is the
## value of the option in front of it (see $ValueAppOptions).
function fHasLocationArg {
	param([string[]]$PassArgs)

	if (-not $PassArgs) { return $false }

	$skipNext  = $false
	$afterDash = $false
	foreach ($arg in $PassArgs) {
		if ($afterDash) { return $true }
		if ($arg -eq "--") { $afterDash = $true; continue }
		if ($skipNext) { $skipNext = $false; continue }
		if ($arg -match '^-') {
			## '--opt=value' carries its own value; '--opt value' eats the next token.
			if ($arg -notmatch '=' -and $ValueAppOptions -contains $arg) { $skipNext = $true }
			continue
		}
		return $true
	}
	return $false
}


## What to look at to decide a source root is really there: its main binary inside
## the prefix on Linux, or the root itself on Windows, where the source IS the exe.
function fSourceProbePath {
	param([Parameter(Mandatory)][string]$Root)
	if ($SourceMainBin) { return (Join-Path $Root $SourceMainBin) }
	return $Root
}


## Ask every configured source what build it is holding, report each one as it
## answers, and return a row per source:
##   { Label, Root, Probe, Stamp(DateTime), Length, Reachable, Duplicate }
##
## Roots are usually alternate spellings of the same place (a clone root that differs
## per box, a junction, a share mounted somewhere else), but not always - on some box
## two of them really are separate trees holding different builds. So every root is
## probed and the newest build among them is that source's build; an exact tie keeps
## the first, which is the local spelling of a junction pair.
##
## Two sources can legitimately hold the SAME build - the sync layer copies the
## local drop to the other box and back. Sameness is decided on the build itself
## (stamp plus size), not on the path, because a junction, a symlink and a UNC
## spelling of one file all look like different paths and none of them is a second
## build. The later one is flagged so the report says so once and the copy step
## ignores it.
function fProbeSources {
	$rows = @()

	foreach ($src in $Sources) {
		$row = [pscustomobject]@{
			Label     = $src.Label
			Root      = $src.Roots[0]
			Probe     = fSourceProbePath $src.Roots[0]
			Stamp     = $null
			Length    = [int64]0
			Reachable = $false
			Duplicate = $false
		}

		$why = "not there"
		foreach ($root in $src.Roots) {
			$probe = fSourceProbePath $root
			$build = fProbeBuild $probe
			if (-not $build) {
				if (-not $row.Reachable) { $row.Probe = $probe }
				if ($script:ProbeNote) { $why = $script:ProbeNote }
				continue
			}
			## Truncate to the granularity the copy name is written at, right here,
			## so the report, the sameness test, the sort and the name a copy ends
			## up with can't disagree. They already did: a build handed over by the
			## sync layer keeps whole seconds while the one it was copied from keeps
			## ticks, which made one build look like two.
			$stamp = fParseStamp $build.Stamp.ToString($StampFormat)
			if ($row.Reachable -and $stamp -le $row.Stamp) { continue }
			$row.Root      = $root
			$row.Probe     = $probe
			$row.Stamp     = $stamp
			$row.Length    = $build.Length
			$row.Reachable = $true
		}

		if (-not $row.Reachable) {
			fItem "skip" $row.Label "${why}: $($row.Probe)"
		} else {
			$same = fSameBuild -Row $row -Against $rows
			if ($same) {
				$row.Duplicate = $true
				fItem "-" $row.Label "same build as $($same.Label)"
			} else {
				fItem "ok" $row.Label ("{0}   {1}" -f $row.Stamp.ToString($StampDisplay), $row.Probe)
			}
		}

		$rows += $row
	}

	return $rows
}


## The first earlier source found to be holding the same build as this one, or
## $null. Same build = identical size, and stamps no further apart than
## $SameBuildSlackSec.
##
## Identical size is the strong half of the test. The slack is there because a
## build's mtime does not survive the sync layer intact - it comes back rounded to
## whole seconds, and nothing promises it rounds DOWN. Without slack, a build that
## rounded up reads as one second newer than the very copy it was made from, and
## the launcher drags tens of megabytes across the network to fetch a build already
## sitting on this disk. Two genuinely different builds of byte-identical size
## seconds apart is not a thing that happens.
##
## Marking a later source a duplicate never costs us the build: the earlier source
## holding it is by definition reachable, and every source stands on its own the
## moment what it holds actually differs.
function fSameBuild {
	param(
		[Parameter(Mandatory)]$Row,
		$Against
	)
	if (-not $Against) { return $null }
	foreach ($other in $Against) {
		if (-not $other.Reachable -or $other.Duplicate) { continue }
		if ($other.Length -ne $Row.Length) { continue }
		if ([math]::Abs(($other.Stamp - $Row.Stamp).TotalSeconds) -le $SameBuildSlackSec) { return $other }
	}
	return $null
}


## What a source is holding: build stamp (mtime) and size, or $null when the path
## isn't there. A local path answers from the filesystem straight away, so it is
## asked directly. A path on a network share gets a short leash instead: a share
## that is down wedges the query until the SMB stack gives up on its own schedule,
## and a launcher with a perfectly good local copy in hand has no business making
## the user wait that long. Past the deadline the probe is abandoned - its thread
## unwinds whenever SMB is done with it - and the source is treated as absent.
function fProbeBuild {
	param([Parameter(Mandatory)][string]$Path)

	## Why the last probe came back empty, for the caller's one-line report. Set
	## here rather than printed here, so a source with several roots still gets a
	## single row instead of one per root.
	$script:ProbeNote = ""

	if (-not (fIsRemotePath $Path)) {
		return (& ([scriptblock]::Create($ProbeScript)) $Path)
	}

	$probe = [powershell]::Create()
	$null  = $probe.AddScript($ProbeScript).AddArgument($Path)
	$async = $probe.BeginInvoke()

	if ($async.AsyncWaitHandle.WaitOne($ProbeTimeoutMs)) {
		$build = $null
		try { $build = $probe.EndInvoke($async) | Select-Object -First 1 } catch { }
		$probe.Dispose()
		return $build
	}

	## Dispose would block on the wedged probe, so hand it off and walk away.
	$null = $probe.BeginStop($null, $null)
	$script:ProbeNote = "gave up after ${ProbeTimeoutMs}ms"
	return $null
}




## True for a path that may be served over the network - a UNC name, a drive letter
## mapped to a share, or a unix path under a mount dir. The last is a guess, but a
## dead mount wedges exactly the way a down share does and the probe timeout costs
## a local path nothing. Everything else is local and needs no protection.
function fIsRemotePath {
	param([Parameter(Mandatory)][string]$Path)

	if ($Path -match '^(\\\\|//)') { return $true }
	if (-not $IsWindows -and $Path -match '^/(mnt|media|net)/') { return $true }
	if ($IsWindows -and $Path -match '^([A-Za-z]):') {
		try {
			$drive = [System.IO.DriveInfo]::new($Matches[1] + ":\")
			return ($drive.DriveType -eq [System.IO.DriveType]::Network)
		} catch { return $false }
	}
	return $false
}


## Copy in the newest source build as '<prefix>_<stamp>_<tag>' when it beats what
## we already hold. Copies to a .tmp name then renames, so an interrupted copy can
## never pass for a complete one. No-op when nothing is reachable, or when the copy
## in hand already IS the newest build anyone is offering.
function fCopyIfNewer {
	param($Rows)

	$held = fNewestCopy
	if ($held) { fItem "-" "held" ("{0}   {1}" -f $held.Stamp.ToString($StampDisplay), $held.Name) }
	else       { fItem "-" "held" "nothing held yet" }

	## Duplicates are the same build reached by another name, so they can't win.
	## -Stable so an exact tie falls to listing order, which puts the local copy of
	## a build ahead of the one across the network.
	$best = @($Rows | Where-Object { $_.Reachable -and -not $_.Duplicate } |
		Sort-Object Stamp -Descending -Stable)
	$best = if ($best.Count) { $best[0] } else { $null }

	if (-not $best) {
		if ($held) { fItem "skip" "source" "none reachable - running the copy in hand" }
		else       { fWarn -Gui "no source reachable and no copy held" }
		return
	}

	## Round-trip through the stamp text the copy is named with, so the comparison
	## can't disagree with the name over sub-second precision.
	$stamp     = $best.Stamp.ToString($StampFormat)
	$stampTime = fParseStamp $stamp

	if ($held -and $held.Stamp -ge $stampTime) {
		fItem "ok" "newest" "$($best.Label) - already held, nothing to copy"
		return
	}
	fItem "-" "newest" ("{0}   {1}" -f $best.Stamp.ToString($StampDisplay), $best.Label)

	$dst = Join-Path $TargetDir "${DogfoodPrefix}_${stamp}_${SourceTag}${CopyExt}"
	if (Test-Path -LiteralPath $dst) {
		fItem "ok" "copy" "already present: $(Split-Path $dst -Leaf)"
		return
	}

	$tmp   = "$dst.tmp"
	$clock = [System.Diagnostics.Stopwatch]::StartNew()
	fItem "-" "copy" "$($best.Root) -> $(Split-Path $dst -Leaf)"
	try {
		if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
		if ($CopyIsFile) { Copy-Item -LiteralPath $best.Root -Destination $tmp -Force -ErrorAction Stop }
		else             { Copy-Item -LiteralPath $best.Root -Destination $tmp -Recurse -Force -ErrorAction Stop }
		Rename-Item -LiteralPath $tmp -NewName (Split-Path $dst -Leaf) -ErrorAction Stop
		## A synced-sourced exe can carry a mark-of-the-web; clear it so the launch
		## isn't SmartScreen-blocked. Best-effort, Windows-only (no-op for a dir).
		if ($CopyIsFile) { try { Unblock-File -LiteralPath $dst -ErrorAction SilentlyContinue } catch { } }
		$clock.Stop()
		$size = if ($CopyIsFile) { " " + (fHumanSize (Get-Item -LiteralPath $dst).Length) } else { "" }
		fItem "ok" "copy" ("done{0} in {1:n2}s" -f $size, $clock.Elapsed.TotalSeconds)
	} catch {
		fWarn -Gui "couldn't copy the build from $($best.Label) ($($_.Exception.Message))"
		if (Test-Path -LiteralPath $tmp) { try { Remove-Item -LiteralPath $tmp -Recurse -Force } catch { } }
	}
}




## Delete stamped copies whose build is older than $MaxAgeDays, skipping any
## with a running process inside (a delete that throws is also treated as in
## use). Only ever touches dirs matching THIS launcher's own name spec - never
## a foreign entry that merely shares the dir.
##
## The newest copy is exempt whatever its age: it is the one about to launch, and
## a source that has itself gone quiet for longer than the cutoff would otherwise
## have us delete and re-copy the very same build on every single run.
function fDeleteOldBuilds {
	## Any tag ages out here (incl. one-off hand-dropped tags).
	$rx      = "^$([regex]::Escape($DogfoodPrefix))_\d{8}-\d{6}(_[a-z0-9]+)?$([regex]::Escape($CopyExt))$"
	$cutoff  = (Get-Date).AddDays(-$MaxAgeDays)
	$running = @(fRunningExePaths)
	$newest  = fNewestCopy
	$keep    = if ($newest) { $newest.File.FullName } else { "" }
	$deleted = 0

	Get-ChildItem -LiteralPath $TargetDir @CopyGciType -Filter "${DogfoodPrefix}_*" -ErrorAction SilentlyContinue |
		Where-Object { $_.Name -match $rx } |
		Where-Object { $_.FullName -ne $keep } |
		Where-Object { (fBuildTime $_) -lt $cutoff } |
		ForEach-Object {
			if (fRemoveIfIdle -DirInfo $_ -Running $running) { $deleted++ }
		}

	if ($deleted) { fNote "deleted $deleted copy(ies) older than $MaxAgeDays days" }
}


## Delete leftover partial copies (an interrupted run's .tmp dirs), unless
## fresh enough to be a concurrent run's copy in progress.
function fDeleteStaleTmp {
	$cutoff = (Get-Date).AddHours(-1)
	Get-ChildItem -LiteralPath $TargetDir @CopyGciType -Filter "${DogfoodPrefix}_*.tmp" -ErrorAction SilentlyContinue |
		Where-Object { $_.LastWriteTime -lt $cutoff } |
		ForEach-Object {
			try {
				Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction Stop
				fNote "deleted stale partial copy: $($_.Name)"
			} catch { }
		}
}


## Windows only: clear out copies left by the pre-single-exe layout, when a copy
## was the whole app\+mingw64\ tree rather than one exe. The sweeps above look for
## files now, so those dirs are invisible to them and would sit there for good at
## a couple of hundred MB each. Same self-heal the cicd dogfood stage does to the
## old bundle folder in the synced drop.
function fRetireLegacyCopies {
	if (-not $CopyIsFile) { return }

	$rx      = "^$([regex]::Escape($DogfoodPrefix))_\d{8}-\d{6}(_[a-z0-9]+)?(\.tmp)?$"
	$running = @(fRunningExePaths)

	Get-ChildItem -LiteralPath $TargetDir -Directory -Filter "${DogfoodPrefix}_*" -ErrorAction SilentlyContinue |
		Where-Object { $_.Name -match $rx } |
		ForEach-Object {
			$prefix = $_.FullName + [System.IO.Path]::DirectorySeparatorChar
			if ($running | Where-Object { $_.StartsWith($prefix) }) {
				fNote "kept (running): $($_.Name)"
				return
			}
			try {
				Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction Stop
				fNote "retired pre-single-exe copy: $($_.Name)"
			} catch {
				fNote "kept (locked): $($_.Name)"
			}
		}
}


## All stamped copies as objects { File, Name, Tag, Stamp(DateTime) }, current
## OS's tag only - a lin prefix can't run on Windows or vice versa.
function fHeldCopies {
	$rx = "^$([regex]::Escape($DogfoodPrefix))_(?<stamp>\d{8}-\d{6})_$([regex]::Escape($SourceTag))$([regex]::Escape($CopyExt))$"
	Get-ChildItem -LiteralPath $TargetDir @CopyGciType -Filter "${DogfoodPrefix}_*" -ErrorAction SilentlyContinue |
		ForEach-Object {
			if ($_.Name -match $rx) {
				[pscustomobject]@{
					File  = $_
					Name  = $_.Name
					Stamp = fParseStamp $Matches.stamp
				}
			}
		}
}


## Newest held copy (object from fHeldCopies), or $null.
function fNewestCopy {
	fHeldCopies | Sort-Object Stamp -Descending | Select-Object -First 1
}


## A copy's build time: the stamp embedded in its name if present, else mtime.
function fBuildTime {
	param([Parameter(Mandatory)]$DirInfo)
	if ($DirInfo.Name -match "_(?<stamp>\d{8}-\d{6})(?:_[a-z0-9]+)?(?:\.[A-Za-z0-9]+)?$") {
		return fParseStamp $Matches.stamp
	}
	return $DirInfo.LastWriteTime
}


## Parse a 'yyyyMMdd-HHmmss' stamp to a DateTime.
function fParseStamp {
	param([Parameter(Mandatory)][string]$Stamp)
	return [datetime]::ParseExact($Stamp, $StampFormat, [System.Globalization.CultureInfo]::InvariantCulture)
}


## Delete one copy dir unless a running process lives inside it. Returns $true
## if deleted.
function fRemoveIfIdle {
	param(
		[Parameter(Mandatory)]$DirInfo,
		[string[]]$Running
	)
	if ($CopyIsFile) {
		## Single exe: in use = a running process whose image IS this exact copy.
		$inUse = $Running | Where-Object { $_ -ieq $DirInfo.FullName }
	} else {
		## Prefix dir: in use = a running process whose image lives inside the copy.
		$prefix = $DirInfo.FullName + [System.IO.Path]::DirectorySeparatorChar
		$inUse = $Running | Where-Object { $_.StartsWith($prefix) }
	}
	if ($inUse) {
		fNote "kept (running): $($DirInfo.Name)"
		return $false
	}
	try {
		Remove-Item -LiteralPath $DirInfo.FullName -Recurse -Force -ErrorAction Stop
		return $true
	} catch {
		fNote "kept (locked): $($DirInfo.Name)"
		return $false
	}
}


## Full image paths of all currently running processes (best-effort). Paths we
## can't read are skipped. Worked out once per run - every sweep asks the same
## question, and the answer is not cheap.
##
## On Windows the Path property throws for each of the few hundred protected
## system processes, and swallowing those exceptions costs whole seconds; one CIM
## query answers the same thing in a fraction of the time. Elsewhere the property
## is the cheap way round.
function fRunningExePaths {
	if ($null -ne $script:RunningPaths) { return $script:RunningPaths }

	if ($IsWindows) {
		$script:RunningPaths = @(
			Get-CimInstance -ClassName Win32_Process -Property ExecutablePath -ErrorAction SilentlyContinue |
				ForEach-Object { $_.ExecutablePath } |
				Where-Object { $_ }
		)
	} else {
		$script:RunningPaths = @(
			Get-Process -ErrorAction SilentlyContinue |
				ForEach-Object { try { $_.Path } catch { $null } } |
				Where-Object { $_ }
		)
	}
	return $script:RunningPaths
}


## Launch a stamped copy detached, wiring the runtime env at the copy the same
## way the fixed dogfood wrapper (Linux) / wine runner (Windows) do. The env
## edits ride process inheritance; this launcher exits right after, so nothing
## else sees them.
function fLaunchNemo {
	param(
		[Parameter(Mandatory)][string]$CopyPath,
		[string[]]$PassArgs
	)

	if ($IsWindows) {
		## Single self-contained exe - the copy IS the exe, and it carries its whole
		## GTK runtime (dlls, schemas, data) packed inside, so nothing is wired.
		$exe = $CopyPath
	} else {
		$exe = Join-Path $CopyPath "bin/nemo-anywhere"
		## The extension lib sits under whatever multiarch dir the prefix was built
		## for, which is not x86_64 on arm64 - find it rather than bake one in.
		$libDirs = @(Get-ChildItem -LiteralPath (Join-Path $CopyPath "lib") -Directory -ErrorAction SilentlyContinue |
			Where-Object { $_.Name -like "*-linux-gnu*" } | ForEach-Object { $_.FullName })
		$libDirs += (Join-Path $CopyPath "lib")
		$env:LD_LIBRARY_PATH = ($libDirs -join ":") +
			$(if ($env:LD_LIBRARY_PATH) { ":" + $env:LD_LIBRARY_PATH } else { "" })
		$env:GSETTINGS_SCHEMA_DIR = (Join-Path $CopyPath "share/glib-2.0/schemas") +
			$(if ($env:GSETTINGS_SCHEMA_DIR) { ":" + $env:GSETTINGS_SCHEMA_DIR } else { "" })
		$env:XDG_DATA_DIRS = (Join-Path $CopyPath "share") + ":" +
			$(if ($env:XDG_DATA_DIRS) { $env:XDG_DATA_DIRS } else { "/usr/local/share:/usr/share" })
	}

	if (-not (Test-Path -LiteralPath $exe)) {
		fFail "copy is missing its main binary: $exe"
	}
	return fStartApp -Exe $exe -ArgList $PassArgs
}


## Fall back to whatever file manager is installed, in $FallbackManagers order.
function fLaunchFallback {
	param([string[]]$PassArgs)

	foreach ($cand in $FallbackManagers) {
		$path = fFindOnPath $cand
		if (-not $path) { continue }
		fNote "falling back to ${cand}: $path"
		return fStartApp -Exe $path -ArgList $PassArgs
	}

	fFail ("no file manager available (no dogfood copy/source, and none of " +
		($FallbackManagers -join ", ") + " on PATH)")
}


## Resolve an executable's full path from PATH, or $null. -CommandType Application
## keeps it to real executables (never a shell function/alias of the same name).
function fFindOnPath {
	param([Parameter(Mandatory)][string]$Exe)
	$cmd = Get-Command $Exe -CommandType Application -ErrorAction SilentlyContinue |
		Select-Object -First 1
	if ($cmd) { return $cmd.Source }
	return $null
}


## Launch the app detached and return the Process, so the launcher can exit
## immediately while the app keeps running. Returning the Process lets a caller
## (e.g. a test harness) stop this exact instance by PID - matching on name
## risks hitting another copy launched elsewhere.
##
## Windows needs nothing extra: with no redirections Start-Process goes through
## ShellExecute, which already gives the app its own process and console.
##
## Unix hands the app our own stdout/stderr, so it holds the caller's pipe open
## for its whole life - `n8runfm | cat` blocks until the app quits, and its
## warnings land in a console that has long moved on. Fix it in the shell rather
## than with Start-Process -RedirectStandard*: those pump through a pipe owned by
## THIS process, so once we exit the app's output is dropped and it eventually
## blocks on the full pipe. 'sh -c exec' re-points all three streams at real fds
## and then execs in place, so the app itself owns them; setsid in front (also
## exec-in-place) gives it a fresh session, out of reach of a terminal hangup.
## Both execs keep the PID, so the one reported is the app's own. The log path
## rides an env var to keep quotes out of the command line.
function fStartApp {
	param(
		[Parameter(Mandatory)][string]$Exe,
		[string[]]$ArgList
	)

	$sp = @{ FilePath = $Exe; PassThru = $true }
	if ($ArgList -and $ArgList.Count) { $sp.ArgumentList = $ArgList }

	if (-not $IsWindows) {
		$env:N8RUNFM_APPLOG = $AppLog
		$shArgs = @("-c", 'exec "$0" "$@" </dev/null >>"$N8RUNFM_APPLOG" 2>&1', $Exe)
		if ($ArgList -and $ArgList.Count) { $shArgs += $ArgList }

		$setsid = fFindOnPath "setsid"
		if ($setsid) {
			$sp.FilePath     = $setsid
			$sp.ArgumentList = @("/bin/sh") + $shArgs
		} else {
			## No setsid (macOS, some BSDs): streams still detached, session not.
			fWarn "setsid not found; launching without a new session"
			$sp.FilePath     = "/bin/sh"
			$sp.ArgumentList = $shArgs
		}
	}

	## Start-Process joins ArgumentList into one command line with a naive space
	## join and no quoting, then the target re-splits it (.NET on unix, the MSVCRT
	## parser on Windows). Quote every element so args with spaces, quotes or
	## trailing backslashes survive that round trip.
	## ContainsKey, not $sp.ArgumentList: under Set-StrictMode -Version Latest a
	## hashtable member that was never set throws rather than answering $null, so
	## the plain read blew up every launch that passed no arguments.
	if ($sp.ContainsKey("ArgumentList")) {
		$sp.ArgumentList = @($sp.ArgumentList | ForEach-Object { fQuoteArg $_ })
	}

	## RunAs is a ShellExecute verb, so Windows only - and only reached when the
	## whole launcher is already elevated (the entry point self-elevates first), so
	## this raises no second consent prompt.
	if ($IsWindows -and $RunAsAdmin) { $sp.Verb = "RunAs" }

	try {
		$proc = Start-Process @sp
	} catch {
		## RunAs throws if UAC is declined; surface it plainly.
		fFail "launch failed for $Exe ($($_.Exception.Message))"
	}

	$how = if ($IsWindows -and $RunAsAdmin) { " as admin," } else { "" }
	fItem "ok" "launched" ("{0}{1} pid {2}" -f [System.IO.Path]::GetFileName($Exe), $how, $proc.Id)
	return $proc
}


## Quote one argument so it survives Start-Process joining ArgumentList into a
## single command line and the target re-splitting it. MSVCRT/CommandLineToArgvW
## rules: only quote when needed; double the backslashes that precede a quote or
## end the arg; escape embedded quotes.
function fQuoteArg {
	param([string]$Arg)
	if ($Arg -ne '' -and $Arg -notmatch '[\s"]') { return $Arg }
	$sb = [System.Text.StringBuilder]::new()
	[void]$sb.Append('"')
	$slashes = 0
	foreach ($ch in $Arg.ToCharArray()) {
		if ($ch -eq '\') {
			$slashes++
		} elseif ($ch -eq '"') {
			[void]$sb.Append('\', ($slashes * 2) + 1)
			[void]$sb.Append('"')
			$slashes = 0
		} else {
			if ($slashes -gt 0) { [void]$sb.Append('\', $slashes); $slashes = 0 }
			[void]$sb.Append($ch)
		}
	}
	if ($slashes -gt 0) { [void]$sb.Append('\', $slashes * 2) }
	[void]$sb.Append('"')
	return $sb.ToString()
}


## Run header. Bracketed the way the cicd scripts do it, so a launcher run and a
## pipeline run read the same.
function fBanner {
	param([string]$Msg)
	fLog "=== $Msg ==="
	Write-Host ""
	Write-Host "[ $Msg ]" -ForegroundColor Cyan
}


## Start a step. Everything a step decides prints under it as an fItem row, so a
## run reads as a short report rather than a stream of loose notes.
function fStep {
	param([string]$Msg)
	fLog "-- $Msg"
	$script:StepRows = 0
	Write-Host ""
	Write-Host "  $Msg" -ForegroundColor Cyan
}


## One row under a step: a status tag, an optional label column, then free text.
## Only the tag is coloured - a whole coloured line is a wall of green.
function fItem {
	param([string]$Status = "-", [string]$Label = "", [string]$Detail = "")

	$script:StepRows++
	fLog ("{0,-5} {1,-12} {2}" -f $Status, $Label, $Detail).TrimEnd()

	$colour = switch ($Status) {
		"ok"   { "Green" }
		"skip" { "Yellow" }
		"warn" { "Yellow" }
		"fail" { "Red" }
		default { "DarkGray" }
	}
	Write-Host "    " -NoNewline
	Write-Host ("{0,-5}" -f $Status) -NoNewline -ForegroundColor $colour
	Write-Host (" {0,-12} {1}" -f $Label, $Detail).TrimEnd()
}


## Byte count for a human, one decimal. Only ever describes a copy we just made,
## so no need to care about anything past GB.
function fHumanSize {
	param([Parameter(Mandatory)][int64]$Bytes)
	if ($Bytes -ge 1GB) { return ("{0:n1} GB" -f ($Bytes / 1GB)) }
	if ($Bytes -ge 1MB) { return ("{0:n1} MB" -f ($Bytes / 1MB)) }
	if ($Bytes -ge 1KB) { return ("{0:n1} KB" -f ($Bytes / 1KB)) }
	return "$Bytes B"
}


## Informational row (and the run log).
function fNote { param([string]$Msg); fItem "-" "" $Msg }

## Non-fatal problem (and the run log). Pass -Gui to also surface it in the
## end-of-run dialog (the shortcut case, where the console flashes shut) - reserved
## for real problems (a failed copy), not benign skips (an offline source).
function fWarn {
	param([string]$Msg, [switch]$Gui)
	fItem "warn" "" $Msg
	if ($Gui) { $script:RunWarnings += $Msg }
}


## Fatal error to stderr (and the run log), then stop. Pops a dialog first when GUI
## feedback is on, so a shortcut click shows WHY instead of a blank flash.
##
## WriteErrorLine rather than Write-Error: under $ErrorActionPreference = "Stop" a
## Write-Error throws, so the exit below never runs and the caller reads a thrown
## error instead of rc 1 - and its formatter folds a multi-line message (the option
## list) onto one wrapped line.
function fFail {
	param([string]$Msg)
	fLog "FAIL: $Msg"
	if ($script:GuiFeedback) { fGuiShow -Msg $Msg -Icon Error -Title "Nemo Anywhere dogfood - failed" }
	$Host.UI.WriteErrorLine("n8runfm: $Msg")
	exit 1
}


## True when this process is running elevated (Administrators / high integrity).
## Windows-only notion; everything else answers false and never elevates.
function fIsElevated {
	if (-not $IsWindows) { return $false }
	$id = [System.Security.Principal.WindowsIdentity]::GetCurrent()
	return (New-Object System.Security.Principal.WindowsPrincipal($id)).IsInRole(
		[System.Security.Principal.WindowsBuiltInRole]::Administrator)
}


## True when we were double-clicked (a .lnk / Explorer launch) rather than started
## from a shell - Explorer is the parent of a shortcut click, a terminal (pwsh/cmd/
## wt) is the parent of a command-line run. Used to auto-enable GUI feedback so a
## flash-and-close shortcut can still report a failure. Best-effort -> $false.
function fLaunchedFromShortcut {
	if (-not $IsWindows) { return $false }
	try {
		$parentId = (Get-CimInstance Win32_Process -Filter "ProcessId=$PID" -ErrorAction Stop).ParentProcessId
		$parent   = (Get-Process -Id $parentId -ErrorAction Stop).ProcessName
		return ($parent -ieq "explorer")
	} catch { return $false }
}


## Show a modal message box. Never throws - feedback must not be the thing that
## breaks a launch; a no-op if WinForms can't load (which is every non-Windows box).
function fGuiShow {
	param(
		[Parameter(Mandatory)][string]$Msg,
		[ValidateSet("Error", "Warning", "Information")][string]$Icon = "Information",
		[string]$Title = "Nemo Anywhere dogfood"
	)
	try {
		Add-Type -AssemblyName System.Windows.Forms -ErrorAction Stop
		[System.Windows.Forms.MessageBox]::Show(
			$Msg, $Title,
			[System.Windows.Forms.MessageBoxButtons]::OK,
			[System.Windows.Forms.MessageBoxIcon]::$Icon) | Out-Null
	} catch { }
}


## Append a timestamped line to the run log. Best-effort: logging must never be
## the thing that stops a launch.
function fLog {
	param([string]$Msg)
	try {
		Add-Content -LiteralPath $RunLog -Encoding utf8 -Value `
			("{0}  {1}" -f (Get-Date -Format "yyyy-MM-dd HH:mm:ss"), $Msg)
	} catch { }
}


## Keep a log from growing without bound.
function fTrimLog {
	param([Parameter(Mandatory)][string]$Path)
	try {
		if ((Test-Path -LiteralPath $Path) -and (Get-Item -LiteralPath $Path).Length -gt 256KB) {
			$tail = Get-Content -LiteralPath $Path -Tail 500
			Set-Content -LiteralPath $Path -Value $tail -Encoding utf8
		}
	} catch { }
}


## Remove any mark-of-the-web this script picked up from the sync layer, so an
## unsigned script under a RemoteSigned policy isn't silently refused on the
## NEXT run. Best-effort; never let it stop a launch.
function fSelfHealMotw {
	try {
		$zone = Get-Content -LiteralPath $PSCommandPath -Stream Zone.Identifier -ErrorAction SilentlyContinue
		if ($zone) {
			Unblock-File -LiteralPath $PSCommandPath -ErrorAction Stop
			fNote "cleared mark-of-the-web on this script"
		}
	} catch {
		fWarn "couldn't clear mark-of-the-web on this script ($($_.Exception.Message))"
	}
}


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Script entry point

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

## Problems worth surfacing at the end (a failed copy etc.), shown in a dialog when
## launched from a shortcut. Must exist before any fWarn -Gui / fFail can run.
$script:RunWarnings = @()

## Rows printed under the current step, so a step that decided nothing can say so.
## Must exist before the first fItem, which under StrictMode is not allowed to
## increment a variable nobody has declared.
$script:StepRows = 0

## Consume our own flags; forward everything else to the app.
##   --no-admin  run without elevating. On Windows elevation is on by default: the
##               whole launcher self-elevates below, so the copy, the log and the
##               launched app all get admin rights. '--admin' is still accepted, and
##               is the only way to ask for it on unix - where it is then refused.
##   --gui       force the end-of-run / failure dialog on (auto-on for a shortcut click).
## Single-dash spellings are accepted too: '-admin' is what a PowerShell user types,
## and it collides with nothing in the app's own option set.
$wantAdmin = $IsWindows
$forceGui  = $false
$passArgs  = @()
foreach ($arg in $args) {
	switch -Regex ($arg) {
		'^--?admin$'    { $wantAdmin = $true;  continue }
		'^--?no-admin$' { $wantAdmin = $false; continue }
		'^--?gui$'      { $forceGui  = $true;  continue }
		default         { $passArgs += $arg }
	}
}

$script:GuiFeedback = $forceGui -or (fLaunchedFromShortcut)

## Refuse a flag the app doesn't know before anything else happens - ahead of the
## UAC prompt in particular, so a typo can't cost a consent click and a copy first.
fCheckPassArgs -PassArgs $passArgs

if ($wantAdmin -and -not $IsWindows) {
	fWarn "--admin is Windows-only; ignoring (running a file manager as root is a footgun)"
	$wantAdmin = $false
}

## Self-elevate: unless '--no-admin', and not already elevated, relaunch the whole
## script elevated and hand off. Everything then runs high-integrity, so it no longer
## matters whether the target dir grants a normal user write - the real fix for
## "a shortcut click launches a stale build". The relaunch carries the original args
## plus '--gui' (its parent is the UAC broker, not Explorer, so it can't re-detect
## the shortcut). If consent is declined we DON'T abort - we fall through and run
## non-elevated so the user still gets a file manager, with a dialog saying it may
## be stale.
if ($wantAdmin -and -not (fIsElevated)) {
	$self = (Get-Process -Id $PID).Path      # the pwsh.exe hosting this script
	$fwd  = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath) + $args + "--gui"
	$fwd  = @($fwd | ForEach-Object { fQuoteArg $_ })
	try {
		Start-Process -FilePath $self -Verb RunAs -ArgumentList $fwd -ErrorAction Stop | Out-Null
		exit 0
	} catch {
		fWarn "elevation declined; running without admin (a newer build may not copy)"
		if ($script:GuiFeedback) {
			fGuiShow -Icon Warning -Title "Nemo Anywhere dogfood - not elevated" -Msg (
				"Administrator access was declined.`n`nRunning without it - a newer " +
				"build may not copy in, so an older one could launch.")
		}
	}
}

## Elevated (self- or from an elevated shell): also launch the app elevated.
if ($wantAdmin) { $RunAsAdmin = $true }

## Kick everything off, passing through whatever's left.
fMain -PassArgs $passArgs

## Surface any real problems (a failed copy etc.) for the shortcut case.
if ($script:GuiFeedback -and $script:RunWarnings.Count) {
	fGuiShow -Icon Warning -Title "Nemo Anywhere dogfood" -Msg (
		"Launched, but with issues:`n`n - " + ($script:RunWarnings -join "`n - "))
}

exit 0


##	History:
##		- 2026-09-01: Elevate by default on Windows; '--no-admin' opts out. A
##		  filtered token has no SeCreateSymbolicLinkPrivilege, so an unelevated app
##		  can't make a symlink at all. Unchanged on unix.
##		- 2026-08-24: A source with more than one root now probes all of them and
##		  takes the newest, instead of stopping at the first that answers. The roots
##		  are usually one place spelled two ways, but not on every box - where they
##		  are two real trees, an older one listed first was quietly winning and a
##		  newer build at the second root was never seen. A unix path under /mnt,
##		  /media or /net gets the network probe timeout too, so probing the extra
##		  roots can't wedge on a dead mount.
##		- 2026-08-22: Launch opens at a configured startup location (the 0_links
##		  folder on either platform) when nothing else was named on the command
##		  line, so a shortcut click lands somewhere useful instead of the app's
##		  own default.
##		- 2026-08-21: Every source is probed now instead of taking the first that
##		  answers, and the newest build wins wherever it sits: this box's own repo
##		  build, b23 across the network, and the synced dogfood drop - none of them
##		  a fallback for the others, so whichever is holding the newest build is the
##		  one used, and any one of them on its own is enough to launch. Two sources
##		  holding the same build (identical size, stamps within a couple of seconds)
##		  are reported once rather than counted twice, so neither a junction nor a
##		  stamp the sync layer rounded can pass one build off as two - the second of
##		  which would have cost a needless copy across the network. The probe reads
##		  mtime and size in the same guarded call it used to spend on existence
##		  alone. Output is now a step-by-step
##		  report - housekeeping, sources, build in hand, launch - with a status tag
##		  per row, and the same lines go to the run log.
##		- 2026-08-19: '--admin' self-elevates the whole launcher and launches the app
##		  elevated, matching n8runterm. Report failures in a dialog for the shortcut
##		  case (the console flashes shut); new '--gui' flag, auto-on when double-
##		  clicked. Log the pass-through args, and refuse an option the app doesn't
##		  know rather than forwarding it - the packed exe is GUI-subsystem, so its
##		  parse error goes nowhere and an unknown flag just looked like a no-op. Drop
##		  the second round of arg quoting at the call sites (fStartApp already does
##		  it), which was wrapping any path with a space in literal quotes.
##		- 2026-08-15: A source on a network share is given 1.5s to answer and then
##		  written off, instead of blocking the launch for the SMB timeout. Windows
##		  also retires copies left by the old app\+mingw64\ layout, which the
##		  file-shaped sweeps can't see. Reading every process image path cost ~4s a
##		  sweep on Windows - one CIM query now, cached for the run. The newest copy
##		  no longer ages out, so an older source can't force a re-copy every run.
##		- 2026-08-04: Windows copies are now a single packed exe (a file), not the
##		  app\+mingw64\ bundle - source is the packed win-portable exe, the pool holds
##		  '.exe' copies, and launch needs no env wiring. Linux stays a prefix dir.
##		- 2026-07-23: Launch detached - own session, stdio off the caller - so
##		  the launcher returns at once and the app outlives it.
##		- 2026-07-23: Created (nemo-anywhere analog of silkterm's n8runterm.ps1;
##		  prefix-dir copies instead of a single exe).
