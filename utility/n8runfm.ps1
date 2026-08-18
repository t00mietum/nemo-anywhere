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
##		  dir); refresh from the source if its build is newer than what we hold;
##		  launch the newest. A source on a network share only gets a moment to
##		  answer - an unreachable one must not hold up a launch a held copy can serve.
##		- Sources per OS (tag in the copy name):
##			lin  the synced dogfood prefix nemo-anywhere.app (Linux)
##			win  the packed single exe from the native build (Windows)
##		- On Linux the launcher wires the runtime env itself (loader path, schemas,
##		  data dirs) at the stamped copy; on Windows the packed exe carries its whole
##		  runtime, so nothing is wired. Either way it starts the app detached and
##		  exits - on unix the app's own output goes to a log in the target dir, since
##		  it no longer has the caller's console.
##		- If no copy is held and the source is unreachable, falls back to the
##		  first installed known file manager.
##	History: At bottom of script.


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Configuration

## Source per OS. The 'main binary' relative path doubles as the reachability
## probe and the build-stamp source (its mtime). Candidates are tried in order;
## if none exist the first is kept so the copy step warn-skips it like any
## other unreachable source (held copies still run).

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT
if ($IsWindows) {
	## Single self-contained exe now (extension + whole GTK runtime packed in), so a
	## copy is one file, not a prefix tree - the same shape as silkterm's n8runterm.
	## The freshest source is the local build's packed exe (cicd-win.ps1 produces it);
	## the synced by-self drop is the fallback for a box without the repo. Clone root
	## differs per host, so try candidates in order and take the first that exists.
	$SourceCandidates = @(
		"C:\opt\0-0\users\collierjr\data\prs\dev\github.com\t00mietum\nemo-anywhere\github\cicd\artifacts\win-portable\nemo-anywhere.exe"
		"C:\0-0\users\collierjr\data\prs\dev\github.com\t00mietum\nemo-anywhere\github\cicd\artifacts\win-portable\nemo-anywhere.exe"
		"C:\opt\0-0\common\exec\synced\util\mswin\gui\by-self\win64\nemo-anywhere.exe"
		## Add the SMB path to a build host's packed exe here when it's shared, e.g.:
		## "\\b23\...\t00mietum\nemo-anywhere\github\cicd\artifacts\win-portable\nemo-anywhere.exe"
	)
	$SourceMainBin = ""            # the source IS the exe (single file, no sub-path)
	$SourceTag     = "win"
	$CopyIsFile    = $true         # a held copy is one .exe file, not a dir tree
	$CopyExt       = ".exe"
	$TargetDir     = Join-Path $env:LOCALAPPDATA "nemo-anywhere-dogfood"
} else {
	$SourceCandidates = @(
		(Join-Path $HOME ".synced/Dropbox/0-0/common/exec/util/linux/nemo-anywhere.app")
		(Join-Path $HOME "synced/0-0/common/exec/util/linux/nemo-anywhere.app")
	)
	$SourceMainBin = "bin/nemo-anywhere"
	$SourceTag     = "lin"
	$CopyIsFile    = $false        # a held copy is the whole prefix dir
	$CopyExt       = ""
	$TargetDir     = Join-Path $HOME ".local/share/nemo-anywhere-dogfood"
}
## Resolved by fResolveSource at copy time - the probe it uses is defined further
## down, and a function isn't callable before its definition runs.
$SourceDir = $null

## How long a candidate on a network share gets to answer the does-it-exist probe.
## An unreachable share otherwise wedges Test-Path for the SMB stack's own timeout,
## tens of seconds of nothing while a held copy sits ready to launch.
$ProbeTimeoutMs = 1500

## Get-ChildItem item-type for the pool: files on Windows (single exe), dirs on Linux.
$CopyGciType = if ($CopyIsFile) { @{ File = $true } } else { @{ Directory = $true } }

## Prefix for the date-stamped copy dirs.
$DogfoodPrefix = "nemofmdf"

## Delete idle stamped copies older than this many days.
$MaxAgeDays = 7

