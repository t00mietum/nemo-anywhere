#!/usr/bin/env pwsh

##	Purpose:
##		- One-liner installer for Nemo Anywhere on Windows. Fetches a release
##		  build, verifies its checksum, and installs it as a self-contained
##		  folder plus a Start Menu shortcut and a PATH entry.
##		- Idempotent: reinstalling replaces the folder in place, and -Uninstall
##		  removes exactly what was installed. Nothing is touched before the plan
##		  is printed and confirmed.
##		- On Linux, BSD and macOS it hands off to install.bash, which is the
##		  native installer there (same options, same result).
##	Syntax:
##		& ([scriptblock]::Create((irm 'https://raw.githubusercontent.com/t00mietum/nemo-anywhere/main/install.ps1'))) [options]
##			-Release dev|stable     which release to take (default: stable)
##			-Target  user|system    where to install (default: user)
##			-Arch    x64|amd64|arm64
##			                        override the detected architecture
##			-From    PATH|URL       install this archive instead of a release
##			-Uninstall              remove an existing install
##			-Yes                    don't ask before making changes
##		Installs to %LOCALAPPDATA%\Programs\Nemo Anywhere (user) or
##		C:\Program Files\Nemo Anywhere (system, needs an elevated shell).
##	History: At bottom of script.

##	Copyright © 2026 t00mietum (ID: f⍒Ê🝅ĜᛎỹqFẅ▿⍢Ŷ‡ʬẼᛏ🜣)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

[CmdletBinding()]
param(
	[ValidateSet("dev", "stable")][string]$Release = "stable",
	[ValidateSet("user", "system")][string]$Target = "user",
	[ValidateSet("x64", "amd64", "arm64")][string]$Arch = "",
	[string]$From = "",
	[switch]$Uninstall,
	[switch]$Yes
)


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Configuration

$Repo    = "t00mietum/nemo-anywhere"
$AppName = "Nemo Anywhere"
$ExeName = "nemo-anywhere"
$RawBase = "https://raw.githubusercontent.com/${Repo}/main"


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Output helpers

## Same shape as install.bash: fEcho prints a bracketed status line,
## fEcho_Clean a plain one.
function fEcho       { param([string]$Msg) Write-Host "[ $Msg ]" }
function fEcho_Clean { param([string]$Msg = "") Write-Host $Msg }
function fWarn       { param([string]$Msg) Write-Host "WARNING: $Msg" -ForegroundColor Yellow }
function fFail {
	param([string]$Msg)
	Write-Host ""
	Write-Host "FAILED: $Msg" -ForegroundColor Red
	Write-Host ""
	exit 1
}


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Functions

## Linux/BSD/macOS: install.bash is the native installer there. Prefer a copy
## sitting beside this script (a repo checkout), else pull the published one.
function fHandOffToBash {
	## $PSCommandPath is empty when this came in over the one-liner, so there is
	## no sibling to prefer then - fall through to the published copy.
	$script = $null
	if ($PSCommandPath) {
		$sibling = Join-Path (Split-Path -Parent $PSCommandPath) "install.bash"
		if (Test-Path -LiteralPath $sibling) { $script = $sibling }
	}

	if (-not $script) {
		$script = Join-Path ([System.IO.Path]::GetTempPath()) "${ExeName}-install.bash"
		try {
			Invoke-WebRequest -Uri "${RawBase}/install.bash" -OutFile $script -UseBasicParsing
		} catch {
			fFail "couldn't fetch install.bash ($($_.Exception.Message))"
		}
	}

	$passArgs = @()
	if ($Release -ne "stable") { $passArgs += @("--release", $Release) }
	if ($Target  -ne "user")   { $passArgs += @("--target",  $Target) }
	if ($Arch)                 { $passArgs += @("--arch",    $Arch) }
	if ($From)                 { $passArgs += @("--from",    $From) }
	if ($Uninstall)            { $passArgs += "--uninstall" }
	if ($Yes)                  { $passArgs += "-y" }

	fEcho_Clean ""
	fEcho "Not Windows - handing off to install.bash"
	& bash $script @passArgs
	exit $LASTEXITCODE
}

function fConfirm {
	if ($Yes) { return }
	$answer = Read-Host "Proceed? [y/N]"
	if ($answer -notmatch '^[yY]') {
		fEcho_Clean ""
		fEcho "Nothing changed."
		fEcho_Clean ""
		exit 1
	}
}

## stable = the newest non-prerelease; dev = the newest release of any kind.
function fResolveTag {
	$api = "https://api.github.com/repos/${Repo}"
	try {
		$info = if ($Release -eq "stable") {
			Invoke-RestMethod -Uri "${api}/releases/latest" -UseBasicParsing
		} else {
			Invoke-RestMethod -Uri "${api}/releases?per_page=10" -UseBasicParsing | Select-Object -First 1
		}
		if (-not $info) { return $null }
		return $info.tag_name
	} catch {
		return $null
	}
}

