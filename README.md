<!-- markdownlint-disable MD007 -- Unordered list indentation -->
<!-- markdownlint-disable MD010 -- No hard tabs -->
<!-- markdownlint-disable MD033 -- No inline html -->
<!-- markdownlint-disable MD055 -- Table pipe style [Expected: leading_and_trailing; Actual: leading_only; Missing trailing pipe] -->
<!-- markdownlint-disable MD041 -- First line in a file should be a top-level heading -->
<div align="center">

![Made with](https://img.shields.io/badge/Made%20with-C-1f425f.svg)
![License: GPL v2](https://img.shields.io/badge/License-GPLv2-blue.svg)
[![Release](https://img.shields.io/github/v/release/t00mietum/nemo-anywhere?include_prereleases&label=release)](https://github.com/t00mietum/nemo-anywhere/releases)
![Lifecycle](https://img.shields.io/badge/Lifecycle-Beta-yellow)
![Support](https://img.shields.io/badge/Support-Maintained-brightgreen)

</div>
<!--
[![!#/bin/bash](https://img.shields.io/badge/-%23!%2Fbin%2Fbash-1f425f.svg?logo=gnu-bash)](https://www.gnu.org/software/bash/)
[![made-with-python](https://img.shields.io/badge/Made%20with-Python-1f425f.svg)](https://www.python.org/)
[![made-with-rust](https://img.shields.io/badge/Made%20with-Rust-1f425f.svg)](https://www.rust-lang.org/)
![Go](https://img.shields.io/badge/Go-00ADD8?logo=go&logoColor=white)
![Made with](https://img.shields.io/badge/Made%20with-C%2B%2B-brightgreen?style=plastic)
![License: GPL v2](https://img.shields.io/badge/License-GPLv2-blue.svg)
![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
![Lifecycle: Alpha](https://img.shields.io/badge/Lifecycle-Alpha-orange)
![Lifecycle: Beta](https://img.shields.io/badge/Lifecycle-Beta-yellow)
![Lifecycle: RC](https://img.shields.io/badge/Lifecycle-RC-blue)
![Lifecycle: Stable](https://img.shields.io/badge/Lifecycle-Stable-brightgreen)
![Coverage](https://img.shields.io/badge/Coverage-75%25-yellow)
![Status: Passing](https://img.shields.io/badge/Status-Passing-brightgreen)
-->

<!-- TOC ignore:true -->
# nemo-anywhere

Nemo, freed from its desktop. A great file manager should run anywhere. This one will.

<!-- TOC ignore:true -->
## Table of contents

<!-- TOC -->

- [Why](#why)
- [Existing features](#existing-features)
- [What this fork adds or enhances](#what-this-fork-adds-or-enhances)
- [Status](#status)
- [Installation](#installation)
	- [Packages and installers](#packages-and-installers)
	- [Direct stable and dev install scripts](#direct-stable-and-dev-install-scripts)
	- [DIY](#diy)
- [Set up development environment](#set-up-development-environment)
- [Longer-term roadmap](#longer-term-roadmap)
- [Copyright and license](#copyright-and-license)

<!-- /TOC -->

## Why

Nemo is one of the best file managers made to date. Fast, sane, powerful, and it respects how you actually work.

There is one catch. It belongs to the Cinnamon desktop. If you run anything else, you get it with strings attached - or not at all.

This project cuts the strings:

- It takes Nemo as-is, from the source.

- Removes every assumption that says "you are running Cinnamon" or even "you are running Linux".

- Removes the heavy desktop integration. Your existing manager is untouched - which can even be original Nemo, they don't conflict. (This is also a big step towards OS portability.)

- Shippable everywhere. (At least, desktop OSes.)

That means, in order:

- **Linux**:
	- Standalone on any desktop.

	- No Cinnamon dependencies. No Cinnamon, no xapp, no desktop stack pulled in behind it.

	- Doesn't try to compete with existing desktop managers for control of desktop rendering. (A real pain point with OG Nemo.)

- **Windows**: A real native build, not a compatibility shim.

- **BSD**: Just works.

- **macOS** - coming soon.

One codebase. "For Windows" and friends are just labels on builds, not separate projects.

This is an independent, unofficial hard fork of [linuxmint/nemo](https://github.com/linuxmint/nemo), taken at the 6.6.4 release. It is not affiliated with, endorsed by, or supported by Linux Mint, the Cinnamon team, or GNOME.

Report issues here, never upstream. Provenance details live in [fork.md](fork.md).

## Existing features

Everything that makes Nemo worth porting:

- Fast, no-nonsense navigation. Back, forward, up, refresh, breadcrumbs or a path box - your pick.

- Real file operation progress. See what is happening, and how far along it is.

- Folder contents merging is intuitive. No more accidental clobbering.

- Open in terminal, built in.

- Proper bookmarks.

- A deep bench of configuration options.

- An extension system with a real API.

- Copies use near-instant and near-zero-size CoW copies automatically, if the underlying filesystem allows it.

## What this fork adds or enhances

- Runs without Cinnamon. No desktop-drawing baggage, no pulled-in desktop stack.

- Runs without Linux. Native Windows first, BSD and macOS after.

- On Windows it is one executable. The whole runtime is packed inside it, so there is nothing to install and nothing to keep in step. Copy it where you like and run it.
	- Same idea as an AppImage or a Flatpak, without the runtime or the sandbox.
	- On Linux it stays a small folder that uses the GTK3 your distro already has, because that is what a Linux user expects and it keeps the download tiny.

- Settings live in one plain text file you can read and edit. No registry, no dconf, no compiled schema.

- Stays Nemo. Same code lineage, GPL intact.

## Status

Beta. It builds and runs on Linux and Windows, browses, copies, moves, trashes, searches and thumbnails. The Cinnamon decoupling is finished and the Windows port is feature-complete enough for daily use.

Rough edges to know about before you rely on it:

- The Windows build has been exercised mostly through a compatibility layer, not on real hardware. Expect papercuts.

- Settings from a pre-1.0 install do not carry over.

- macOS and BSD are not built yet.

Details:

- Plans and progress: [project/backlog.md](project/backlog.md)

- Design and reasoning: [project/design.md](project/design.md)

## Installation

Everything is on the [releases page](https://github.com/t00mietum/nemo-anywhere/releases). Pick whichever of the three below suits you. Building from source is for working on it, not for using it.

Note that the current release is a prerelease, so the install scripts need `--release dev` to find it.

### Packages and installers

- **Windows**: download `nemo-anywhere.exe` and run it. That is the whole program - the runtime is inside it. Nothing is installed and nothing is registered.

- **Debian, Ubuntu, Mint**: `sudo apt install ./nemo-anywhere-<version>-linux-x86_64.deb`

- **Fedora, openSUSE, RHEL**: `sudo dnf install ./nemo-anywhere-<version>-linux-x86_64.rpm`

Both packages install to `/opt/nemo-anywhere` with a menu entry and `nemo-anywhere` on PATH, and use the GTK3 your distro already provides.

### Direct stable and dev install scripts

One command. It downloads the right build for the machine, verifies its checksum, tells you exactly what it is about to do, and waits for a yes.

Linux, BSD, macOS, WSL:

~~~bash
bash <(curl -fsSL https://raw.githubusercontent.com/t00mietum/nemo-anywhere/main/install.bash)  [--release dev|stable]  [--target user|system]  [--arch x64|amd64|arm64]
~~~

Windows - or anywhere else with PowerShell 7, since it is a standalone installer in its own right and not a wrapper around the one above:

~~~powershell
& ([scriptblock]::Create((irm 'https://raw.githubusercontent.com/t00mietum/nemo-anywhere/main/install.ps1')))  [-Release dev|stable]  [-Target user|system]  [-Arch x64|amd64|arm64]
~~~

Add `--uninstall` (or `-Uninstall`) to reverse it. Reinstalling over an existing copy is fine - it replaces it.

Where it goes:

| OS      | User install (default)              | ￩ Launcher                                                | (or) System install     | ￩ Launcher
| :---    | :---                                | :---                                                      | :---                    | :---
| Linux   | ~/.local/share/nemo-anywhere/       | ~/.local/share/applications/ + ~/.local/bin/nemo-anywhere | /opt/nemo-anywhere/     | /usr/local/share/applications/ + /usr/local/bin/nemo-anywhere
| BSD     | ~/.local/share/nemo-anywhere/       | ~/.local/share/applications/ + ~/.local/bin/nemo-anywhere | /usr/local/nemo-anywhere/ | /usr/local/share/applications/ + /usr/local/bin/nemo-anywhere
| Windows | %LOCALAPPDATA%\Programs\Nemo Anywhere\ | Start Menu shortcut + a PATH entry                     | C:\Program Files\Nemo Anywhere\ | Start Menu shortcut + a PATH entry
| macOS   | *pending a macOS build*             |                                                           |                         |

Settings live in `~/.config/nemo-anywhere` (`%LOCALAPPDATA%\nemo-anywhere` on Windows) and are left alone by an uninstall.

### DIY

Unpack `nemo-anywhere-<version>-linux-x86_64.tar.gz` wherever you like and run `bin/nemo-anywhere` from inside it. It is relocatable, so no fixed path is required. Verify the download against the `sha256sums.txt` file published beside it.

What a Linux build needs at runtime: GTK 3.24.33 or newer and glibc 2.35 or newer, which means Ubuntu 22.04, Debian 12, Mint 21, Fedora 36 or anything more recent.

## Set up development environment

The reference Linux build happens in a container, so no development packages land on your own machine and the dependency versions are pinned to something known good.

You need Docker (or Podman with a Docker alias) and git. Everything else is fetched by the build.

~~~bash
git clone https://github.com/t00mietum/nemo-anywhere.git
cd nemo-anywhere
cicd/hooks/install.bash          # merge gate as a pre-push hook
cicd/cicd.bash --gate            # build, test and lint
~~~

`--gate` is the quick check. A bare `cicd/cicd.bash` runs the whole pipeline, which ends by committing and pushing, so leave that one until you mean it.

To build without the container, on a Linux box with the GTK3 development stack:

~~~bash
meson setup build source && ninja -C build
~~~

On Windows the build is native, not cross-compiled. You need [MSYS2](https://www.msys2.org/) with the mingw64 GTK3 toolchain, and [Enigma Virtual Box](https://enigmaprotector.com/en/aboutvb.html) if you want the single-exe artifact - without it the pipeline still builds and tests, it just skips packing.

~~~powershell
pacman -S --needed mingw-w64-x86_64-{gcc,meson,ninja,pkgconf,gtk3,json-glib,libexif,libgsf,cppcheck,gettext} intltool git
pwsh cicd/cicd-win.ps1 -Gate     # lint, build and smoke
~~~

The full picture - the exact package list, the Windows cross-compile, the release lanes and the pipeline stages - is in [project/design.md](project/design.md). Conventions for contributors are in [contributing.md](contributing.md).

## Longer-term roadmap

For maximum cross-platform portability, Nemo Anywhere needs to move off of not just GTK+ v3, but GTK+ period. While GTK+ v3 works, it's no longer actively developed, is basically stuck with C, and is comparatively weak and fragile on Windows and macOS (compared to, say, Qt). That's what the sister project [Captain Nemo](https://github.com/t00mietum/captain-nemo) is for, once this project reaches v1.0.0 stable.

## Copyright and license

The [original Nemo](https://github.com/linuxmint/nemo) is the work of the Linux Mint project and [many contributors](https://github.com/linuxmint/nemo/graphs/contributors), and is itself a hard fork from 2012 of [GNOME Files aka Nautilus](https://github.com/GNOME/nautilus).

This repository, although also a hard fork, retains all original copyright and license notices; see `license.txt` (originally 'COPYING'), `license-lib.txt` (originally 'COPYING.LIB'), `license-docs.txt` (originally 'COPYING-DOCS'), and `license-for-extensions.txt` (originally 'COPYING.EXTENSIONS').

> Copyright © 2026 t00mietum (ID: f⍒Ê🝅ĜᛎỹqFẅ▿⍢Ŷ‡ʬẼᛏ🜣)<br>
> Upstream code Copyrights © [Nemo authors](https://github.com/linuxmint/nemo/graphs/contributors).<br />
> Licensed under [GNU GPL v2](https://opensource.org/license/GPL-2.0) license. No warranty.