## Fallback file managers, tried in order when no copy is held and the source is
## unreachable. Launched plainly (generic managers accept a path arg at most).
$FallbackManagers = if ($IsWindows) {
	@("explorer.exe")
} else {
	@("nemo", "nautilus", "pcmanfm", "thunar", "dolphin")
}

## Per-run decision log, kept in the target dir, so a closed console can't lose
## the copy/skip reasons behind a launch.
$RunLog = Join-Path $TargetDir "n8runfm.log"

## Unix only: where the detached app's own output goes (GTK/GLib gripes and any
## crash message), since it no longer has the caller's console. Appended to and
## trimmed like the run log.
$AppLog = Join-Path $TargetDir "n8runfm-app.log"

## Stamp format shared by the copy name and every date comparison below.
$StampFormat = "yyyyMMdd-HHmmss"

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
	fLog ("=== run: PS {0}, script {1} ===" -f $PSVersionTable.PSVersion, $PSCommandPath)

	## 0. Windows only: strip a synced-on mark-of-the-web so a later click can't
	##    be policy-blocked.
	if ($IsWindows) { fSelfHealMotw }

	## 1. Sweep stale partial copies, stale idle copies, and anything left by the
	##    old layout.
	fDeleteStaleTmp
	fDeleteOldBuilds
	fRetireLegacyCopies

	## 2. Refresh from the source if it has a newer build than we hold.
	fCopyIfNewer

	## 3. Launch the newest copy. The Process goes nowhere - it's there for a
	##    test harness, and letting it reach the output stream would dump a
	##    process table on the way out.
	$copy = fNewestCopy
	if ($copy) {
		fNote "running: $($copy.File.Name)"
		$null = fLaunchNemo -CopyPath $copy.File.FullName -PassArgs $PassArgs
		return
	}

	## 4. Nothing held and no source reachable - fall back to any file manager.
	fWarn "no dogfood copy held and source not reachable; trying fallbacks"
	$null = fLaunchFallback -PassArgs $PassArgs
}


## What to look at to decide a source candidate is really there: its main binary
## on Linux, or the candidate itself on Windows, where the source IS the exe. Its
## mtime is also the build stamp.
function fSourceProbePath {
	param([Parameter(Mandatory)][string]$Candidate)
	if ($SourceMainBin) { return (Join-Path $Candidate $SourceMainBin) }
	return $Candidate
}


## Pick the first candidate that's actually there. Keeps the first as a placeholder
## and returns false if none are, so the copy step warn-skips like any other
## unreachable source - held copies still run.
function fResolveSource {
	foreach ($cand in $SourceCandidates) {
		if (fPathExists (fSourceProbePath $cand)) {
			$script:SourceDir = $cand
			return $true
		}
	}
	$script:SourceDir = $SourceCandidates[0]
	return $false
}


## Does a path exist? Local paths answer from the filesystem straight away, so
## they go through Test-Path as-is. A path on a network share gets a short leash
## instead: a share that is down wedges Test-Path until the SMB stack gives up on
## its own schedule, and a launcher with a perfectly good local copy in hand has
## no business making the user wait that long. Past the deadline the probe is
## abandoned - its thread unwinds whenever SMB is done with it - and the candidate
## is treated as absent.
function fPathExists {
	param([Parameter(Mandatory)][string]$Path)

	if (-not (fIsRemotePath $Path)) { return [bool](Test-Path -LiteralPath $Path) }

	$probe = [powershell]::Create()
	$null  = $probe.AddScript('param($p) [bool](Test-Path -LiteralPath $p)').AddArgument($Path)
	$async = $probe.BeginInvoke()

	if ($async.AsyncWaitHandle.WaitOne($ProbeTimeoutMs)) {
		$found = $false
		try { $found = [bool]($probe.EndInvoke($async) | Select-Object -First 1) } catch { }
		$probe.Dispose()
		return $found
	}

	## Dispose would block on the wedged probe, so hand it off and walk away.
	$null = $probe.BeginStop($null, $null)
	fNote "gave up on network source after ${ProbeTimeoutMs}ms: $Path"
	return $false
}