## True when a running process is executing something inside the given folder -
## Windows can't replace files that are still open.
function fInUse {
	param([string]$Folder)
	$full = [System.IO.Path]::GetFullPath($Folder)
	foreach ($proc in (Get-Process -ErrorAction SilentlyContinue)) {
		try {
			$path = $proc.Path
			if ($path -and $path.StartsWith($full, [StringComparison]::OrdinalIgnoreCase)) { return $true }
		} catch { }
	}
	return $false
}

function fMakeShortcut {
	param([string]$LinkPath, [string]$TargetPath, [string]$WorkDir)
	$shell = New-Object -ComObject WScript.Shell
	$link  = $shell.CreateShortcut($LinkPath)
	$link.TargetPath       = $TargetPath
	$link.WorkingDirectory = $WorkDir
	$link.IconLocation     = $TargetPath
	$link.Description      = $AppName
	$link.Save()
}

## PATH edits go through the registry, NOT [Environment]::SetEnvironmentVariable:
## that reads the value expanded and writes it back literally, which would bake
## %SystemRoot% and friends into the machine PATH permanently. Read raw, write
## back as an expandable string, and leave every other entry untouched.
function fPathKey {
	param([string]$Scope)
	if ($Scope -eq "Machine") { return "HKLM:\SYSTEM\CurrentControlSet\Control\Session Manager\Environment" }
	return "HKCU:\Environment"
}

