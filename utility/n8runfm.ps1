#!/usr/bin/env pwsh

##	Purpose:
##		- Dogfood launcher for Nemo Anywhere, same script on Linux, Windows and macOS.
##		  Copies the current build out of the synced dogfood dir into a local pool of
##		  date-stamped versions, points a symlink at the newest, and runs it with
##		  whatever arguments were passed.
##		- A version is named '<name>_<YYYYMMDD-HHMMSS>_<role>' (+ '.exe' on Windows)
##		  where the stamp is the source build's mtime. Stamped names mean a running
##		  copy never blocks the next one, and the symlink is the only fixed name.
##		- On Windows a build is one packed self-contained exe. Everywhere else it is a
##		  relocatable prefix tree, so a version is a directory and the symlink points
##		  at the wrapper in its bin/, which sorts out the runtime environment itself.
##		- The pool is GFS-rotated on every run: the newest of each completed hour, day,
##		  week, month and year, plus the most recent few, plus the very first build,
##		  which is kept forever. On top of that a hard budget - at most 10 versions, at
##		  least 5, and between those two only as many as fit in 1 GB. A version with a
##		  running process in it is never removed.
##		- Copies of one build do not agree on mtime (the sync layer restamps what it
##		  carries), so a build that is already held is settled on its bytes, not its
##		  date. Otherwise the same build comes back in under a new stamp every run.
##		- On Windows the whole launcher self-elevates: making a symlink needs a
##		  privilege a filtered token does not have, so an unelevated run could not
##		  repoint the link and would keep launching the version it already had.
##		  '--no-admin' opts out.
##		- Opens at a configured startup location when the caller names none, and falls
##		  back to another file manager when there is no build to run at all.
##		- '--no-admin', '--no-update' and '--gui' are consumed here; everything else is
##		  checked against the app's own options and forwarded.
##	History: At bottom of script.

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Configuration

$ProgramName = "nemo-anywhere"

## Windows needs the extension on both the pool copies and the symlink, or nothing
## will run them.
$ExeExt  = if ($IsWindows) { ".exe" } else { "" }
$ExeName = "${ProgramName}${ExeExt}"

## What a build looks like. Windows packs the whole GTK runtime into one exe; every
## other platform ships the relocatable prefix, so a version is a directory and
## $PayloadMainBin is the wrapper inside it that the symlink and the launch point at.
## $PayloadIdBin is the real binary, which is what a build is identified by - the
## wrapper is generated boilerplate and is byte-identical between builds.
$PayloadIsFile  = $IsWindows
$PayloadMainBin = if ($IsWindows) { "" } else { "bin/${ProgramName}" }
$PayloadIdBin   = if ($IsWindows) { "" } else { "libexec/${ProgramName}" }

## Where a build arrives from: the synced dogfood dir for this platform, which is
## what the pipeline's dogfood stage publishes to. One entry each for now, but the
## sync tree gets spelled differently from box to box, so each platform keeps a list
## and the first one actually holding a build wins.
$SourceDirs = if ($IsWindows) {
	@(
		(Join-Path $HOME "synced\0-0\common\exec\app\mswin")
	)
} elseif ($IsMacOS) {
	@(
		(Join-Path $HOME "synced/0-0/common/exec/app/macos")
	)
} else {
	@(
		(Join-Path $HOME "synced/0-0/common/exec/app/linux")
	)
}

## Where the pool lives and what the fixed name is. Deliberately local rather than
## synced: versions churn on every build and have no business riding Dropbox. A
## packed exe also starts noticeably faster from LOCALAPPDATA than from a synced
## tree, where a scanner or the sync client is watching every read.
if ($IsWindows) {
	$InstallDir = Join-Path $env:LOCALAPPDATA "Programs"
} elseif ($IsMacOS) {
	$InstallDir = Join-Path $HOME "Applications"
} else {
	$InstallDir = Join-Path $HOME ".local/bin"
}
$TargetDir = Join-Path $InstallDir "${ProgramName}_versions"
$LinkPath  = Join-Path $InstallDir $ExeName

## Pool from before it was GFS-rotated: stamped copies sat loose in $InstallDir under
## their own prefix. Retired on sight, since neither sweep below can see them.
$LegacyPrefix = "nemofmdf"

## Pool budget. Never more than $MaxVersions, never fewer than $MinVersions, and
## between the two only as many as fit in $MaxPoolBytes. The oldest version held is
## exempt: it is the one build that is kept forever.
$MaxVersions  = 10
$MinVersions  = 5
$MaxPoolBytes = 1GB

## GFS retention, before the budget above trims it. Roles are retrospective: a
## version is tagged hour/day/week/month/year only once that period has ended and it
## is the last one in it. Sums to the budget's ceiling plus the first.
$KeepFrequent = 3
$KeepHourly   = 2
$KeepDaily    = 2
$KeepWeekly   = 1
$KeepMonthly  = 1
$KeepYearly   = 1

