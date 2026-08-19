# Changelog

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

<!--
## vNEXT - DATE

### Notes

### Added

### Changed

### Removed

### Other work
-->

## vNEXT - unreleased

### Added

- An Appearance page in Preferences: Light, Dark, or follow the system, plus a style and an icon theme to draw with. Both lists only offer themes drawn for the mode you are in, and picking one half of a light/dark pair follows the pair when the mode changes.
- Windows and macOS builds carry a set of themes so the app looks like something out of the box: Windows 11, Windows 10, Windows 7, Windows XP and macOS window styles, and nine icon styles including Windows XP and Windows 7 sets drawn for this project. Linux keeps using whatever the desktop provides.
- Themes can be dropped in on any platform. Put a GTK theme folder in `themes`, or an icon theme in `icons`, beside the settings file and it shows up in the lists.
- A small panel while Windows starts the app, saying what it is doing, so a launch that takes a moment does not look like nothing happened. It goes away the instant the real window has drawn.

### Changed

- The bundled icon themes are trimmed to the icons a file manager actually asks for. The Windows 11 set went from 1.8 MB to around 300 KB; anything not shipped falls back the way icon themes are meant to.
- Settings now live where each platform expects them: `%APPDATA%\nemo-anywhere` on Windows, `~/Library/Application Support/nemo-anywhere` on macOS, `~/.config/nemo-anywhere` on Linux and BSD as before. An existing settings folder is moved to the new place on first run, so nothing is lost. Drop-in themes are unaffected.
- The single-file Windows build starts far faster. Nearly all of its startup went on unpacking the couple of thousand loose theme and icon files it carried; those now live inside the executable itself. Nothing about how themes are chosen or dropped in changes.
- The window appears at the size and place you left it as soon as it exists, rather than waiting for the first folder to finish loading.

### Fixed

- On Windows the window opened behind whatever you were already looking at, so a launch could look like nothing had happened until you noticed the taskbar button. It comes to the front now.
## v1.0.0-beta2 - 2026-08-04

### Notes

- Settings do not carry over from beta1. Configuration moved out of GSettings, and nothing can read the old store once its schema is gone, so the first run after upgrading starts from defaults.

### Added

- Linux packages: a `.deb`, an `.rpm`, and a portable `.tar.gz`. They are built against glibc 2.35, so they run on Ubuntu 22.04+, Debian 12+, Mint 21+, Fedora 36+, and anything newer.
- A Windows `.zip` beside the single executable. This is the archive the PowerShell installer looks for, so the one-liner install works now.

### Changed

- Settings live in one readable file - `~/.config/nemo-anywhere/settings.shcl`, or `%LOCALAPPDATA%\nemo-anywhere\settings.shcl` on Windows. Only the values you changed are written, each with a short comment saying what it is, and editing the file while the app is running applies straight away.

### Removed

- GSettings and dconf, along with the compiled schema that used to be installed system-wide.

### Other work

- The local build pipeline gained packaging, profiling, and a remote-sync step.

## v1.0.0-beta1 - 2026-08-04

### Notes

- First public beta. Windows shipped as a single self-contained executable; Linux was source-only at this point.