function fPathRead {
	param([string]$Scope)
	$key = Get-Item -LiteralPath (fPathKey $Scope)
	return [string]$key.GetValue("Path", "", [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
}

function fPathWrite {
	param([string]$Scope, [string]$Value)
	Set-ItemProperty -LiteralPath (fPathKey $Scope) -Name "Path" -Value $Value -Type ExpandString
	## Without this, a newly opened console keeps inheriting the old PATH until
	## the next sign-in. Best effort - the install is fine either way.
	try {
		if (-not ("NemoEnvBroadcast" -as [type])) {
			Add-Type -Namespace "" -Name "NemoEnvBroadcast" -MemberDefinition @"
[System.Runtime.InteropServices.DllImport("user32.dll", SetLastError = true, CharSet = System.Runtime.InteropServices.CharSet.Auto)]
public static extern System.IntPtr SendMessageTimeout(System.IntPtr hWnd, uint Msg, System.IntPtr wParam, string lParam, uint fuFlags, uint uTimeout, out System.UIntPtr lpdwResult);
"@
		}
		$ignored = [UIntPtr]::Zero
		[NemoEnvBroadcast]::SendMessageTimeout([IntPtr]0xffff, 0x1A, [IntPtr]::Zero, "Environment", 2, 5000, [ref]$ignored) | Out-Null
	} catch { }
}

function fPathContains {
	param([string]$Scope, [string]$Dir)
	$current = fPathRead $Scope
	if (-not $current) { return $false }
	return @($current -split ';' | Where-Object { $_.TrimEnd('\') -ieq $Dir.TrimEnd('\') }).Count -gt 0
}

function fPathAdd {
	param([string]$Scope, [string]$Dir)
	if (fPathContains $Scope $Dir) { return $false }
	$current = fPathRead $Scope
	fPathWrite $Scope $(if ($current) { "$($current.TrimEnd(';'));$Dir" } else { $Dir })
	return $true
}

function fPathRemove {
	param([string]$Scope, [string]$Dir)
	if (-not (fPathContains $Scope $Dir)) { return $false }
	$kept = (fPathRead $Scope) -split ';' | Where-Object { $_ -and ($_.TrimEnd('\') -ine $Dir.TrimEnd('\')) }
	fPathWrite $Scope ($kept -join ';')
	return $true
}


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Script entry point

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference    = "SilentlyContinue"   ## Invoke-WebRequest is far faster without the bar

fEcho_Clean ""
fEcho_Clean "${AppName} installer"

if (-not $IsWindows) { fHandOffToBash }

## Architecture: detected from the process unless overridden.
if ($Arch) {
	$arch = if ($Arch -eq "arm64") { "arm64" } else { "x86_64" }
} else {
	$arch = switch ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture) {
		"X64"   { "x86_64" }
		"Arm64" { "arm64" }
		default { fFail "unsupported architecture: $_ (override with -Arch)" }
	}
}

## GUI package layout: the whole folder in one place, reached by a Start Menu
## shortcut and a PATH entry (a file manager gets started both ways).
if ($Target -eq "system") {
	$prefix   = Join-Path $env:ProgramFiles $AppName
	$menuDir  = Join-Path $env:ProgramData "Microsoft\Windows\Start Menu\Programs"
	$pathScope = "Machine"
	$identity  = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
	if (-not $identity.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
		fFail "-Target system needs an elevated shell - re-run this from 'Run as administrator', or use -Target user"
	}
} else {
	$prefix    = Join-Path $env:LOCALAPPDATA "Programs\$AppName"
	$menuDir   = Join-Path $env:APPDATA "Microsoft\Windows\Start Menu\Programs"
	$pathScope = "User"
}
$shortcut = Join-Path $menuDir "${AppName}.lnk"
$exePath  = Join-Path $prefix "${ExeName}.exe"

## Guard every destructive path: an unexpected prefix must never reach a delete.
if ((Split-Path -Leaf $prefix) -ne $AppName) { fFail "refusing to touch a folder not named '${AppName}': ${prefix}" }


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Uninstall

if ($Uninstall) {
	$havePrefix   = Test-Path -LiteralPath $prefix
	$haveShortcut = Test-Path -LiteralPath $shortcut
	$havePath     = fPathContains $pathScope $prefix

	fEcho_Clean ""
	fEcho "Uninstall plan"
	fEcho_Clean ("Folder ....: {0}{1}" -f $prefix,   $(if ($havePrefix)   { "" } else { "   (not present)" }))
	fEcho_Clean ("Shortcut ..: {0}{1}" -f $shortcut, $(if ($haveShortcut) { "" } else { "   (not present)" }))
	fEcho_Clean ("PATH ......: {0} ({1}){2}" -f $prefix, $pathScope, $(if ($havePath) { "" } else { "   (not present)" }))

	if (-not ($havePrefix -or $haveShortcut -or $havePath)) {
		fEcho_Clean ""
		fEcho "Nothing installed here. Nothing to do."
		fEcho_Clean ""
		exit 0
	}

	fEcho_Clean ""
	fConfirm

	fEcho_Clean ""
	fEcho "Removing"
	if ($havePrefix -and (fInUse $prefix)) { fFail "${AppName} is still running from ${prefix} - close it and try again" }
	if ($haveShortcut) { Remove-Item -LiteralPath $shortcut -Force; fEcho_Clean "removed ${shortcut}" }
	if ($havePrefix)   { Remove-Item -LiteralPath $prefix -Recurse -Force; fEcho_Clean "removed ${prefix}" }
	if (fPathRemove $pathScope $prefix) { fEcho_Clean "removed ${prefix} from the ${pathScope} PATH" }

	fEcho_Clean ""
	fEcho "Uninstalled. Settings in %APPDATA%\${ExeName} were left in place."
	fEcho_Clean ""
	exit 0
}


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Resolve what to install

$work = Join-Path ([System.IO.Path]::GetTempPath()) "${ExeName}-install-$([System.IO.Path]::GetRandomFileName())"
New-Item -ItemType Directory -Path $work -Force | Out-Null

fEcho_Clean ""
fEcho "Resolving"

$sumsUrl = ""
if ($From) {
	$downloadUrl  = $From
	$sourceDesc   = $From
	## Display only: a conventionally named archive still tells us its version.
	$version      = if ((Split-Path -Leaf $From) -match "^${ExeName}-([0-9][^-]*)") { $Matches[1] } else { "" }
	$releaseDesc  = "local archive"
	$verifyDesc   = "no checksum (-From)"
} else {
	$tag = fResolveTag
	if (-not $tag) { fFail "no ${Release} release published yet for ${Repo}" }
	$version    = $tag -replace '^v', ''
	$asset      = "${ExeName}-${version}-windows-${arch}.zip"
	$sumsAsset  = "${ExeName}-${version}-sha256sums.txt"

	try {
		$tagInfo = Invoke-RestMethod -Uri "https://api.github.com/repos/${Repo}/releases/tags/${tag}" -UseBasicParsing
	} catch {
		fFail "couldn't read release ${tag} ($($_.Exception.Message))"
	}
	## Read the property off the matched asset, not off the match expression -
	## strict mode throws on a property of nothing.
	$assetInfo   = $tagInfo.assets | Where-Object { $_.name -eq $asset     } | Select-Object -First 1
	$sumsInfo    = $tagInfo.assets | Where-Object { $_.name -eq $sumsAsset } | Select-Object -First 1
	$downloadUrl = if ($assetInfo) { $assetInfo.browser_download_url } else { "" }
	$sumsUrl     = if ($sumsInfo)  { $sumsInfo.browser_download_url  } else { "" }

	if (-not $downloadUrl) {
		fEcho_Clean ""
		fEcho_Clean "Release ${tag} has no build for windows-${arch}. It publishes:"
		$tagInfo.assets | ForEach-Object { fEcho_Clean "  $($_.name)" }
		fFail "no ${asset} in release ${tag}"
	}
	$sourceDesc  = $downloadUrl
	$releaseDesc = "${Release} ${version}"
	$verifyDesc  = if ($sumsUrl) { "sha256, against ${sumsAsset}" } else { "UNVERIFIED - release publishes no checksums" }
}

fEcho_Clean ""
fEcho "Plan"
fEcho_Clean "Release ...: ${releaseDesc}"
fEcho_Clean "Platform ..: windows-${arch}"
fEcho_Clean "Download ..: ${sourceDesc}"
fEcho_Clean "Verify ....: ${verifyDesc}"
fEcho_Clean ("Folder ....: {0}{1}" -f $prefix, $(if (Test-Path -LiteralPath $prefix) { "   (replaces the install already there)" } else { "" }))
fEcho_Clean "Shortcut ..: ${shortcut}"
fEcho_Clean "PATH ......: adds ${prefix} to the ${pathScope} PATH"

fEcho_Clean ""
fConfirm


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Download and verify

fEcho_Clean ""
fEcho "Downloading"

$archive = Join-Path $work "${ExeName}.zip"
if ($From -and (Test-Path -LiteralPath $From)) {
	Copy-Item -LiteralPath $From -Destination $archive
	fEcho_Clean "using local archive ${From}"
} else {
	try {
		Invoke-WebRequest -Uri $downloadUrl -OutFile $archive -UseBasicParsing
	} catch {
		fFail "download failed ($($_.Exception.Message))"
	}
	fEcho_Clean "got $((Get-Item -LiteralPath $archive).Length) bytes"
}

## A downloaded zip carries a mark-of-the-web that would follow every file out
## of it and have SmartScreen block the exe.
try { Unblock-File -LiteralPath $archive -ErrorAction Stop } catch { }

if (-not $From -and $sumsUrl) {
	$sumsFile = Join-Path $work "sums.txt"
	Invoke-WebRequest -Uri $sumsUrl -OutFile $sumsFile -UseBasicParsing
	$line = Get-Content -LiteralPath $sumsFile | Where-Object { $_ -match "[ *]$([regex]::Escape($asset))$" } | Select-Object -First 1
	if (-not $line) { fFail "${sumsAsset} has no line for ${asset}" }
	$expected = ($line -split '\s+')[0]
	$actual   = (Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash
	if ($actual -ine $expected) { fFail "checksum mismatch - expected ${expected}, got ${actual}" }
	fEcho_Clean "sha256 verified"
} else {
	fWarn "skipping checksum verification"
}


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Install

fEcho_Clean ""
fEcho "Installing"

$unpacked = Join-Path $work "unpacked"
try {
	Expand-Archive -LiteralPath $archive -DestinationPath $unpacked -Force
} catch {
	fFail "could not unpack the archive ($($_.Exception.Message))"
}

## Archives carry one top-level folder; tolerate a flat one too.
$tree    = $unpacked
$entries = @(Get-ChildItem -LiteralPath $unpacked)
if ($entries.Count -eq 1 -and $entries[0].PSIsContainer) { $tree = $entries[0].FullName }
if (-not (Test-Path -LiteralPath (Join-Path $tree "${ExeName}.exe"))) {
	fFail "archive has no ${ExeName}.exe - wrong or damaged package"
}

if ((Test-Path -LiteralPath $prefix) -and (fInUse $prefix)) {
	fFail "${AppName} is still running from ${prefix} - close it and try again"
}
if (Test-Path -LiteralPath $prefix) { Remove-Item -LiteralPath $prefix -Recurse -Force }
New-Item -ItemType Directory -Path (Split-Path -Parent $prefix) -Force | Out-Null
Move-Item -LiteralPath $tree -Destination $prefix
fEcho_Clean "folder installed at ${prefix}"

New-Item -ItemType Directory -Path $menuDir -Force | Out-Null
fMakeShortcut -LinkPath $shortcut -TargetPath $exePath -WorkDir $prefix
fEcho_Clean "shortcut installed at ${shortcut}"

if (fPathAdd $pathScope $prefix) {
	fEcho_Clean "added ${prefix} to the ${pathScope} PATH"
} else {
	fEcho_Clean "${prefix} was already on the ${pathScope} PATH"
}

Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Done

fEcho_Clean ""
fEcho ("Installed {0} {1}" -f $AppName, $version).TrimEnd()
fEcho_Clean "Start it from the Start Menu, or type: ${ExeName}"
fEcho_Clean "A new shell is needed before the PATH entry takes effect."
fEcho_Clean "Uninstall with the same command plus -Uninstall."
fEcho_Clean ""


##	History:
##		- 2026-07-23 JC: Created (Windows half of the one-liner install; hands
##		  off to install.bash on the unix side).
