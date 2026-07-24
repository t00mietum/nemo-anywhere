#!/usr/bin/env pwsh

##	Purpose:
##		- Cross-platform (PowerShell 7) dogfood launcher for Nemo Anywhere, same
##		  concept as silkterm's n8runterm: keep a small pool of date-stamped
##		  release copies in a local target dir and launch the newest, passing
##		  through any arguments. Independent of the cicd pipeline.
##		- Nemo Anywhere is a prefix (a whole directory tree), not a single exe, so
##		  each copy is a dir 'nemofmdf_<YYYYMMDD-HHMMSS>_<tag>' where the stamp is
##		  the source build's main-binary mtime - a given build is copied once, and
##		  a running copy never blocks the copy.
##		- Each run, in order: delete idle copies over 7 days old (in use = any
##		  running process whose image lives inside the copy); refresh from the
##		  source if its build is newer than what we hold; launch the newest.
##		- Sources per OS (tag in the copy name):
##			lin  the synced dogfood prefix nemo-anywhere.app (Linux)
##			win  the staged win-run snapshot from the cross build (Windows)
##		- The launcher wires the runtime env itself (loader path, schemas, data
##		  dirs), pointing everything at the stamped copy, then starts it detached
##		  and exits - on unix the app's own output goes to a log in the target
##		  dir, since it no longer has the caller's console.
##		- If no copy is held and the source is unreachable, falls back to the
##		  first installed known file manager.
##	History: At bottom of script.


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Configuration

## Source per OS. The 'main binary' relative path doubles as the reachability
## probe and the build-stamp source (its mtime). Candidates are tried in order;
## if none exist the first is kept so the copy step warn-skips it like any
## other unreachable source (held copies still run).
if ($IsWindows) {
	$SourceCandidates = @(
		"C:\0-0\users\collierjr\data\prs\dev\github.com\t00mietum\nemo-anywhere\github\cicd\artifacts\win-run"
		## Add the SMB path to the build host's snapshot here when it's shared, e.g.:
		## "\\b23\home-collierjr\...\t00mietum\nemo-anywhere\github\cicd\artifacts\win-run"
	)
	$SourceMainBin = "app\nemo-anywhere.exe"
	$SourceTag     = "win"
	$TargetDir     = Join-Path $env:LOCALAPPDATA "nemo-anywhere-dogfood"
} else {
	$SourceCandidates = @(
		(Join-Path $HOME ".synced/Dropbox/0-0/common/exec/util/linux/nemo-anywhere.app")
		(Join-Path $HOME "synced/0-0/common/exec/util/linux/nemo-anywhere.app")
	)
	$SourceMainBin = "bin/nemo-anywhere"
	$SourceTag     = "lin"
	$TargetDir     = Join-Path $HOME ".local/share/nemo-anywhere-dogfood"
}
$SourceDir = $SourceCandidates | Where-Object { Test-Path -LiteralPath (Join-Path $_ $SourceMainBin) } | Select-Object -First 1
if (-not $SourceDir) { $SourceDir = $SourceCandidates[0] }

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

	## 1. Sweep stale partial copies and stale idle copies.
	fDeleteStaleTmp
	fDeleteOldBuilds

	## 2. Refresh from the source if it has a newer build than we hold.
	fCopyIfNewer

	## 3. Launch the newest copy. The Process goes nowhere - it's there for a
	##    test harness, and letting it reach the output stream would dump a
	##    process table on the way out.
	$copy = fNewestCopy
	if ($copy) {
		fNote "running: $($copy.File.Name)"
		$null = fLaunchNemo -CopyDir $copy.File.FullName -PassArgs $PassArgs
		return
	}

	## 4. Nothing held and no source reachable - fall back to any file manager.
	fWarn "no dogfood copy held and source not reachable; trying fallbacks"
	$null = fLaunchFallback -PassArgs $PassArgs
}