## True for a path served over the network - a UNC name, or a drive letter mapped
## to a share. Everything else is local and needs no protection.
function fIsRemotePath {
	param([Parameter(Mandatory)][string]$Path)

	if ($Path -match '^(\\\\|//)') { return $true }
	if ($IsWindows -and $Path -match '^([A-Za-z]):') {
		try {
			$drive = [System.IO.DriveInfo]::new($Matches[1] + ":\")
			return ($drive.DriveType -eq [System.IO.DriveType]::Network)
		} catch { return $false }
	}
	return $false
}


## Copy the source prefix in as '<prefix>_<stamp>_<tag>' when its build is newer
## than the newest copy we hold. Copies to a .tmp name then renames, so an
## interrupted copy can never pass for a complete one. No-op if the source is
## unreachable or we're already current.
function fCopyIfNewer {
	if (-not (fResolveSource)) {
		fWarn "source not reachable: $(fSourceProbePath $SourceDir)"
		return
	}
	$srcBin = fSourceProbePath $SourceDir

	$stamp     = (Get-Item -LiteralPath $srcBin).LastWriteTime.ToString($StampFormat)
	$stampTime = fParseStamp $stamp
	$held      = fNewestCopy

	if ($held -and $held.Stamp -ge $stampTime) {
		fNote "already current (held $($held.Stamp.ToString($StampFormat)), src $stamp)"
		return
	}

	$dst = Join-Path $TargetDir "${DogfoodPrefix}_${stamp}_${SourceTag}${CopyExt}"
	if (Test-Path -LiteralPath $dst) {
		fNote "copy already present: $(Split-Path $dst -Leaf)"
		return
	}

	$tmp = "$dst.tmp"
	try {
		if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
		if ($CopyIsFile) { Copy-Item -LiteralPath $SourceDir -Destination $tmp -Force -ErrorAction Stop }
		else             { Copy-Item -LiteralPath $SourceDir -Destination $tmp -Recurse -Force -ErrorAction Stop }
		Rename-Item -LiteralPath $tmp -NewName (Split-Path $dst -Leaf) -ErrorAction Stop
		## A synced-sourced exe can carry a mark-of-the-web; clear it so the launch
		## isn't SmartScreen-blocked. Best-effort, Windows-only (no-op for a dir).
		if ($CopyIsFile) { try { Unblock-File -LiteralPath $dst -ErrorAction SilentlyContinue } catch { } }
		fNote "copied -> $(Split-Path $dst -Leaf)"
	} catch {
		fWarn "couldn't copy build ($($_.Exception.Message))"
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
	return fStartApp -Exe $exe -ArgList @($PassArgs | ForEach-Object { fQuoteArg $_ })
}


## Fall back to whatever file manager is installed, in $FallbackManagers order.
function fLaunchFallback {
	param([string[]]$PassArgs)

	foreach ($cand in $FallbackManagers) {
		$path = fFindOnPath $cand
		if (-not $path) { continue }
		fNote "falling back to ${cand}: $path"
		return fStartApp -Exe $path -ArgList @($PassArgs | ForEach-Object { fQuoteArg $_ })
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
	if ($sp.ArgumentList) {
		$sp.ArgumentList = @($sp.ArgumentList | ForEach-Object { fQuoteArg $_ })
	}

	try {
		$proc = Start-Process @sp
	} catch {
		fFail "launch failed for $Exe ($($_.Exception.Message))"
	}

	fNote "launched pid $($proc.Id): $([System.IO.Path]::GetFileName($Exe))"
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


## Informational note to the host (and the run log).
function fNote { param([string]$Msg); fLog $Msg; Write-Host "n8runfm: $Msg" }

## Non-fatal note to stderr (and the run log).
function fWarn {
	param([string]$Msg)
	fLog "WARN: $Msg"
	Write-Warning "n8runfm: $Msg"
}

## Fatal error to stderr (and the run log), then stop.
function fFail {
	param([string]$Msg)
	fLog "FAIL: $Msg"
	Write-Error "n8runfm: $Msg"
	exit 1
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

fMain -PassArgs $args
exit 0


##	History:
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
