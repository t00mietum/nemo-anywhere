##	Purpose:
##		- Windows-side CI/CD pipeline for Nemo Anywhere. A PowerShell companion to
##		  the Linux cicd.bash, doing as much of the same work as Windows allows -
##		  including the parts cicd.bash farms out to helper scripts (the git
##		  sync/publish). Does NOT touch cicd.bash (that stays the Linux/cross
##		  pipeline) or config.bash.
##		- The reference build is C/GTK via meson/ninja INSIDE the nemo-build Linux
##		  container - there is no native-Windows build yet - so the build and smoke
##		  stages run in that container through Docker Desktop, exactly as cicd.bash
##		  drives them via cicd/utility/docker-run.bash. Only the git sync + publish
##		  run natively on the host.
##		- Stages (fail-fast; any error aborts before the next stage):
##		   0. remote sync    (fetch; fast-forward if safely behind; abort if diverged)
##		   1. format         (disabled - no C formatter gate yet; see config.bash)
##		   2. debug build    (meson setup + ninja, in the nemo-build container)
##		   3. tests          (headless --version smoke, in the container)
##		   4. release build  (disabled - no host-side release binary yet)
##		   5. packages       (disabled - depends on the release stage)
##		   6. dogfood        (disabled - depends on the release stage)
##		   7. publish        (stash -> pull -> add -> commit -> push, current branch)
##		- Container stages skip-with-warning (never hard-block a push) when Docker is
##		  absent, its daemon is down, or the nemo-build container is missing - the
##		  same policy as docker-run.bash. -DockerStrict turns that miss into a hard
##		  failure for a run that must not silently no-op.
##		- What Windows can't do (dropped vs cicd.bash): the profiler (Unix sampler +
##		  Xvfb), the headless harness / screenshots / demo (Xvfb), .deb/.rpm packages,
##		  and the rar version-archive step of publish (Linux publisher only).
##		- Stages still disabled in config.bash (release, packages, dogfood) show as
##		  disabled here too, with the same NEEDS shape; they light up on Windows once
##		  a host-side release binary exists.
##		- Syntax:
##		  pwsh cicd/cicd-win.ps1 [options]
##		  Options:
##		   -Yes            run unattended (no confirm / message prompt)
##		   -Quiet          quiet + unattended (implies -Yes); publish runs quiet too
##		   -Quick          skip the slow stages (reserved; none enabled yet)
##		   -Gate           merge gate only: format-check + lints + tests, then exit
##		   -NoSync         skip the remote sync check (stage 0)
##		   -NoFmt          skip the formatter stage (a no-op today; format is disabled)
##		   -NoBuild        skip the container build + smoke stages
##		   -NoPublish      skip the git publish stage
##		   -DockerStrict   a Docker/container miss aborts instead of skip-with-warning
##		   -Message MSG    publish hands-off with this commit message (no editor)
##		   -Help           show this help
##	History: At bottom of script.

##	Copyright © 2026 t00mietum (ID: f⍒Ê🝅ĜᛎỹqFẅ▿⍢Ŷ‡ʬẼᛏ🜣)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

[CmdletBinding()]
param(
	[switch]$Yes,
	[switch]$Quiet,
	[switch]$Quick,
	[switch]$Gate,
	[switch]$NoSync,
	[switch]$NoFmt,
	[switch]$NoBuild,
	[switch]$NoPublish,
	[switch]$DockerStrict,
	[string]$Message = "",
	[switch]$Help
)

## Requires PowerShell 7+ (pwsh): this uses $IsWindows and PS7 semantics. Windows
## PowerShell 5.1 has no $IsWindows, so the StrictMode guard below would throw a
## cryptic error instead. Bail early with a clear pointer.
if ($PSVersionTable.PSVersion.Major -lt 6) {
	Write-Error "cicd-win.ps1 needs PowerShell 7+ (pwsh); you're on Windows PowerShell $($PSVersionTable.PSVersion). Run: pwsh -File cicd/cicd-win.ps1"
	exit 1
}

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
## We drive native tools (git, docker) by hand and read $LASTEXITCODE - several
## probes (git diff --quiet, docker info) return non-zero ON PURPOSE. Keep a
## non-zero native exit from throwing so those reads work regardless of the shell.
$PSNativeCommandUseErrorActionPreference = $false

