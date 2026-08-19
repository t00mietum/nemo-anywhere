# Assets

First-party artwork. Everything here is original work by the nemo-anywhere project and carries the project's own license (GPL-2.0-only, `license.txt` at the repo root). Third-party themes are not here - those live under `vendor/` with their own license files and pinned upstream commits.

## icons

Two icon themes, `Luna` (Windows XP) and `Aero` (Windows 7), drawn by `cicd/utility/gen-icon-theme.py` and committed so no build step depends on the script. Re-run it after changing a shape or a palette:

```sh
python3 cicd/utility/gen-icon-theme.py
```

They exist because there is no cleanly-licensed SVG icon set in either style. What circulates is Microsoft's own shell art extracted and repackaged, sometimes under a license the repackager was not in a position to grant. This project will not ship that, so the two styles are drawn from scratch instead - a shared vocabulary of base shapes (folder, page, disc, drive, monitor, bin) and glyphs, recoloured per theme.

Coverage is the file-manager surface that carries the look: folders, file types and devices. Toolbar and status glyphs are left out deliberately and fall through `Inherits` to Adwaita, which is what the freedesktop fallback chain is for.
