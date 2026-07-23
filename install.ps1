#!/usr/bin/env pwsh

##	Purpose:
##		- One-liner installer for Nemo Anywhere, standalone on every platform it
##		  covers: Windows, Linux, BSD, WSL, and macOS once there is a build.
##		  Fetches a release build, verifies its checksum, and installs it as a
##		  self-contained folder plus a menu entry and a name on PATH.
##		- Full parity with install.bash - same options, same plan, same result.
##		  Either script alone does the whole job; this one needs PowerShell 7,
##		  install.bash needs nothing but a shell.
##		- Idempotent: reinstalling replaces the folder in place, and -Uninstall
##		  removes exactly what was installed. Nothing is touched before the plan
##		  is printed and confirmed.
##	Syntax:
##		& ([scriptblock]::Create((irm 'https://raw.githubusercontent.com/t00mietum/nemo-anywhere/main/install.ps1'))) [options]
##			-Release dev|stable     which release to take (default: stable)
##			-Target  user|system    where to install (default: user)
##			-Arch    x64|amd64|arm64
##			                        override the detected architecture
##			-From    PATH|URL       install this archive instead of a release
##			-Uninstall              remove an existing install
##			-Yes                    don't ask before making changes
##		Windows installs to %LOCALAPPDATA%\Programs\Nemo Anywhere (user) or
##		C:\Program Files\Nemo Anywhere (system, needs an elevated shell). Unix
##		installs to ~/.local/share/nemo-anywhere (user) or /opt/nemo-anywhere
##		(system, via sudo; /usr/local/nemo-anywhere on BSD).
##	History: At bottom of script.

##	Copyright © 2026 t00mietum (ID: f⍒Ê🝅ĜᛎỹqFẅ▿⍢Ŷ‡ʬẼᛏ🜣)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

[CmdletBinding()]
param(
	[ValidateSet("dev", "stable")][string]$Release = "stable",
	[ValidateSet("user", "system")][string]$Target = "user",
	[ValidateSet("x64", "amd64", "x86_64", "arm64", "aarch64")][string]$Arch = "",
	[string]$From = "",
	[switch]$Uninstall,
	[switch]$Yes
)


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Configuration

$Repo    = "t00mietum/nemo-anywhere"
$AppName = "Nemo Anywhere"
$ExeName = "nemo-anywhere"


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
# Functions - both platforms