## Stamp format shared by every copy name and date comparison below.
$StampFormat = "yyyyMMdd-HHmmss"

## Same instant, spelled for a person.
$StampDisplay = "yyyy-MM-dd HH:mm:ss"

## Launch elevated. Windows only, on by default, '--no-admin' turns it off. Set from
## the flag at the entry point, so this initial value is not the default.
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
	"--no-default-window", "-n", "--no-desktop", "--tabs", "-t", "--select", "-s",
	"--existing-window", "--fix-cache", "--debug", "--quit", "-q", "--reset", "--about",
	"--help", "--help-all", "-h", "-?",
	"--display", "--screen", "--class", "--name", "--sync", "--gtk-module",
	"--g-fatal-warnings", "--gdk-debug", "--gdk-no-debug", "--gtk-debug",
	"--gtk-no-debug", "--help-gtk", "--help-gtk-1", "--help-gdk"
)

## Options whose value arrives as its own token. That token is a value, not a
## location, even though it doesn't lead with '-'.
$ValueAppOptions = @(
	"--geometry", "-g", "--display", "--screen", "--class", "--name",
	"--gtk-module", "--gdk-debug", "--gdk-no-debug", "--gtk-debug", "--gtk-no-debug"
)

## Fallback file managers, tried in order when there is no build of ours to run.
## Launched plainly (generic managers accept a path arg at most).
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

## Logs. On unix they stay out of a PATH directory and keep the path the project's
## notes refer to; on Windows the pool dir is as good a home as any.
$LogDir = if ($IsWindows) { $TargetDir } else { Join-Path $HOME ".local/share/nemo-anywhere-dogfood" }

## Per-run decision log, so a window that closes on its own cannot lose the reason
## behind a launch.
$RunLog = Join-Path $LogDir "n8runfm.log"

## Unix only: where the detached app's own output goes (GTK/GLib gripes, the trash
## and delete job log, and any crash message), since it no longer has the caller's
## console. Appended to and trimmed like the run log.
$AppLog = Join-Path $LogDir "n8runfm-app.log"

## Running-process image paths, filled in on first use (see fRunningExePaths).
$RunningPaths = $null


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Functions

