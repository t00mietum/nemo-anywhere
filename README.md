<!-- markdownlint-disable MD007 -- Unordered list indentation -->
<!-- markdownlint-disable MD010 -- No hard tabs -->
<!-- markdownlint-disable MD033 -- No inline html -->
<!-- markdownlint-disable MD055 -- Table pipe style [Expected: leading_and_trailing; Actual: leading_only; Missing trailing pipe] -->
<!-- markdownlint-disable MD041 -- First line in a file should be a top-level heading -->
<div align="center">

![Made with](https://img.shields.io/badge/Made%20with-C-1f425f.svg)
![License: GPL v2](https://img.shields.io/badge/License-GPLv2-blue.svg)
![Lifecycle](https://img.shields.io/badge/Lifecycle-Alpha-orange)
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
- [Installing](#installing)
- [Building from source](#building-from-source)
- [Longer-term roadmap](#longer-term-roadmap)
- [Copyright and license](#copyright-and-license)

<!-- /TOC -->

## Why

Nemo is one of the best file managers made to date. Fast, sane, powerful, and it respects how you actually work.

There is one catch. It belongs to the Cinnamon desktop. If you run anything else, you get it with strings attached - or not at all.

This project cuts the strings:

- Takes Nemo as-is, from the source.

- Removes every assumption that says "you are running Cinnamon" or even "you are running Linux".

- Shippable everywhere. (At least, desktop OSes.)

That means, in order:

- **Linux**:
	- Standalone on any desktop.

	- No Cinnamon Desktop dependencies.

	- No external dependencies whatsoever!

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

- All dependencies are baked into a single executable. Even if author gets hit by a bus, it will resist dependency drift and bitrot for decades.
	- Same concept as `.AppImage` or Flatpack, but just native code, with dependencies statically rather than dynamically compiled for maximum portability and compatibility.

- Stays Nemo. Same code lineage, GPL intact.

## Status

Early bring-up. The fork is established and the Linux baseline builds and runs. The decoupling work is next.

- Plans and progress: [project/backlog.md](project/backlog.md)

- Design and reasoning: [project/design.md](project/design.md)

## Installing

No releases yet. When per-platform installers exist, they will land on the releases page with the GTK runtime bundled - no hunting for dependencies.

## Building from source

Right now this builds like upstream Nemo: meson and ninja on a Linux box with the GTK3 development stack. Stock Debian 13 packages are a known-good baseline. See [project/design.md](project/design.md) for the package list and steps.

## Longer-term roadmap

For maximum cross-platform portability, Nemo Anywhere needs to move off of not just GTK+ v3, but GTK+ period. While GTK+ v3 it works, it's no longer actively developed, is basically stuck with C, and is comparatively weak and fragile on Windows and macOS (compared to, say, Qt).

Here are the two main options being considered (once Nemo Anywhere v1.0.0 stable release has been live for a while):

- Rust and QML. This is the most viable option for moving away from C and for long-term maintenance. But the only viable QML bindings for Rust is `cxx-qt`. While is seems fine for now, it also carries a big vendor dependency and risk.

	- Mitigation strategy: Use `cxx-qt`, but also maintain a hard-forked subset of only the parts needed, and keep it up-to-date as we go. If and when the time comes that `cxx-qt` is ever abandoned, falls behind QML, and/or pursues different goals: Our minimal hard fork is ready to go.

- Idiomatic/RAII C++ v23, combined with native QML bindings. Harder to port the code (ironically in spite of both having the same ancestry), but less risk with QML.

## Copyright and license

The [original Nemo](https://github.com/linuxmint/nemo) is the work of the Linux Mint project and [many contributors](https://github.com/linuxmint/nemo/graphs/contributors), and is itself a hard fork from 2012 of [GNOME Files aka Nautilus](https://github.com/GNOME/nautilus).

This repository, although also a hard fork, retains all original copyright and license notices; see `license.txt` (originally 'COPYING'), `license-lib.txt` (originally 'COPYING.LIB'), `license-docs.txt` (originally 'COPYING-DOCS'), and `license-for-extensions.txt` (originally 'COPYING.EXTENSIONS').

> Copyright © 2026 t00mietum (ID: f⍒Ê🝅ĜᛎỹqFẅ▿⍢Ŷ‡ʬẼᛏ🜣)<br>
> Upstream code Copyrights © [Nemo authors](https://github.com/linuxmint/nemo/graphs/contributors).<br />
> Licensed under [GNU GPL v2](https://opensource.org/license/GPL-2.0) license. No warranty.
