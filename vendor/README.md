# Vendored

## Code

SHCL config engine, single-header C binding, **MIT** - compiled into nemo, so unlike
the themes below this one is linked. Keeps its `LICENSE.md`. MIT sits fine under our
GPL-2.0-only. Update by copying `source/c/shcl.h` from a newer tag and re-pinning here.

- `shcl/shcl.h` <- https://github.com/jim-collier/shcl @ `192d206e7eeb196598d4bfc385dd9527e698ed8a` (tag `v1.2.0`)

## Themes

Regenerate with `cicd/utility/vendor-themes.bash` - do not hand-edit. Bundled as
mere aggregation: GTK reads them at runtime, nothing is linked into nemo. Each
theme keeps its own `COPYING`. Our own Windows-look icon sets are not here -
Luna, Aero, Metro and Mica are first-party art in `assets/icons`, built by
`gen-icon-theme.py`. Nothing Windows-styled is vendored as icons: every such
set that circulates draws blue folders, and Windows folders are yellow.

| Theme | Kind | Style | Upstream | Commit |
| :-- | :-- | :-- | :-- | :-- |
| `Fluent` | Widget | Windows 11 | https://github.com/vinceliuice/Fluent-gtk-theme | `7a49a464b0188c340101c52965c18190b1c694cf` |
| `macOS` | Widget | macOS | https://github.com/B00merang-Project/macOS | `3951a4224ebab3c6a37b7abcf8690a1de9a42914` |
| `macOS-dark` | Widget | macOS | https://github.com/B00merang-Project/macOS-Dark | `85f6339b864d40299c2a250131ae56b3940cb59f` |
| `Windows-10` | Widget | Windows 10 | https://github.com/B00merang-Project/Windows-10 | `3a4116603b66a9adcb78f3987d7ea6f01de1cbce` |
| `Windows-10-dark` | Widget | Windows 10 | https://github.com/B00merang-Project/Windows-10-Dark | `10e4bd54b8ca14f5efb741c891d19090493ff476` |
| `Windows-7` | Widget | Windows 7 | https://github.com/B00merang-Project/Windows-7 | `943b5307b349d3526068be0fa32f7549ee37ab45` |
| `Windows-XP` | Widget | Windows XP | https://github.com/B00merang-Project/Windows-XP | `7637830906823af40a3cd7e7079be753d8b7d679` |
| `Windows-XP-dark` | Widget | Windows XP | https://github.com/B00merang-Project/Windows-XP | `7637830906823af40a3cd7e7079be753d8b7d679` |
| `Adwaita` | Icon | Adwaita | https://gitlab.gnome.org/GNOME/adwaita-icon-theme | `e58656b901e8366f20b74ae33755ac7d8026fa4c` |
| `AdwaitaLegacy` | Icon | Adwaita | https://gitlab.gnome.org/GNOME/adwaita-icon-theme-legacy | `7642b102c4a7c4088f170f548ae37960f2443522` |
| `Colloid` | Icon | Rounded | https://github.com/vinceliuice/Colloid-icon-theme | `fa07485895a2443f7cfbceefe9dcdde798a05215` |
| `Colloid-dark` | Icon | Rounded | https://github.com/vinceliuice/Colloid-icon-theme | `fa07485895a2443f7cfbceefe9dcdde798a05215` |
| `Papirus` | Icon | Flat | https://github.com/PapirusDevelopmentTeam/papirus-icon-theme | `5f8b701d7521e27b4859d7e4f9b0da4c423c036c` |
| `BeautyLine` | Icon | Outline | https://github.com/gvolpe/BeautyLine | `6ba5aeaeea2efb5f9d25ea39bed5dfda07b3ab70` |
| `Simply-Blue-Circles` | Icon | Circles Blue | https://github.com/ju1464/Simply_Circles_Icons | `6e2985c5affa161df3daed95be6b2dc1cfb2b4d6` |
| `Simply-Cyan-Circles` | Icon | Circles Cyan | https://github.com/ju1464/Simply_Circles_Icons | `6e2985c5affa161df3daed95be6b2dc1cfb2b4d6` |
| `Simply-Orange-Circles` | Icon | Circles Orange | https://github.com/ju1464/Simply_Circles_Icons | `6e2985c5affa161df3daed95be6b2dc1cfb2b4d6` |
| `Simply-Purple-Circles` | Icon | Circles Purple | https://github.com/ju1464/Simply_Circles_Icons | `6e2985c5affa161df3daed95be6b2dc1cfb2b4d6` |
| `Simply-Red-Circles` | Icon | Circles Red | https://github.com/ju1464/Simply_Circles_Icons | `6e2985c5affa161df3daed95be6b2dc1cfb2b4d6` |
| `Simply-White-Circles` | Icon | Circles White | https://github.com/ju1464/Simply_Circles_Icons | `6e2985c5affa161df3daed95be6b2dc1cfb2b4d6` |
| `Lime-Numix-2021` | Icon | Numix Lime | https://github.com/rtlewis88/rtl88-Themes (branch `MBC-Icon-SuperPack`) | `ddbf2329bce0f8b4005665863bdde3f4c7eed2a2` |
| `MB-Lime-Suru-GLOW` | Icon | Suru Lime | https://github.com/rtlewis88/rtl88-Themes (branch `MBC-Icon-SuperPack`) | `ddbf2329bce0f8b4005665863bdde3f4c7eed2a2` |
| `Material-Black-Pistachio-Suru` | Icon | Suru Pistachio | https://github.com/rtlewis88/rtl88-Themes (branch `MBC-Icon-SuperPack`) | `ddbf2329bce0f8b4005665863bdde3f4c7eed2a2` |
| `Avidity-Dusk-Mixed-Suru` | Icon | Suru Dusk | https://github.com/rtlewis88/rtl88-Themes (branch `Avidity-Icons-and-Folders`) | `7b94756dcc0fdf25c45efa25bb7da30cc4295556` |
| `FF-BlackGreen` | Icon | Black and Green | https://www.opencode.net/felipefacundes/ff-blackgreen | `df6ff8f4aac05010d07817e8ebbdd9ffd2176ab9` |
| `FF-Flamengo-RJ-BR` | Icon | Flamengo | https://www.opencode.net/felipefacundes/ff-flamengo-rj-br | `7f23a786048c61ca5926d6563847098f2138a163` |
| `Qogir` | Icon | Soft | https://github.com/vinceliuice/Qogir-icon-theme | `c633057ba0d27a504b3255144071c9691ed0264a` |
| `Tela` | Icon | Circles | https://github.com/vinceliuice/Tela-icon-theme | `a1fffc5bfab716bd022dd228ee96fe3965cdb33d` |
| `WhiteSur` | Icon | macOS | https://github.com/vinceliuice/WhiteSur-icon-theme | `f6a78df1c9ea8c5f804b6c72d03408ca3db3521b` |

### Licences worth knowing about

Most of the above state their licence in the repository and keep it beside the theme here. Three do not, and are bundled on weaker evidence than the rest - worth revisiting if any of them ever matters:

- **`BeautyLine`** - the mirror we take it from carries no licence file, and the page it points at is behind an anti-bot wall that will not serve a licence string. nixpkgs packages the same theme and records it as public domain, which is the basis for shipping it.
- **The four `rtl88-Themes` sets** (`Lime-Numix-2021`, `MB-Lime-Suru-GLOW`, `Material-Black-Pistachio-Suru`, `Avidity-Dusk-Mixed-Suru`) - no licence file upstream. Its README states the artwork derives from Suru (GPL-3), Numix (GPL-3), La Capitaine (GPL-3 / MIT) and Twemoji (MIT), which is what it is shipped on.
- **`FF-BlackGreen`** - no licence file. Its sibling project by the same author, `FF-Flamengo-RJ-BR`, is LGPL-3.0, and the two are the same work in two colourways.

**Buuf is deliberately not here.** It was asked for, and it is CC BY-NC-SA - the NonCommercial term rules it out of anything shipped, and out of this repository entirely. Anyone who wants it can drop it into their own icons folder; `filesystem/` explains how.