## Entry point.
function fMain {
	param([string[]]$PassArgs, [switch]$NoUpdate)

	foreach ($dir in @($TargetDir, $LogDir)) {
		if (-not (Test-Path -LiteralPath $dir)) {
			New-Item -ItemType Directory -Path $dir -Force | Out-Null
		}
	}

	fTrimLog $RunLog
	if (-not $IsWindows) { fTrimLog $AppLog }
	## Log the args too: a launch that dies on one of them leaves no other trace.
	fLog ("=== run: PS {0}, script {1}, admin {2}, args [{3}] ===" -f `
		$PSVersionTable.PSVersion, $PSCommandPath, $RunAsAdmin, ($PassArgs -join " "))

	fBanner "n8runfm - Nemo Anywhere dogfood launcher"

	fStep "Housekeeping"
	if ($IsWindows) { fSelfHealMotw }
	fDeleteStalePartials
	fRetireLegacyCopies
	if (-not $script:StepRows) { fItem "-" "" "nothing to clean up" }

	if ($NoUpdate) {
		fStep "Build in hand"
		fItem "skip" "update" "--no-update: running what is already held"
	} else {
		fStep "Build in hand"
		fCopyIfNewer
		fStep "Pool"
		fRotate
		fUpdateLink
	}

	fStep "Launch"
	$PassArgs = fAddStartupLocation -PassArgs $PassArgs
	$exe = fRunTarget
	if ($exe) {
		## The Process goes nowhere - it's there for a test harness, and letting it
		## reach the output stream would dump a process table on the way out.
		$null = fLaunchApp -Exe $exe -PassArgs $PassArgs
		return
	}

	fWarn -Gui "no ${ProgramName} build held and none in $($SourceDirs -join ', ')"
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
			"`n`nLauncher flags: --admin, --no-admin, --no-update, --gui" +
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


## The build to consider: the first configured source dir that actually holds one.
## Returns a FileInfo (Windows) or DirectoryInfo (the prefix everywhere else), or
## $null.
function fPickSource {
	foreach ($dir in $SourceDirs) {
		$path = Join-Path $dir $ExeName
		$item = Get-Item -LiteralPath $path -ErrorAction SilentlyContinue
		if (-not $item) { continue }
		if ($PayloadIsFile -ne ($item -is [System.IO.FileInfo])) { continue }
		if (fIdFile $item.FullName) { return $item }
	}
	return $null
}


## The one file that says which build a payload is: the payload itself on Windows,
## the real binary inside the prefix everywhere else. Its mtime is the build stamp
## and its bytes settle whether two copies are the same build. Returns a FileInfo,
## or $null when the payload is incomplete.
function fIdFile {
	param([Parameter(Mandatory)][string]$PayloadPath)
	$path = if ($PayloadIdBin) { Join-Path $PayloadPath $PayloadIdBin } else { $PayloadPath }
	return (Get-Item -LiteralPath $path -ErrorAction SilentlyContinue)
}


## Bring in the synced build when it is newer than everything held. Copies to a
## '.partial' name and renames into place, so a run that dies mid-copy cannot leave
## a half-written version that later reads as a perfectly good one.
function fCopyIfNewer {
	$src = fPickSource
	if (-not $src) {
		fItem "skip" "source" "no build in $($SourceDirs -join ', ')"
		$held = fNewestCopy
		if ($held) { fItem "-" "held" ("{0}   {1}" -f $held.Stamp.ToString($StampDisplay), $held.Name) }
		return
	}

	$srcId     = fIdFile $src.FullName
	$stamp     = $srcId.LastWriteTime.ToString($StampFormat)
	$stampTime = fParseStamp $stamp
	$newest    = fNewestCopy

	if ($newest) { fItem "-" "held" ("{0}   {1}" -f $newest.Stamp.ToString($StampDisplay), $newest.Name) }
	else         { fItem "-" "held" "nothing held yet" }

	if ($newest -and $newest.Stamp -ge $stampTime) {
		fItem "ok" "source" "already current ($($src.FullName))"
		return
	}

	## The sync layer restamps what it carries, so a build already held keeps looking
	## new. Settle it on the bytes and just take the newer stamp, which makes the
	## cheap date test above answer it next run.
	$twin = fHeldMatching -SourceId $srcId
	if ($twin) {
		$restamped = Join-Path $TargetDir "${ProgramName}_${stamp}${ExeExt}"
		if (fHeldCopies | Where-Object { $_.Stamp -eq $stampTime }) {
			fItem "ok" "source" "same build as $($twin.Name); already held under that stamp"
			return
		}
		try {
			Move-Item -LiteralPath $twin.Payload.FullName -Destination $restamped -ErrorAction Stop
			fItem "ok" "source" "same build as $($twin.Name) - restamped to $stamp"
		} catch {
			fItem "-" "source" "same build as $($twin.Name), but the rename was refused"
		}
		return
	}

	$dst   = Join-Path $TargetDir "${ProgramName}_${stamp}${ExeExt}"
	$tmp   = "$dst.partial"
	$clock = [System.Diagnostics.Stopwatch]::StartNew()
	fItem "-" "copy" "$($src.FullName) -> $(Split-Path $dst -Leaf)"
	try {
		if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
		if ($PayloadIsFile) { Copy-Item -LiteralPath $src.FullName -Destination $tmp -Force -ErrorAction Stop }
		else                { Copy-Item -LiteralPath $src.FullName -Destination $tmp -Recurse -Force -ErrorAction Stop }
		## A synced-sourced exe can carry a mark-of-the-web; clear it so the launch
		## isn't SmartScreen-blocked. Best-effort, and no-op for a prefix dir.
		if ($PayloadIsFile) { try { Unblock-File -LiteralPath $tmp -ErrorAction SilentlyContinue } catch { } }
		Move-Item -LiteralPath $tmp -Destination $dst -Force -ErrorAction Stop
		$clock.Stop()
		fItem "ok" "copy" ("done, {0} in {1:n2}s" -f (fHumanSize (fPayloadSize $dst)), $clock.Elapsed.TotalSeconds)
	} catch {
		fWarn -Gui "couldn't copy the build ($($_.Exception.Message))"
		if (Test-Path -LiteralPath $tmp) { try { Remove-Item -LiteralPath $tmp -Recurse -Force } catch { } }
	}
}


## A held version built from the same bytes as $SourceId, or $null. Size first,
## because these are large and a hash of every one of them is not free.
function fHeldMatching {
	param([Parameter(Mandatory)]$SourceId)

	## Newest first: every twin has the same bytes, so which one comes back only
	## decides which one gets carried forward under the new stamp. Taking the newest
	## keeps the pool's history in order.
	$candidates = @(fHeldCopies |
		Where-Object { $_.IdFile -and $_.IdFile.Length -eq $SourceId.Length } |
		Sort-Object Stamp -Descending)
	if (-not $candidates) { return $null }

	try {
		$want = (Get-FileHash -LiteralPath $SourceId.FullName -Algorithm SHA256 -ErrorAction Stop).Hash
	} catch { return $null }

	foreach ($copy in $candidates) {
		try {
			if ((Get-FileHash -LiteralPath $copy.IdFile.FullName -Algorithm SHA256 -ErrorAction Stop).Hash -eq $want) {
				return $copy
			}
		} catch { }
	}
	return $null
}


## GFS-rotate the pool, then trim what survives to the budget. Kept versions are
## renamed to '<name>_<stamp>_<role>', so a plain listing sorts chronologically and
## says what each one is being kept for.
function fRotate {
	$copies = @(fHeldCopies | Sort-Object Stamp)
	if (-not $copies) { fItem "-" "pool" "empty"; return }

	$roles  = fGfsRoles -Copies $copies
	$budget = fBudget -Copies $copies -Roles $roles

	$running = @(fRunningExePaths)
	$pruned  = 0

	foreach ($copy in $copies) {
		if ($budget.Contains($copy.Name)) { continue }
		if (fRemoveIfIdle -Copy $copy -Running $running) { $pruned++ }
	}

	## Rename after the prune, so a name freed up this run is available.
	foreach ($copy in $copies) {
		if (-not $budget.Contains($copy.Name)) { continue }
		$want = "${ProgramName}_$($copy.Stamp.ToString($StampFormat))_$($roles[$copy.Name])${ExeExt}"
		if ($copy.Name -eq $want) { continue }
		$wantPath = Join-Path $TargetDir $want
		if (Test-Path -LiteralPath $wantPath) { continue }
		try {
			Move-Item -LiteralPath $copy.Payload.FullName -Destination $wantPath -ErrorAction Stop
		} catch {
			## A running image refuses the rename on Windows; next run tries again.
			fItem "-" "kept" "under its old name (in use): $($copy.Name)"
		}
	}

	$kept = ($budget.Count)
	fItem "ok" "pool" ("{0} version(s) kept{1}" -f $kept, $(if ($pruned) { ", $pruned pruned" } else { "" }))
}


## Assign each version its GFS role, coarsest wins: first, year, month, week, day,
## hour, frequent, and 'latest' for the single newest. Returns a name -> role map; a
## version with no entry is one nothing is keeping. Same role names as the pipeline's
## gfs-rotate.bash, so both pools read alike.
function fGfsRoles {
	param([Parameter(Mandatory)][object[]]$Copies)

	$cur = fPeriodKeys (Get-Date)

	## Last version in each COMPLETED period. The still-open current period is
	## skipped, which is what makes the roles retrospective.
	$per = @{}
	foreach ($p in "hour", "day", "week", "month", "year") { $per[$p] = @{} }
	foreach ($copy in $Copies) {
		$keys = fPeriodKeys $copy.Stamp
		foreach ($p in "hour", "day", "week", "month", "year") {
			if ($keys[$p] -ne $cur[$p]) { $per[$p][$keys[$p]] = $copy }
		}
	}

	$roles = @{}
	$roles[$Copies[0].Name] = "first"

	foreach ($spec in @(
		@("year",  $KeepYearly),  @("month", $KeepMonthly), @("week", $KeepWeekly),
		@("day",   $KeepDaily),   @("hour",  $KeepHourly)
	)) {
		$role  = $spec[0]
		$keys  = @($per[$role].Keys | Sort-Object)
		$start = [Math]::Max(0, $keys.Count - $spec[1])
		for ($i = $start; $i -lt $keys.Count; $i++) {
			$name = $per[$role][$keys[$i]].Name
			if (-not $roles.ContainsKey($name)) { $roles[$name] = $role }
		}
	}

	$start = [Math]::Max(0, $Copies.Count - $KeepFrequent)
	for ($i = $start; $i -lt $Copies.Count; $i++) {
		if (-not $roles.ContainsKey($Copies[$i].Name)) { $roles[$Copies[$i].Name] = "frequent" }
	}

	## The newest is labelled 'latest' rather than by period - a stable, naturally
	## sorting pointer at the most recent build. Unless it is also the only one, in
	## which case 'first' already claimed it.
	$last = $Copies[$Copies.Count - 1].Name
	if ($roles[$last] -ne "first") { $roles[$last] = "latest" }

	return $roles
}


## The hour/day/week/month/year keys a timestamp falls in.
function fPeriodKeys {
	param([Parameter(Mandatory)][datetime]$When)
	return @{
		hour  = $When.ToString("yyyyMMddHH")
		day   = $When.ToString("yyyyMMdd")
		week  = "{0:D4}{1:D2}" -f [System.Globalization.ISOWeek]::GetYear($When),
		                          [System.Globalization.ISOWeek]::GetWeekOfYear($When)
		month = $When.ToString("yyyyMM")
		year  = $When.ToString("yyyy")
	}
}


## Trim the GFS-kept set to what the pool is allowed to hold: at most $MaxVersions,
## at least $MinVersions, and between those only as many as fit $MaxPoolBytes.
## Filled newest-first, because that is the end that gets run. The oldest version is
## seeded in first and so survives whatever the budget does to the rest.
## Returns a set of names to keep.
function fBudget {
	param(
		[Parameter(Mandatory)][object[]]$Copies,
		[Parameter(Mandatory)][hashtable]$Roles
	)

	$keep = [System.Collections.Generic.HashSet[string]]::new()
	$kept = @($Copies | Where-Object { $Roles.ContainsKey($_.Name) })
	if (-not $kept) { return ,$keep }

	$oldest = $kept[0]
	$keep.Add($oldest.Name) | Out-Null
	$bytes = fPayloadSize $oldest.Payload.FullName

	foreach ($copy in @($kept | Sort-Object Stamp -Descending)) {
		if ($keep.Contains($copy.Name)) { continue }
		if ($keep.Count -ge $MaxVersions) { break }
		$size = fPayloadSize $copy.Payload.FullName
		if ($keep.Count -ge $MinVersions -and ($bytes + $size) -gt $MaxPoolBytes) { break }
		$keep.Add($copy.Name) | Out-Null
		$bytes += $size
	}

	## Comma: a HashSet is enumerable, so a plain return unrolls it - and a one-version
	## pool then comes back as a bare string whose .Contains is a substring test.
	return ,$keep
}


## Bytes a version occupies: the file's own length, or everything under the prefix.
## Asked at most once per version per run, and only by the budget.
function fPayloadSize {
	param([Parameter(Mandatory)][string]$Path)
	if ($PayloadIsFile) {
		$item = Get-Item -LiteralPath $Path -ErrorAction SilentlyContinue
		if ($item) { return [int64]$item.Length }
		return [int64]0
	}
	$total = [int64]0
	Get-ChildItem -LiteralPath $Path -Recurse -File -Force -ErrorAction SilentlyContinue |
		ForEach-Object { $total += $_.Length }
	return $total
}


## Every stamped version in the pool, as { Payload, Name, Stamp, IdFile }. The role
## suffix is optional: a version copied in this run has not been tagged yet.
function fHeldCopies {
	$rx   = "^$([regex]::Escape($ProgramName))_(?<stamp>\d{8}-\d{6})(_[a-z]+)?$([regex]::Escape($ExeExt))$"
	$type = if ($PayloadIsFile) { @{ File = $true } } else { @{ Directory = $true } }
	Get-ChildItem -LiteralPath $TargetDir @type -Filter "${ProgramName}_*" -ErrorAction SilentlyContinue |
		ForEach-Object {
			if ($_.Name -match $rx) {
				[pscustomobject]@{
					Payload = $_
					Name    = $_.Name
					Stamp   = fParseStamp $Matches.stamp
					IdFile  = fIdFile $_.FullName
				}
			}
		}
}


## Newest held version, or $null.
function fNewestCopy {
	fHeldCopies | Sort-Object Stamp -Descending | Select-Object -First 1
}


## Parse a 'yyyyMMdd-HHmmss' stamp to a DateTime.
function fParseStamp {
	param([Parameter(Mandatory)][string]$Stamp)
	return [datetime]::ParseExact($Stamp, $StampFormat, [System.Globalization.CultureInfo]::InvariantCulture)
}


## Delete one version unless something is running out of it, or it is locked.
## Returns $true if deleted.
function fRemoveIfIdle {
	param(
		[Parameter(Mandatory)]$Copy,
		[string[]]$Running
	)
	if ($PayloadIsFile) {
		## Single exe: in use = a running process whose image IS this exact copy.
		$inUse = $Running | Where-Object { $_ -ieq $Copy.Payload.FullName }
	} else {
		## Prefix: in use = a running process whose image lives anywhere inside it.
		$prefix = $Copy.Payload.FullName + [System.IO.Path]::DirectorySeparatorChar
		$inUse = $Running | Where-Object { $_.StartsWith($prefix) }
	}
	if ($inUse) {
		fItem "-" "kept" "running: $($Copy.Name)"
		return $false
	}
	try {
		Remove-Item -LiteralPath $Copy.Payload.FullName -Recurse -Force -ErrorAction Stop
		return $true
	} catch {
		fItem "-" "kept" "locked: $($Copy.Name)"
		return $false
	}
}


## Full image paths of everything currently running (best-effort). Worked out once
## per run - every sweep asks the same question, and the answer is not cheap.
##
## On Windows the Path property throws for each of the few hundred protected system
## processes, and swallowing those exceptions costs whole seconds; one CIM query
## answers the same thing in a fraction of the time. Elsewhere the property is the
## cheap way round.
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


## Delete leftover partial copies (an interrupted run's '.partial'), unless fresh
## enough to be a concurrent run's copy in progress.
function fDeleteStalePartials {
	$cutoff = (Get-Date).AddHours(-1)
	Get-ChildItem -LiteralPath $TargetDir -Force -Filter "${ProgramName}_*.partial" -ErrorAction SilentlyContinue |
		Where-Object { $_.LastWriteTime -lt $cutoff } |
		ForEach-Object {
			try {
				Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction Stop
				fItem "ok" "cleaned" "stale partial copy: $($_.Name)"
			} catch { }
		}
}


## Retire the pool from before it was GFS-rotated: stamped copies loose in the
## install dir under their own prefix. Neither sweep above can see them, so they
## would sit there for good at a couple of hundred MB each.
function fRetireLegacyCopies {
	$rx      = "^$([regex]::Escape($LegacyPrefix))_\d{8}-\d{6}(_[a-z0-9]+)?(\.tmp)?$([regex]::Escape($ExeExt))?$"
	$running = @(fRunningExePaths)

	Get-ChildItem -LiteralPath $InstallDir -Force -Filter "$($LegacyPrefix)_*" -ErrorAction SilentlyContinue |
		Where-Object { $_.Name -match $rx } |
		ForEach-Object {
			## $_ is rebound by the Where-Object below, so hold the item first.
			$item   = $_
			$prefix = $item.FullName + [System.IO.Path]::DirectorySeparatorChar
			if ($running | Where-Object { $_ -ieq $item.FullName -or $_.StartsWith($prefix) }) {
				fItem "-" "kept" "running, old layout: $($item.Name)"
				return
			}
			try {
				Remove-Item -LiteralPath $item.FullName -Recurse -Force -ErrorAction Stop
				fItem "ok" "retired" "copy from the old layout: $($item.Name)"
			} catch {
				fItem "-" "kept" "locked, old layout: $($item.Name)"
			}
		}
}


## Point the fixed name at the newest version. Replaced rather than updated in place:
## repointing an existing symlink is not something every platform agrees on, and the
## launch reads the link fresh anyway.
function fUpdateLink {
	$newest = fNewestCopy
	if (-not $newest) { return }

	$target = fMainBin $newest.Payload.FullName
	$cur    = Get-Item -LiteralPath $LinkPath -Force -ErrorAction SilentlyContinue
	if ($cur -and $cur.Target -eq $target) { fItem "ok" "link" "$LinkPath -> $($newest.Name)"; return }

	try {
		if ($cur) { Remove-Item -LiteralPath $LinkPath -Force -ErrorAction Stop }
		New-Item -ItemType SymbolicLink -Path $LinkPath -Target $target -Force -ErrorAction Stop | Out-Null
		fItem "ok" "link" "$LinkPath -> $($newest.Name)"
	} catch {
		## Windows without the symlink privilege is the case this catches. A plain
		## copy still runs, it just costs the disk space - and only where a version
		## is one file; a prefix is left alone and the launch falls back to it.
		if (-not $PayloadIsFile) {
			fWarn -Gui "couldn't link $LinkPath ($($_.Exception.Message))"
			return
		}
		try {
			Copy-Item -LiteralPath $target -Destination $LinkPath -Force -ErrorAction Stop
			fItem "ok" "link" "$LinkPath copied from $($newest.Name) (no symlink privilege)"
		} catch {
			fWarn -Gui "couldn't update $LinkPath ($($_.Exception.Message))"
		}
	}
}


## The thing to run inside a version: the payload itself on Windows, the prefix's
## own wrapper everywhere else - it resolves its own location through the symlink and
## sets up the runtime environment, so nothing here has to.
function fMainBin {
	param([Parameter(Mandatory)][string]$PayloadPath)
	if ($PayloadMainBin) { return (Join-Path $PayloadPath $PayloadMainBin) }
	return $PayloadPath
}


## What to actually run: the fixed name when it is there, else the newest version
## directly, else nothing.
function fRunTarget {
	if (Test-Path -LiteralPath $LinkPath) { return $LinkPath }
	$newest = fNewestCopy
	if ($newest) { return (fMainBin $newest.Payload.FullName) }
	return $null
}


## Give the desktop a menu entry, so the program shows its own icon in the menu and
## the switcher instead of a generic one. Exec is this launcher, not the app: a menu
## click should pick up a new build the same way a shell launch does. The icon comes
## from the newest version, so it is rewritten every run. Linux only - Windows takes
## its icon out of the exe. Never fatal: a missing entry costs an icon.
function fRegisterDesktopEntry {
	$newest = fNewestCopy
	if (-not $newest) { return }

	$dataHome = if ($env:XDG_DATA_HOME) { $env:XDG_DATA_HOME } else { Join-Path $HOME ".local/share" }
	$appsDir  = Join-Path $dataHome "applications"
	$icon     = Join-Path $newest.Payload.FullName "share/icons/hicolor/256x256/apps/${ProgramName}.png"
	$exec     = fWrapperPath
	## The wrapper is directly executable; this script itself needs pwsh in front of it.
	$execLine = if ($exec -like "*.ps1") { "Exec=pwsh -NoProfile -File `"$exec`" %U" }
	            else                     { "Exec=`"$exec`" %U" }

	try {
		if (-not (Test-Path -LiteralPath $icon)) { return }
		if (-not (Test-Path -LiteralPath $appsDir)) {
			New-Item -ItemType Directory -Path $appsDir -Force | Out-Null
		}
		## Written from scratch rather than copied out of the prefix: that file is
		## four hundred lines of translations and every Exec in it is unqualified.
		$entry = @(
			"[Desktop Entry]"
			"Type=Application"
			"Name=Nemo Anywhere (dogfood)"
			"Comment=Access and organize files"
			$execLine
			"Icon=$icon"
			"Terminal=false"
			"StartupNotify=false"
			"StartupWMClass=${ProgramName}"
			"Categories=GTK;Utility;Core;FileTools;"
			"MimeType=inode/directory;"
			"Keywords=folders;filesystem;explorer;"
		) -join "`n"
		Set-Content -LiteralPath (Join-Path $appsDir "${ProgramName}-dogfood.desktop") -Value $entry -Encoding utf8NoBOM
		$update = fFindOnPath "update-desktop-database"
		if ($update) { & $update $appsDir 2>$null | Out-Null }
	} catch {
		fItem "-" "menu" "could not register the entry: $($_.Exception.Message)"
	}
}


## The shell wrapper a desktop entry should run. The wrapper tells us where it is;
## failing that, whatever is on PATH; failing that, this script, which at least works
## for anyone who has pwsh associated.
function fWrapperPath {
	if ($env:N8RUNFM_WRAPPER -and (Test-Path -LiteralPath $env:N8RUNFM_WRAPPER)) {
		return $env:N8RUNFM_WRAPPER
	}
	$onPath = fFindOnPath "runfm"
	if ($onPath) { return $onPath }
	return $PSCommandPath
}


## Launch the app detached, so the launcher exits while the app runs on.
function fLaunchApp {
	param(
		[Parameter(Mandatory)][string]$Exe,
		[string[]]$PassArgs
	)

	if (-not (Test-Path -LiteralPath $Exe)) { fFail "nothing to run at $Exe" }
	if (-not $IsWindows) { fRegisterDesktopEntry }
	return fStartApp -Exe $Exe -ArgList $PassArgs
}


## Fall back to whatever file manager is installed, in $FallbackManagers order.
function fLaunchFallback {
	param([string[]]$PassArgs)

	foreach ($cand in $FallbackManagers) {
		$path = fFindOnPath $cand
		if (-not $path) { continue }
		fItem "-" "fallback" "${cand}: $path"
		return fStartApp -Exe $path -ArgList $PassArgs
	}

	fFail ("no file manager available (no ${ProgramName} build, and none of " +
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


## Launch detached and return the Process. The Process lets a test harness stop this
## exact instance by PID; matching on name risks hitting a copy someone else started.
##
## Windows needs nothing extra: with no redirections Start-Process goes through
## ShellExecute, which already detaches.
##
## Unix would hand the app our own stdout, holding the caller's pipe open for the
## app's whole life. Start-Process -RedirectStandard* is worse - it pumps through a
## pipe owned by THIS process, so the app's output is dropped once we exit. Do it in
## the shell instead: setsid and sh both exec in place, so the app owns real fds, sits
## in a fresh session out of reach of a terminal hangup, and keeps the reported PID.
## The log path rides an env var to keep quotes off the command line.
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
			$sp.FilePath     = "/bin/sh"
			$sp.ArgumentList = $shArgs
		}
	}

	## Start-Process joins ArgumentList into one command line with a naive space join
	## and no quoting, then the target re-splits it (.NET on unix, the MSVCRT parser
	## on Windows). Quote every element so args with spaces, quotes or trailing
	## backslashes survive that round trip.
	## ContainsKey, not $sp.ArgumentList: under Set-StrictMode -Version Latest a
	## hashtable member that was never set throws rather than answering $null, so the
	## plain read blew up every launch that passed no arguments.
	if ($sp.ContainsKey("ArgumentList")) {
		$sp.ArgumentList = @($sp.ArgumentList | ForEach-Object { fQuoteArg $_ })
	}

	## RunAs is a ShellExecute verb, so Windows only - and only reached when the whole
	## launcher is already elevated (the entry point self-elevates first), so this
	## raises no second consent prompt.
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


## Quote one argument so it survives Start-Process joining ArgumentList into a single
## command line and the target re-splitting it. MSVCRT/CommandLineToArgvW rules: only
## quote when needed; double the backslashes that precede a quote or end the arg;
## escape embedded quotes.
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


## Start a step. Everything a step decides prints under it as an fItem row, so a run
## reads as a short report rather than a stream of loose notes.
function fStep {
	param([string]$Msg)
	fLog "-- $Msg"
	$script:StepRows = 0
	Write-Host ""
	Write-Host "  $Msg" -ForegroundColor Cyan
}


## One row under a step: a status tag, an optional label column, then free text. Only
## the tag is coloured - a whole coloured line is a wall of green.
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


## Byte count for a human, one decimal.
function fHumanSize {
	param([Parameter(Mandatory)][int64]$Bytes)
	if ($Bytes -ge 1GB) { return ("{0:n1} GB" -f ($Bytes / 1GB)) }
	if ($Bytes -ge 1MB) { return ("{0:n1} MB" -f ($Bytes / 1MB)) }
	if ($Bytes -ge 1KB) { return ("{0:n1} KB" -f ($Bytes / 1KB)) }
	return "$Bytes B"
}


## Non-fatal problem (and the run log). Pass -Gui to also surface it in the
## end-of-run dialog (the shortcut case, where the console flashes shut) - reserved
## for real problems (a failed copy), not benign skips (a source that isn't there).
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


## Append a timestamped line to the run log. Best-effort: logging must never be the
## thing that stops a launch.
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


## Remove any mark-of-the-web this script picked up from the sync layer. An unsigned
## script carrying MOTW is refused under a RemoteSigned policy, which silently kills a
## shortcut click - the body never runs, so nothing copies and nothing logs. Only
## helps the NEXT run; this one already got past the policy.
function fSelfHealMotw {
	try {
		$zone = Get-Content -LiteralPath $PSCommandPath -Stream Zone.Identifier -ErrorAction SilentlyContinue
		if ($zone) {
			Unblock-File -LiteralPath $PSCommandPath -ErrorAction Stop
			fItem "ok" "cleaned" "cleared mark-of-the-web on this script"
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
##   --no-admin   run without elevating. On Windows elevation is on by default: the
##                whole launcher self-elevates below, so the copy, the symlink and
##                the launched app all get admin rights. '--admin' is still accepted,
##                and is the only way to ask for it on unix - where it is refused.
##   --no-update  skip the copy, rotate and relink; just run what is already held.
##   --gui        force the end-of-run / failure dialog on (auto-on for a shortcut).
## Single-dash spellings are accepted too: '-admin' is what a PowerShell user types,
## and it collides with nothing in the app's own option set.
$wantAdmin = $IsWindows
$noUpdate  = $false
$forceGui  = $false
$passArgs  = @()
foreach ($arg in $args) {
	switch -Regex ($arg) {
		'^--?admin$'     { $wantAdmin = $true;  continue }
		'^--?no-admin$'  { $wantAdmin = $false; continue }
		'^--?no-update$' { $noUpdate  = $true;  continue }
		'^--?gui$'       { $forceGui  = $true;  continue }
		default          { $passArgs += $arg }
	}
}

$script:GuiFeedback = $forceGui -or (fLaunchedFromShortcut)

## Refuse a flag the app doesn't know before anything else happens - ahead of the UAC
## prompt in particular, so a typo can't cost a consent click and a copy first.
fCheckPassArgs -PassArgs $passArgs

if ($wantAdmin -and -not $IsWindows) {
	fWarn "--admin is Windows-only; ignoring (running a file manager as root is a footgun)"
	$wantAdmin = $false
}

## Self-elevate: unless '--no-admin', and not already elevated, relaunch the whole
## script elevated and hand off. Making the symlink needs a privilege a filtered token
## does not carry, so an unelevated run could not repoint the fixed name and would go
## on launching whatever it already had. The relaunch carries the original args plus
## '--gui' (its parent is the UAC broker, not Explorer, so it can't re-detect the
## shortcut). A declined consent is not fatal: fall through and run unelevated, with a
## dialog saying the build may be stale.
if ($wantAdmin -and -not (fIsElevated)) {
	$self = (Get-Process -Id $PID).Path      # the pwsh.exe hosting this script
	$fwd  = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $PSCommandPath) + $args + "--gui"
	$fwd  = @($fwd | ForEach-Object { fQuoteArg $_ })
	try {
		Start-Process -FilePath $self -Verb RunAs -ArgumentList $fwd -WindowStyle Minimized -ErrorAction Stop | Out-Null
		exit 0
	} catch {
		fWarn "elevation declined; running without admin (a newer build may not be linked in)"
		if ($script:GuiFeedback) {
			fGuiShow -Icon Warning -Title "Nemo Anywhere dogfood - not elevated" -Msg (
				"Administrator access was declined.`n`nRunning without it - a newer " +
				"build may not be linked in, so an older one could launch.")
		}
	}
}

## Elevated (self- or from an elevated shell): also launch the app elevated.
if ($wantAdmin) { $RunAsAdmin = $true }

fMain -PassArgs $passArgs -NoUpdate:$noUpdate

## Surface any real problems (a failed copy etc.) for the shortcut case.
if ($script:GuiFeedback -and $script:RunWarnings.Count) {
	fGuiShow -Icon Warning -Title "Nemo Anywhere dogfood" -Msg (
		"Launched, but with issues:`n`n - " + ($script:RunWarnings -join "`n - "))
}

exit 0


##	History:
##		- 2026-09-07: Source is now the synced dogfood dir for the running platform and
##		  nothing else - the repo build and the b23 share are gone, along with the
##		  probe-every-source machinery and its network timeout. The pool moved into
##		  '<name>_versions' beside a symlink at the fixed name, and is GFS-rotated on
##		  every run (hour/day/week/month/year plus the most recent few and the very
##		  first build) under a 10/5/1GB budget, replacing the flat seven-day sweep. A
##		  build already held is recognised by its bytes rather than its date, so a
##		  restamp by the sync layer no longer costs a re-copy. macOS joins Linux and
##		  Windows. '--no-update' runs what is held without touching the pool. The
##		  desktop entry now runs this launcher rather than a dated copy of the app.
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