function fConfirm {
	if ($Yes) { return }
	## Nothing to ask at, so never assume yes: at EOF Read-Host hands back $null,
	## and a $null -notmatch test is false, which would read as a yes.
	if ([Console]::IsInputRedirected) {
		fFail "nothing to read a confirmation from - re-run with -Yes to accept the plan above"
	}
	$answer = "$(Read-Host 'Proceed? [y/N]')"
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


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Functions - Windows

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
# Functions - unix

## Every unix file step runs through here, so the privileged and plain paths
## stay one code path - sudo only prefixes the same argv. Arguments arrive as
## one array because a bare -R would otherwise bind as a parameter name.
function fSh {
	param([Parameter(Mandatory)][string[]]$Argv, [switch]$Soft)
	## Typed and assigned directly: taking the tail as an if-statement's output
	## would unroll a lone element back to a string, and splatting a string
	## hands the command one argument per character.
	[string[]]$rest = @()
	$exe = $Argv[0]
	if ($Argv.Count -gt 1) { $rest = $Argv[1..($Argv.Count - 1)] }
	if ($priv) { $rest = @($exe) + $rest; $exe = "sudo" }
	if ($Soft) { & $exe @rest 2>$null } else { & $exe @rest }
	if ($LASTEXITCODE -ne 0 -and -not $Soft) { fFail "$($Argv -join ' ') failed (exit ${LASTEXITCODE})" }
}

## Where a symlink points, or nothing when the path is not one.
function fLinkTarget {
	param([string]$Path)
	$item = Get-Item -LiteralPath $Path -Force -ErrorAction SilentlyContinue
	if (-not $item -or $item.LinkType -ne "SymbolicLink") { return $null }
	return @($item.Target)[0]
}

## Write the desktop launcher, preferring the one shipped in the prefix and
## rewriting Exec/Icon to absolute paths (nothing else knows where we landed).
function fInstallLauncher {
	$shipped = Join-Path $prefix "share/applications/${ExeName}.desktop"
	$icon    = ""
	foreach ($candidate in @(
		(Join-Path $prefix "share/icons/hicolor/scalable/apps/${ExeName}.svg"),
		(Join-Path $prefix "share/icons/hicolor/48x48/apps/${ExeName}.png"),
		(Join-Path $prefix "share/pixmaps/${ExeName}.png")
	)) { if (Test-Path -LiteralPath $candidate) { $icon = $candidate; break } }
	if (-not $icon) { $icon = $ExeName }

	$exec = "${prefix}/bin/${ExeName}"
	$tmp  = Join-Path $work "${ExeName}.desktop"
	if (Test-Path -LiteralPath $shipped) {
		## Line by line rather than -replace: a prefix containing a $ would be
		## read as a capture reference in the replacement string.
		$entry = foreach ($line in (Get-Content -LiteralPath $shipped)) {
			if     ($line -like "Exec=*")    { "Exec=${exec} %U" }
			elseif ($line -like "TryExec=*") { "TryExec=${exec}" }
			elseif ($line -like "Icon=*")    { "Icon=${icon}" }
			else                             { $line }
		}
	} else {
		$entry = @(
			"[Desktop Entry]"
			"Type=Application"
			"Name=${AppName}"
			"Comment=Browse the file system"
			"Exec=${exec} %U"
			"Icon=${icon}"
			"Terminal=false"
			"Categories=System;FileTools;FileManager;"
			"MimeType=inode/directory;"
		)
	}
	Set-Content -LiteralPath $tmp -Value $entry

	fSh @("mkdir", "-p", $appDir)
	fSh @("cp", $tmp, $launcher)
	fSh @("chmod", "644", $launcher)
}

## Desktop caches only matter to the menu showing up promptly - never fatal.
function fRefreshCaches {
	if (Get-Command update-desktop-database -ErrorAction SilentlyContinue) {
		fSh @("update-desktop-database", $appDir) -Soft
	}
	$icons = Join-Path $prefix "share/icons/hicolor"
	if ((Test-Path -LiteralPath $icons) -and (Get-Command gtk-update-icon-cache -ErrorAction SilentlyContinue)) {
		fSh @("gtk-update-icon-cache", "-qtf", $icons) -Soft
	}
}


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Script entry point

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$ProgressPreference    = "SilentlyContinue"   ## Invoke-WebRequest is far faster without the bar
## Native tools report through their exit code, which fSh checks itself. Left on
## (the 7.4 default) a non-zero exit would throw before that check is reached.
$PSNativeCommandUseErrorActionPreference = $false

fEcho_Clean ""
fEcho_Clean "${AppName} installer"

## $IsWindows and friends arrived with PowerShell 6; Windows PowerShell 5.1
## predates them and is Windows by definition. Unix flavour comes from uname,
## since .NET reports BSD as neither Linux nor macOS.
if ($PSVersionTable.PSEdition -eq "Desktop" -or $IsWindows) {
	$os = "windows"
} else {
	$uname = "$(& uname -s)".ToLowerInvariant()
	if     ($uname -like "linux*")  { $os = "linux" }
	elseif ($uname -like "*bsd*" -or $uname -like "dragonfly*") { $os = "bsd" }
	elseif ($uname -like "darwin*") { fFail "there is no macOS build yet - the .app bundle gets installed from here once there is one" }
	else                            { fFail "unsupported OS: ${uname}" }
}

## Architecture: detected from the process unless overridden.
if ($Arch) {
	$arch = if ($Arch -eq "arm64" -or $Arch -eq "aarch64") { "arm64" } else { "x86_64" }
} else {
	$arch = switch ([System.Runtime.InteropServices.RuntimeInformation]::OSArchitecture) {
		"X64"   { "x86_64" }
		"Arm64" { "arm64" }
		default { fFail "unsupported architecture: $_ (override with -Arch)" }
	}
}

## GUI package layout, both platforms: the whole folder in one place, reached by
## a menu entry and a name on PATH (a file manager gets started both ways).
$priv = $false
if ($os -eq "windows") {
	if ($Target -eq "system") {
		$prefix    = Join-Path $env:ProgramFiles $AppName
		$menuDir   = Join-Path $env:ProgramData "Microsoft\Windows\Start Menu\Programs"
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
	$leafName = $AppName
} else {
	$dataHome = if ($env:XDG_DATA_HOME) { $env:XDG_DATA_HOME } else { Join-Path $HOME ".local/share" }
	if ($Target -eq "system") {
		$prefix = if ($os -eq "bsd") { "/usr/local/${ExeName}" } else { "/opt/${ExeName}" }
		$appDir = "/usr/local/share/applications"
		$binDir = "/usr/local/bin"
		## Privileged steps go through sudo; not needed when already root.
		if ("$(& id -u)" -ne "0") {
			if (-not (Get-Command sudo -ErrorAction SilentlyContinue)) {
				fFail "-Target system needs root, and sudo was not found - re-run as root"
			}
			$priv = $true
		}
	} else {
		$prefix = Join-Path $dataHome $ExeName
		$appDir = Join-Path $dataHome "applications"
		$binDir = Join-Path $HOME ".local/bin"
	}
	$launcher = Join-Path $appDir "${ExeName}.desktop"
	$symlink  = Join-Path $binDir $ExeName
	$exePath  = "${prefix}/bin/${ExeName}"
	$leafName = $ExeName
	if (-not $prefix.StartsWith("/")) { fFail "refusing to touch a non-absolute prefix: ${prefix}" }
}

## Guard every destructive path: an unexpected prefix must never reach a delete.
if ((Split-Path -Leaf $prefix) -ne $leafName) { fFail "refusing to touch a folder not named '${leafName}': ${prefix}" }


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Uninstall

if ($Uninstall) {
	$havePrefix = Test-Path -LiteralPath $prefix

	fEcho_Clean ""
	fEcho "Uninstall plan"
	if ($os -eq "windows") {
		$haveShortcut = Test-Path -LiteralPath $shortcut
		$havePath     = fPathContains $pathScope $prefix
		$haveAnything = $havePrefix -or $haveShortcut -or $havePath
		fEcho_Clean ("Folder ....: {0}{1}" -f $prefix,   $(if ($havePrefix)   { "" } else { "   (not present)" }))
		fEcho_Clean ("Shortcut ..: {0}{1}" -f $shortcut, $(if ($haveShortcut) { "" } else { "   (not present)" }))
		fEcho_Clean ("PATH ......: {0} ({1}){2}" -f $prefix, $pathScope, $(if ($havePath) { "" } else { "   (not present)" }))
	} else {
		$haveLauncher = Test-Path -LiteralPath $launcher
		$linkTarget   = fLinkTarget $symlink
		$haveAnything = $havePrefix -or $haveLauncher -or $linkTarget
		fEcho_Clean ("Prefix ....: {0}{1}" -f $prefix,   $(if ($havePrefix)   { "" } else { "   (not present)" }))
		fEcho_Clean ("Launcher ..: {0}{1}" -f $launcher, $(if ($haveLauncher) { "" } else { "   (not present)" }))
		fEcho_Clean ("Symlink ...: {0}{1}" -f $symlink,  $(if ($linkTarget)   { "" } else { "   (not present)" }))
		if ($priv) { fEcho_Clean "Privileges : sudo (system target)" }
	}

	if (-not $haveAnything) {
		fEcho_Clean ""
		fEcho "Nothing installed here. Nothing to do."
		fEcho_Clean ""
		exit 0
	}

	fEcho_Clean ""
	fConfirm

	fEcho_Clean ""
	fEcho "Removing"
	if ($os -eq "windows") {
		if ($havePrefix -and (fInUse $prefix)) { fFail "${AppName} is still running from ${prefix} - close it and try again" }
		if ($haveShortcut) { Remove-Item -LiteralPath $shortcut -Force; fEcho_Clean "removed ${shortcut}" }
		if ($havePrefix)   { Remove-Item -LiteralPath $prefix -Recurse -Force; fEcho_Clean "removed ${prefix}" }
		if (fPathRemove $pathScope $prefix) { fEcho_Clean "removed ${prefix} from the ${pathScope} PATH" }
		$settings = "%APPDATA%\${ExeName}"
	} else {
		## Only unlink a symlink that actually points into our prefix.
		if ($linkTarget) {
			if ($linkTarget.StartsWith("${prefix}/")) {
				fSh @("rm", "-f", $symlink); fEcho_Clean "removed ${symlink}"
			} else {
				fWarn "left ${symlink} alone - it points at ${linkTarget}, not our prefix"
			}
		}
		if ($haveLauncher) { fSh @("rm", "-f",  $launcher); fEcho_Clean "removed ${launcher}" }
		if ($havePrefix)   { fSh @("rm", "-rf", $prefix);   fEcho_Clean "removed ${prefix}" }
		if (Get-Command update-desktop-database -ErrorAction SilentlyContinue) {
			fSh @("update-desktop-database", $appDir) -Soft
		}
		$settings = "~/.config/${ExeName}"
	}

	fEcho_Clean ""
	fEcho "Uninstalled. Settings in ${settings} were left in place."
	fEcho_Clean ""
	exit 0
}


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Resolve what to install

$archiveExt = if ($os -eq "windows") { "zip" } else { "tar.gz" }

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
	$asset      = "${ExeName}-${version}-${os}-${arch}.${archiveExt}"
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
		fEcho_Clean "Release ${tag} has no build for ${os}-${arch}. It publishes:"
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
fEcho_Clean "Platform ..: ${os}-${arch}"
fEcho_Clean "Download ..: ${sourceDesc}"
fEcho_Clean "Verify ....: ${verifyDesc}"
$replaces = if (Test-Path -LiteralPath $prefix) { "   (replaces the install already there)" } else { "" }
if ($os -eq "windows") {
	fEcho_Clean ("Folder ....: {0}{1}" -f $prefix, $replaces)
	fEcho_Clean "Shortcut ..: ${shortcut}"
	fEcho_Clean "PATH ......: adds ${prefix} to the ${pathScope} PATH"
} else {
	fEcho_Clean ("Prefix ....: {0}{1}" -f $prefix, $replaces)
	fEcho_Clean "Launcher ..: ${launcher}"
	fEcho_Clean "Symlink ...: ${symlink} -> ${exePath}"
	if ($priv) { fEcho_Clean "Privileges : sudo (system target)" }
}

fEcho_Clean ""
fConfirm


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Download and verify

fEcho_Clean ""
fEcho "Downloading"

$archive = Join-Path $work "${ExeName}.${archiveExt}"
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

## A downloaded archive carries a mark-of-the-web that would follow every file
## out of it and have SmartScreen block the exe. No-op off Windows.
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
New-Item -ItemType Directory -Path $unpacked -Force | Out-Null
if ($os -eq "windows") {
	try {
		Expand-Archive -LiteralPath $archive -DestinationPath $unpacked -Force
	} catch {
		fFail "could not unpack the archive ($($_.Exception.Message))"
	}
} else {
	## tar, not Expand-Archive: the unix packages are tarballs, and tar also
	## keeps the executable bits the launcher and symlink depend on.
	& tar -xzf $archive -C $unpacked
	if ($LASTEXITCODE -ne 0) { fFail "could not unpack the archive" }
}

## Archives carry one top-level folder; tolerate a flat one too.
$tree    = $unpacked
$entries = @(Get-ChildItem -LiteralPath $unpacked -Force)
if ($entries.Count -eq 1 -and $entries[0].PSIsContainer) { $tree = $entries[0].FullName }
$stagedName = if ($os -eq "windows") { "${ExeName}.exe" } else { "bin/${ExeName}" }
if (-not (Test-Path -LiteralPath (Join-Path $tree $stagedName))) {
	fFail "archive has no ${stagedName} - wrong or damaged package"
}

if ($os -eq "windows") {
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
} else {
	fSh @("rm", "-rf", $prefix)
	fSh @("mkdir", "-p", (Split-Path -Parent $prefix))
	fSh @("mv", $tree, $prefix)
	if ($Target -eq "system") {
		## Staged as the invoking user; a system prefix must not stay user-writable.
		fSh @("chown", "-R", "0:0", $prefix) -Soft
		fSh @("chmod", "-R", "a+rX", $prefix)
	}
	fEcho_Clean "prefix installed at ${prefix}"

	fInstallLauncher
	fEcho_Clean "launcher installed at ${launcher}"

	fSh @("mkdir", "-p", $binDir)
	fSh @("ln", "-sfn", $exePath, $symlink)
	fEcho_Clean "symlink installed at ${symlink}"

	fRefreshCaches
}

Remove-Item -LiteralPath $work -Recurse -Force -ErrorAction SilentlyContinue


#••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
# Done

fEcho_Clean ""
fEcho ("Installed {0} {1}" -f $AppName, $version).TrimEnd()
if ($os -eq "windows") {
	fEcho_Clean "Start it from the Start Menu, or type: ${ExeName}"
	fEcho_Clean "A new shell is needed before the PATH entry takes effect."
} else {
	fEcho_Clean "Run it from the menu, or type: ${ExeName}"
	if (-not (@($env:PATH -split ':') -contains $binDir)) {
		fEcho_Clean "Note: ${binDir} is not on your PATH yet."
	}
}
fEcho_Clean "Uninstall with the same command plus -Uninstall."
fEcho_Clean ""


##	History:
##		- 2026-07-23 JC: Created (Windows half of the one-liner install; hands
##		  off to install.bash on the unix side).
##		- 2026-07-23 JC: Now installs on unix itself instead of handing off, so
##		  either installer alone covers every platform.