## Copy the source prefix in as '<prefix>_<stamp>_<tag>' when its build is newer
## than the newest copy we hold. Copies to a .tmp name then renames, so an
## interrupted copy can never pass for a complete one. No-op if the source is
## unreachable or we're already current.
function fCopyIfNewer {
	$srcBin = Join-Path $SourceDir $SourceMainBin
	if (-not (Test-Path -LiteralPath $srcBin)) {
		fWarn "source not reachable: $srcBin"
		return
	}

	$stamp     = (Get-Item -LiteralPath $srcBin).LastWriteTime.ToString($StampFormat)
	$stampTime = fParseStamp $stamp
	$held      = fNewestCopy

	if ($held -and $held.Stamp -ge $stampTime) {
		fNote "already current (held $($held.Stamp.ToString($StampFormat)), src $stamp)"
		return
	}

	$dst = Join-Path $TargetDir "${DogfoodPrefix}_${stamp}_${SourceTag}"
	if (Test-Path -LiteralPath $dst) {
		fNote "copy already present: $(Split-Path $dst -Leaf)"
		return
	}

	$tmp = "$dst.tmp"
	try {
		if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
		Copy-Item -LiteralPath $SourceDir -Destination $tmp -Recurse -Force -ErrorAction Stop
		Rename-Item -LiteralPath $tmp -NewName (Split-Path $dst -Leaf) -ErrorAction Stop
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
function fDeleteOldBuilds {
	## Any tag ages out here (incl. one-off hand-dropped tags).
	$rx      = "^$([regex]::Escape($DogfoodPrefix))_\d{8}-\d{6}(_[a-z0-9]+)?$"
	$cutoff  = (Get-Date).AddDays(-$MaxAgeDays)
	$running = @(fRunningExePaths)
	$deleted = 0

	Get-ChildItem -LiteralPath $TargetDir -Directory -Filter "${DogfoodPrefix}_*" -ErrorAction SilentlyContinue |
		Where-Object { $_.Name -match $rx } |
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
	Get-ChildItem -LiteralPath $TargetDir -Directory -Filter "${DogfoodPrefix}_*.tmp" -ErrorAction SilentlyContinue |
		Where-Object { $_.LastWriteTime -lt $cutoff } |
		ForEach-Object {
			try {
				Remove-Item -LiteralPath $_.FullName -Recurse -Force -ErrorAction Stop
				fNote "deleted stale partial copy: $($_.Name)"
			} catch { }
		}
}


## All stamped copies as objects { File, Name, Tag, Stamp(DateTime) }, current
## OS's tag only - a lin prefix can't run on Windows or vice versa.
function fHeldCopies {
	$rx = "^$([regex]::Escape($DogfoodPrefix))_(?<stamp>\d{8}-\d{6})_$([regex]::Escape($SourceTag))$"
	Get-ChildItem -LiteralPath $TargetDir -Directory -Filter "${DogfoodPrefix}_*" -ErrorAction SilentlyContinue |
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
	if ($DirInfo.Name -match "_(?<stamp>\d{8}-\d{6})(?:_[a-z0-9]+)?$") {
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
	$prefix = $DirInfo.FullName + [System.IO.Path]::DirectorySeparatorChar
	if ($Running | Where-Object { $_.StartsWith($prefix) }) {
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
## can't read are skipped.
function fRunningExePaths {
	Get-Process -ErrorAction SilentlyContinue |
		ForEach-Object { try { $_.Path } catch { $null } } |
		Where-Object { $_ }
}


## Launch a stamped copy detached, wiring the runtime env at the copy the same
## way the fixed dogfood wrapper (Linux) / wine runner (Windows) do. The env
## edits ride process inheritance; this launcher exits right after, so nothing
## else sees them.
function fLaunchNemo {
	param(
		[Parameter(Mandatory)][string]$CopyDir,
		[string[]]$PassArgs
	)

	if ($IsWindows) {
		## Exe in app\, GTK runtime in mingw64\ (DLL-relative lookups intact).
		$exe = Join-Path $CopyDir "app\nemo-anywhere.exe"
		$env:PATH = (Join-Path $CopyDir "mingw64\bin") + ";" + (Join-Path $CopyDir "app") + ";" + $env:PATH
		$env:GSETTINGS_SCHEMA_DIR = (Join-Path $CopyDir "mingw64\share\glib-2.0\schemas") +
			$(if ($env:GSETTINGS_SCHEMA_DIR) { ";" + $env:GSETTINGS_SCHEMA_DIR } else { "" })
		$env:XDG_DATA_DIRS = (Join-Path $CopyDir "mingw64\share") +
			$(if ($env:XDG_DATA_DIRS) { ";" + $env:XDG_DATA_DIRS } else { "" })
	} else {
		$exe = Join-Path $CopyDir "bin/nemo-anywhere"
		$env:LD_LIBRARY_PATH = (Join-Path $CopyDir "lib/x86_64-linux-gnu") +
			$(if ($env:LD_LIBRARY_PATH) { ":" + $env:LD_LIBRARY_PATH } else { "" })
		$env:GSETTINGS_SCHEMA_DIR = (Join-Path $CopyDir "share/glib-2.0/schemas") +
			$(if ($env:GSETTINGS_SCHEMA_DIR) { ":" + $env:GSETTINGS_SCHEMA_DIR } else { "" })
		$env:XDG_DATA_DIRS = (Join-Path $CopyDir "share") + ":" +
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
		$shArgs = @("-c", '"exec \"$0\" \"$@\" </dev/null >>\"$N8RUNFM_APPLOG\" 2>&1"', (fQuoteArg $Exe))
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

	try {
		$proc = Start-Process @sp
	} catch {
		fFail "launch failed for $Exe ($($_.Exception.Message))"
	}

	fNote "launched pid $($proc.Id): $([System.IO.Path]::GetFileName($Exe))"
	return $proc
}


## Wrap an argument in double quotes if it contains whitespace, so Start-Process
## passes it as a single argv entry.
function fQuoteArg {
	param([string]$Arg)
	if ($Arg -match '\s') { return '"' + $Arg + '"' }
	return $Arg
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
##		- 2026-07-23 JC: Launch detached - own session, stdio off the caller - so
##		  the launcher returns at once and the app outlives it.
##		- 2026-07-23 JC: Created (nemo-anywhere analog of silkterm's n8runterm.ps1;
##		  prefix-dir copies instead of a single exe).