if ($Help) {
	## Print only the leading Purpose..History header block (mirrors cicd.bash's
	## `sed -n '/Purpose:/,/History:/p'`), not every top-level ## comment.
	$inBlock = $false
	foreach ($line in (Get-Content -LiteralPath $PSCommandPath)) {
		if ($line -match '^##\tPurpose:') { $inBlock = $true }
		if ($inBlock) {
			if ($line -match '^##\tHistory:') { break }
			$line -replace '^##\t?', ''
		}
	}
	exit 0
}

## Windows-only: this pipeline shells out to Docker Desktop and writes a host-side
## transcript. Refuse to run elsewhere (use cicd/cicd.bash on Linux).
if (-not $IsWindows) {
	Write-Error "cicd-win.ps1: this pipeline only runs on Windows (use cicd/cicd.bash on Linux)."
	exit 1
}

## -Quiet implies unattended; both suppress the preflight prompt.
$Unattended = ($Yes -or $Quiet)


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Configuration (Windows-relevant subset of config.bash; that file stays the
# Linux source of truth - keep the two in step when a live stage changes).

## Repo root = the parent of this script's cicd/ dir. Git runs here.
$Root    = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$AppName = "Nemo Anywhere"
$ExeName = "nemo-anywhere"

## The reference build container (image nemo-build-deps:latest). It mounts the repo
## root at /src and builds into /build. See design.md "Building".
$Container = "nemo-build"

## The single version source. meson.build carries `version : '6.6.4'` (colon form),
## NOT the cargo `version = "..."` the Linux engine's default collector greps for.
$VersionManifest = Join-Path $Root "source\meson.build"

## Full-run transcript (gitignored; a Windows-side sibling of the Linux lint logs,
## so the two lanes never clobber each other's output).
$LogDir = Join-Path $Root "cicd\artifacts\lint-win"

## Container shell commands - kept verbatim from config.bash's DEBUG_BUILD_CMD /
## TEST_CMD so the Windows lane builds and smokes exactly what Linux does.
$BuildCmd = 'if [ -f /build/build.ninja ]; then meson setup --reconfigure /build /src/source; else meson setup /build /src/source; fi && ninja -C /build'
$SmokeCmd = 'xvfb-run -a /build/src/nemo-anywhere --version'

## Cap parallelism to half the cores (display only for now; ninja auto-detects, as
## in config.bash). Kept for the preflight line and for when a -j is wired.
$Cores       = [Environment]::ProcessorCount
$CicdMaxJobs = [Math]::Max(1, [Math]::Floor($Cores / 2))


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Output helpers (mirror cicd.bash: fEcho / fEcho_Clean / fSection)

$script:WasLastEchoBlank = $false
$script:Letterbox = "•" * 73

function fEcho_Clean {
	param([string]$Msg = "")
	if ($Msg) { Write-Host $Msg; $script:WasLastEchoBlank = $false }
	elseif (-not $script:WasLastEchoBlank) { Write-Host ""; $script:WasLastEchoBlank = $true }
}
function fEcho     { param([string]$Msg = ""); if ($Msg) { fEcho_Clean "[ $Msg ]" } else { fEcho_Clean } }
function fSection  { param([string]$Msg);      fEcho_Clean; fEcho_Clean $script:Letterbox; fEcho $Msg }
function fNote     { param([string]$Msg); fEcho_Clean $Msg }
function fWarn     { param([string]$Msg); fEcho "WARNING: $Msg" }
function fDie      { param([string]$Msg); fEcho "FAILED: $Msg"; exit 1 }


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Functions

## Run a native command from the repo root; abort (fail-fast) on a non-zero exit.
function fExec {
	param(
		[Parameter(Mandatory)][string]$What,
		[Parameter(Mandatory)][string]$File,
		[string[]]$CmdArgs = @()
	)
	& $File @CmdArgs
	if ($LASTEXITCODE -ne 0) { fDie "$What failed (exit $LASTEXITCODE): $File $($CmdArgs -join ' ')" }
}

## First `version : '...'` from meson.build (the project() line). Display only -
## returns '?' rather than aborting, since no stage depends on it yet.
function fVersion {
	$m = Select-String -LiteralPath $VersionManifest -Pattern "version\s*:\s*'([^']+)'" | Select-Object -First 1
	if (-not $m) { return "?" }
	return $m.Matches[0].Groups[1].Value
}

## Best-effort docker daemon reachability with a hard timeout, so a stuck Docker
## Desktop can't hang the run. `docker info` errors fast when the daemon is down,
## but the timeout is the backstop.
function fDockerInfoUp {
	$job = Start-Job -ScriptBlock { docker info *> $null; $LASTEXITCODE }
	if (Wait-Job $job -Timeout 15) {
		$rc = Receive-Job $job
		Remove-Job $job -Force -ErrorAction SilentlyContinue
		return ($rc -eq 0)
	}
	Stop-Job $job -ErrorAction SilentlyContinue
	Remove-Job $job -Force -ErrorAction SilentlyContinue
	return $false
}

## Run a shell command in the nemo-build container. Windows port of
## cicd/utility/docker-run.bash: an environmental miss (no docker, daemon down,
## container absent) SKIPS with a warning so it can't hard-block a push; a genuine
## build/smoke failure aborts. -DockerStrict turns a miss into a hard failure. No
## systemctl nudge here - that's the Linux rootless-daemon path.
function fDockerRun {
	param(
		[Parameter(Mandatory)][string]$Label,
		[Parameter(Mandatory)][string]$Command
	)
	$why = $null
	if (-not (Get-Command docker -ErrorAction SilentlyContinue)) { $why = "docker not installed (Docker Desktop)" }
	elseif (-not (fDockerInfoUp)) { $why = "docker daemon not reachable (start Docker Desktop)" }
	else {
		$names = & docker ps -a --format '{{.Names}}' 2>$null
		if ($LASTEXITCODE -ne 0 -or ($names -notcontains $Container)) { $why = "build container '$Container' not found (create it per design.md)" }
	}
	if ($why) {
		if ($DockerStrict) { fDie "${Label}: $why" }
		fWarn "$Label SKIPPED: $why - not verified against the container"
		return
	}
	& docker start $Container *> $null
	## The container workdir is the mounted repo, so `ulimit -c 0` stops a crash
	## dropping a root-owned core.<pid> into the tree (unreadable to the host user,
	## and enough to abort the next backup). Matches docker-run.bash.
	& docker exec $Container sh -c "ulimit -c 0; $Command"
	if ($LASTEXITCODE -ne 0) { fDie "$Label failed (exit $LASTEXITCODE) in container '$Container'" }
	fEcho "OK: $Label (container)"
}

## Stage 0: make sure the local branch can be safely refreshed from its upstream
## BEFORE spending the build - what stage 7 pushes should be what got built and
## tested here, not an untested post-build merge. Behind-only is safe (fast-forward,
## stash-wrapped for a dirty tree); diverged aborts now rather than at publish.
## Offline just warns - a local build shouldn't need the net.
function fRemoteSync {
	& git rev-parse --abbrev-ref '@{u}' 2>$null | Out-Null
	if ($LASTEXITCODE -ne 0) {
		$branch = (& git rev-parse --abbrev-ref HEAD).Trim()
		fNote "no upstream for ${branch}; nothing to sync"
		return
	}
	& git fetch --quiet 2>$null
	if ($LASTEXITCODE -ne 0) { fWarn "git fetch failed (offline?); continuing with the local tree"; return }
	$ahead  = [int](& git rev-list --count '@{u}..HEAD')
	$behind = [int](& git rev-list --count 'HEAD..@{u}')
	if ($behind -eq 0) {
		if ($ahead) { fEcho "OK: up to date with upstream ($ahead ahead)" }
		else        { fEcho "OK: up to date with upstream" }
		return
	}
	if ($ahead -gt 0) { fDie "diverged from upstream ($ahead ahead, $behind behind) - reconcile first, or rerun with -NoSync" }
	## Behind only: a fast-forward can't lose anything. Same stash dance as fPublish
	## so a dirty tree can't block the pull.
	& git diff --quiet;          $dirtyTracked = ($LASTEXITCODE -ne 0)
	& git diff --cached --quiet; $dirtyStaged  = ($LASTEXITCODE -ne 0)
	$untracked = (& git ls-files --others --exclude-standard)
	$didStash = $false
	if ($dirtyTracked -or $dirtyStaged -or $untracked) {
		$before = @(& git stash list).Count
		fEcho_Clean "git stash push --include-untracked ..."
		fExec "git stash" "git" @("stash", "push", "--include-untracked", "-m", "auto-stash")
		$after = @(& git stash list).Count
		$didStash = ($after -gt $before)
	}
	fEcho_Clean "git pull --ff-only ..."
	fExec "git pull" "git" @("pull", "--ff-only")
	if ($didStash) {
		fEcho_Clean "git stash pop ..."
		fExec "git stash pop" "git" @("stash", "pop")
	}
	fEcho "OK: fast-forwarded $behind commit(s) from upstream"
}

## Publish: the host-side half of the Linux backup+publish, MINUS the rar version
## archive (that lives in the Linux-only n8git_backup-and-publish). stash (if dirty)
## -> pull --no-ff (if upstream) -> pop -> add -> commit -> push. $Msg empty means
## "let git open its editor" (core.editor / EDITOR).
function fPublish {
	param([Parameter(Mandatory)][AllowEmptyString()][string]$Msg)
	$branch = (& git rev-parse --abbrev-ref HEAD).Trim()
	fNote "branch: $branch"

	& git diff --quiet;          $dirtyTracked = ($LASTEXITCODE -ne 0)
	& git diff --cached --quiet; $dirtyStaged  = ($LASTEXITCODE -ne 0)
	$untracked = (& git ls-files --others --exclude-standard)
	$didStash = $false
	if ($dirtyTracked -or $dirtyStaged -or $untracked) {
		$before = @(& git stash list).Count
		fEcho_Clean "git stash push --include-untracked ..."
		fExec "git stash" "git" @("stash", "push", "--include-untracked", "-m", "auto-stash")
		$after = @(& git stash list).Count
		$didStash = ($after -gt $before)
	}

	## Sync with this branch's upstream if it has one (a brand-new local branch has
	## nothing to pull; the push below sets its upstream on first publish). --no-edit
	## keeps an unattended run from blocking on a merge-commit editor.
	& git rev-parse --abbrev-ref '@{u}' 2>$null | Out-Null
	$hasUpstream = ($LASTEXITCODE -eq 0)
	if ($hasUpstream) {
		fEcho_Clean "git pull --no-ff ..."
		fExec "git pull" "git" @("pull", "--no-ff", "--no-edit")
	}
	if ($didStash) {
		fEcho_Clean "git stash pop ..."
		fExec "git stash pop" "git" @("stash", "pop")
	}

	fEcho_Clean "git add --all ..."
	fExec "git add" "git" @("add", "--all")

	& git diff --cached --quiet; $hasStaged = ($LASTEXITCODE -ne 0)
	if ($hasStaged) {
		if ($Msg) {
			fExec "git commit" "git" @("commit", "-m", $Msg)
			fEcho "OK: committed (`"$Msg`")"
		} else {
			## No message -> let git open the configured editor (core.editor / EDITOR).
			& git commit
			if ($LASTEXITCODE -ne 0) { fDie "git commit failed or was aborted (empty message?)" }
			fEcho "OK: committed (via editor)"
		}
	} else {
		fNote "nothing to commit"
	}

	## Push: set upstream on first publish, else push only when ahead.
	if (-not $hasUpstream) {
		fEcho_Clean "git push -u origin HEAD ..."
		fExec "git push" "git" @("push", "-u", "origin", "HEAD")
		fEcho "OK: pushed $branch (upstream set)"
	} else {
		$ahead = (& git log '@{u}..' --oneline)
		if ($ahead) {
			fEcho_Clean "git push origin ..."
			fExec "git push" "git" @("push", "origin")
			fEcho "OK: pushed $branch"
		} else {
			fNote "up to date with upstream; nothing to push"
		}
	}
}


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Entry point

function fMain {
	Set-Location -LiteralPath $Root
	$stamp = Get-Date -Format "yyyyMMdd-HHmmss"

	## Gate mode: format-check + lints + tests, then exit. Fast local stand-in for a
	## hosted CI check; nothing is mutated or published. nemo-anywhere has no C
	## formatter/linter gate yet (see config.bash), so today the gate IS the
	## container smoke test - which is what the Linux pre-push hook enforces too.
	if ($Gate) {
		fSection "Gate 1/3  Format check"
		fNote "format check skipped (no C formatter gate yet)"
		fSection "Gate 2/3  Lints"
		fNote "lints skipped (no C linter gate yet)"
		fSection "Gate 3/3  Tests (smoke, in container)"
		fDockerRun "smoke test" $SmokeCmd
		fSection "$AppName gate: PASSED."
		fEcho_Clean
		return
	}

	## Resolve the publish commit message: -Message wins, then an auto stamp when
	## unattended; interactive runs capture it at the preflight prompt below. An
	## empty message at commit time means "let git open its editor".
	$publishMsg = ""
	if     ($Message)     { $publishMsg = $Message }
	elseif ($Unattended)  { $publishMsg = "$AppName CI/CD $stamp" }

	## Preflight summary.
	fEcho_Clean
	fEcho_Clean "$AppName Windows CI/CD"
	fEcho_Clean
	fEcho_Clean "Repo root ...: $Root"
	fEcho_Clean "Version .....: $(fVersion)  (source/meson.build)"
	fEcho_Clean "Jobs ........: $CicdMaxJobs of $Cores cores"
	fEcho_Clean "Container ...: $Container (build + smoke via Docker Desktop)"
	fEcho_Clean "Remote sync .: $(if ($NoSync) { '(skipped)' } else { 'fetch + fast-forward check' })"
	fEcho_Clean "Format ......: (disabled - no C formatter gate yet)"
	fEcho_Clean "Debug build .: $(if ($NoBuild) { '(skipped)' } else { 'meson + ninja, in container' })"
	fEcho_Clean "Tests .......: $(if ($NoBuild) { '(skipped)' } else { 'headless --version smoke, in container' })"
	fEcho_Clean "Release .....: (disabled - no host-side release binary yet)"
	fEcho_Clean "Packages ....: (disabled - depends on the release stage)"
	fEcho_Clean "Dogfood .....: (disabled - depends on the release stage)"
	if ($NoPublish)          { fEcho_Clean "Publish .....: (skipped)" }
	elseif ($publishMsg)     { fEcho_Clean "Publish .....: commit + push current branch (hands-off: `"$publishMsg`")" }
	else                     { fEcho_Clean "Publish .....: commit + push current branch (will prompt; blank = editor)" }
	fEcho_Clean
	fEcho_Clean "Fail-fast: any error aborts before the next stage."
	fEcho_Clean

	## Capture the commit message up front so the run finishes unattended. This is
	## the natural place to bail on the common (publish) path - Ctrl+C aborts.
	if (-not $Unattended -and -not $NoPublish -and -not $publishMsg) {
		$m = Read-Host "Publish commit message (blank = editor; Ctrl+C aborts)"
		## Read-Host bypasses the blank counter; reset it so the next section's
		## leading blank isn't swallowed (the prompt line is now the last output).
		$script:WasLastEchoBlank = $false
		if ($m) { $publishMsg = $m }
	}

	## Start the transcript once past the preflight.
	New-Item -ItemType Directory -Path $LogDir -Force | Out-Null
	try { Start-Transcript -LiteralPath (Join-Path $LogDir "run_$stamp.log") | Out-Null } catch {}

	## Stage 0: remote sync.
	fSection "0  Remote sync"
	if ($NoSync) { fNote "remote sync skipped" }
	else { fRemoteSync }

	## Stage 1: format (disabled - no C formatter gate yet; see config.bash).
	fSection "1  Format"
	fNote "format disabled (no C formatter gate yet)"

	## Stage 2: debug build (meson + ninja, in the nemo-build container).
	fSection "2  Debug build"
	if ($NoBuild) { fNote "debug build skipped (-NoBuild)" }
	else { fDockerRun "debug build" $BuildCmd }

	## Stage 3: tests (headless --version smoke, in the container). No lints yet.
	fSection "3  Tests"
	if ($NoBuild) { fNote "tests skipped (-NoBuild)" }
	else { fDockerRun "smoke test" $SmokeCmd }

	## Stage 4: release build (disabled). NEEDS: an optimized meson buildtype and the
	## binary copied out of the container's /build onto the host before it can be
	## collected, packaged, or dogfooded. No native-Windows build exists yet.
	fSection "4  Release build"
	fNote "release build disabled (no host-side release binary yet)"

	## Stage 5: packages (disabled). NEEDS: the release stage first, then the NSIS
	## installer per built arch (template shared with the Linux lane).
	fSection "5  Packages"
	fNote "packages disabled (depends on the release stage)"

	## Stage 6: dogfood (disabled). NEEDS: the release stage first, then a chosen
	## install story (nemo installs via `meson install` into a prefix, not a single
	## binary drop).
	fSection "6  Dogfood"
	fNote "dogfood disabled (depends on the release stage)"

	## Stage 7: publish.
	fSection "7  Publish"
	if ($NoPublish) { fNote "publish skipped" }
	else { fPublish -Msg $publishMsg }

	fSection "$AppName Windows CI/CD: done."
	fEcho_Clean
}

try {
	fMain
} finally {
	try { Stop-Transcript | Out-Null } catch {}
}


##	History:
##		- 2026-07-30: Created. Windows companion to cicd.bash: container debug build
##		  + smoke via Docker Desktop, native git remote-sync + publish (rar step
##		  dropped). Release/packages/dogfood/profiler shown disabled, mirroring
##		  config.bash; they light up once a host-side release binary exists.
