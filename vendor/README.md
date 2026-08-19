# Vendored

## Code

SHCL config engine, single-header C binding, **MIT** - compiled into nemo, so unlike
the themes below this one is linked. Keeps its `LICENSE.md`. MIT sits fine under our
GPL-2.0-only. Update by copying `source/c/shcl.h` from a newer tag and re-pinning here.

- `shcl/shcl.h` <- https://github.com/jim-collier/shcl @ `192d206e7eeb196598d4bfc385dd9527e698ed8a` (tag `v1.2.0`)

## Themes

Regenerate with `cicd/utility/vendor-themes.bash` - do not hand-edit. Bundled as
mere aggregation: GTK reads them at runtime, nothing is linked into nemo. Each
theme keeps its own `COPYING`. Our own Luna and Aero icon sets are not here -
they are first-party art in `assets/icons`, built by `gen-icon-theme.py`.

| Theme | Kind | Style | Upstream | Commit |
| :-- | :-- | :-- | :-- | :-- |
| `Fluent` | Widget | Windows 11 | https://github.com/vinceliuice/Fluent-gtk-theme | `7a49a464b0188c340101c52965c18190b1c694cf` |
| `Windows-10-dark` | Widget | Windows 10 | https://github.com/B00merang-Project/Windows-10-Dark | `10e4bd54b8ca14f5efb741c891d19090493ff476` |
| `Windows-10` | Widget | Windows 10 | https://github.com/B00merang-Project/Windows-10 | `3a4116603b66a9adcb78f3987d7ea6f01de1cbce` |
| `Windows-7` | Widget | Windows 7 | https://github.com/B00merang-Project/Windows-7 | `943b5307b349d3526068be0fa32f7549ee37ab45` |
| `Windows-XP-dark` | Widget | Windows XP | https://github.com/B00merang-Project/Windows-XP | `7637830906823af40a3cd7e7079be753d8b7d679` |
| `Windows-XP` | Widget | Windows XP | https://github.com/B00merang-Project/Windows-XP | `7637830906823af40a3cd7e7079be753d8b7d679` |
| `macOS-dark` | Widget | macOS | https://github.com/B00merang-Project/macOS-Dark | `85f6339b864d40299c2a250131ae56b3940cb59f` |
| `macOS` | Widget | macOS | https://github.com/B00merang-Project/macOS | `3951a4224ebab3c6a37b7abcf8690a1de9a42914` |
| `Colloid-dark` | Icon | Rounded | https://github.com/vinceliuice/Colloid-icon-theme | `fa07485895a2443f7cfbceefe9dcdde798a05215` |
| `Colloid` | Icon | Rounded | https://github.com/vinceliuice/Colloid-icon-theme | `fa07485895a2443f7cfbceefe9dcdde798a05215` |
| `Fluent-dark` | Icon | Windows 11 | https://github.com/vinceliuice/Fluent-icon-theme | `ad627380aa452aa5e18fd5fbab94291f409af710` |
| `Fluent` | Icon | Windows 11 | https://github.com/vinceliuice/Fluent-icon-theme | `ad627380aa452aa5e18fd5fbab94291f409af710` |
| `Papirus` | Icon | Flat | https://github.com/PapirusDevelopmentTeam/papirus-icon-theme | `5f8b701d7521e27b4859d7e4f9b0da4c423c036c` |
| `Qogir` | Icon | Soft | https://github.com/vinceliuice/Qogir-icon-theme | `c633057ba0d27a504b3255144071c9691ed0264a` |
| `Tela` | Icon | Circles | https://github.com/vinceliuice/Tela-icon-theme | `a1fffc5bfab716bd022dd228ee96fe3965cdb33d` |
| `WhiteSur` | Icon | macOS | https://github.com/vinceliuice/WhiteSur-icon-theme | `f6a78df1c9ea8c5f804b6c72d03408ca3db3521b` |
