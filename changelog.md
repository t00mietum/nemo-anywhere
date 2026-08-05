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
