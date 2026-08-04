#!/usr/bin/env bash

##	- Fetch + trim the vinceliuice Fluent (Windows-11 style) GTK widget theme and
##	  icon theme into vendor/, so the Windows bundle can ship a native-looking
##	  light/dark theme. Re-run to refresh from upstream.
##	- Both upstream themes are GPL-3.0. We bundle them as mere aggregation (data
##	  consumed by GTK at runtime, not linked into nemo), so nemo stays GPL-2.0-only.
##	  We keep each theme's COPYING and pin the source commit in vendor/README.md.
##	- Layout mirrors upstream install.sh: gtk-3.0/assets holds the widget PNGs,
##	  gtk-3.0/assets/scalable the SVGs; gtk.css is light, gtk-dark.css the dark
##	  variant (GTK loads gtk-dark.css when gtk-application-prefer-dark-theme is on).
##	- Icons: only the file-manager contexts (colorful folders/files/devices +
##	  symbolic sidebar/emblems). The huge actions/status sets are dropped; those
##	  fall back to Adwaita via Inherits, which is fine for neutral toolbar glyphs.
##	- Syntax: bash cicd/win/vendor-themes.bash   (needs git + network)

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"			# repo: .../github
vendor="$root/vendor"
tmp="${TMPDIR:-/tmp}/fluent-vendor"
GTK_URL="https://github.com/vinceliuice/Fluent-gtk-theme"
ICON_URL="https://github.com/vinceliuice/Fluent-icon-theme"

## git-for-windows isn't on the mingw64 login PATH on this box.
[[ -d "/c/Program Files/Git/bin" ]] && export PATH="/c/Program Files/Git/bin:$PATH"
command -v git >/dev/null || { echo "[ FAILED: git not found ]"; exit 1; }

fEcho(){ echo "[ $* ]"; }

rm -rf "$tmp"; mkdir -p "$tmp"
fEcho "Cloning Fluent-gtk-theme"
git clone --depth 1 -q "$GTK_URL" "$tmp/gtk"
fEcho "Cloning Fluent-icon-theme (sparse src)"
git clone --no-checkout --depth 1 -q "$ICON_URL" "$tmp/icon"
( cd "$tmp/icon" && git sparse-checkout set --no-cone src links COPYING AUTHORS >/dev/null && git checkout -q )
gtk_sha="$(git -C "$tmp/gtk" rev-parse HEAD)"
icon_sha="$(git -C "$tmp/icon" rev-parse HEAD)"

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Widget theme: one folder, light default + dark variant, stock blue accent.
wt="$vendor/themes/Fluent"
rm -rf "$wt"; mkdir -p "$wt/gtk-3.0"
cp -r "$tmp/gtk/src/gtk/assets"    "$wt/gtk-3.0/assets"		# 44 url(assets/..) refs
cp -r "$tmp/gtk/src/gtk/scalable"  "$wt/gtk-3.0/assets/scalable"	# 14 url(assets/scalable/..)
cp "$tmp/gtk/src/gtk/3.0/gtk-Light.css" "$wt/gtk-3.0/gtk.css"
cp "$tmp/gtk/src/gtk/3.0/gtk-Dark.css"  "$wt/gtk-3.0/gtk-dark.css"
cp "$tmp/gtk/COPYING" "$wt/COPYING" 2>/dev/null || cp "$tmp/gtk/LICENSE" "$wt/COPYING" 2>/dev/null || true
printf '[Desktop Entry]\nType=X-GNOME-Metatheme\nName=Fluent\nComment=Windows 11 style\n\n[X-GNOME-Metatheme]\nGtkTheme=Fluent\nIconTheme=Fluent\n' > "$wt/index.theme"

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Icon theme: file-manager keep-set only. scalable = colorful (icon view),
## symbolic = monochrome (sidebar/emblems, recolored by the widget theme).
it="$vendor/icons/Fluent"
rm -rf "$it"
scal=(places mimetypes devices)
symb=(places devices emblems)
for c in "${scal[@]}"; do mkdir -p "$it/scalable/$c"; cp -r "$tmp/icon/src/scalable/$c/." "$it/scalable/$c/"; done
for c in "${symb[@]}"; do mkdir -p "$it/symbolic/$c"; cp -r "$tmp/icon/src/symbolic/$c/." "$it/symbolic/$c/"; done
cp "$tmp/icon/COPYING" "$it/COPYING" 2>/dev/null || true
cp "$tmp/icon/AUTHORS" "$it/AUTHORS" 2>/dev/null || true

## Materialize the standard freedesktop names. Upstream ships them as symlinks
## under links/ (e.g. folder -> default-folder, text-plain -> text-x-generic);
## a Windows checkout writes those as text files whose content is the target
## filename. GTK/nemo request these canonical names, so resolve each to its real
## SVG and copy it in. Curated to the names a file manager uses - materializing
## all ~1600 aliases (akonadi, insync, per-app) would just bloat the theme.
mat_alias(){	# ctx-path, alias-name (no ext) : copy resolved target -> alias.svg
	local ctx="$1" name="$2" cur="$2.svg" hops=0 rf lf
	while (( hops++ < 6 )); do
		rf="$tmp/icon/src/$ctx/$cur"; lf="$tmp/icon/links/$ctx/$cur"
		[[ -f "$rf" ]] && { cp -f "$rf" "$it/$ctx/$name.svg"; return 0; }
		[[ -f "$lf" ]] || return 1
		cur="$(basename "$(tr -d '[:space:]' < "$lf")")"
	done
	return 1
}
place_names=(folder folder-open inode-directory user-home user-desktop user-trash user-trash-full user-bookmarks folder-documents folder-download folder-downloads folder-music folder-pictures folder-videos folder-video folder-publicshare folder-templates folder-remote folder-recent network-workgroup network-server network-workgroup start-here)
mime_names=(text-x-generic text-plain text-html text-x-script application-x-generic application-octet-stream unknown application-pdf application-zip application-x-archive package-x-generic application-x-executable image-x-generic audio-x-generic video-x-generic font-x-generic x-office-document x-office-spreadsheet x-office-presentation application-x-shellscript)
dev_names=(drive-harddisk drive-harddisk-usb drive-removable-media drive-optical media-optical media-flash media-removable computer)
symplace_names=(folder-symbolic user-home-symbolic user-desktop-symbolic user-trash-symbolic network-workgroup-symbolic network-server-symbolic start-here-symbolic)
symdev_names=(drive-harddisk-symbolic drive-removable-media-symbolic drive-optical-symbolic media-optical-symbolic computer-symbolic)
for n in "${place_names[@]}";    do mat_alias scalable/places "$n"    || true; done
for n in "${mime_names[@]}";     do mat_alias scalable/mimetypes "$n" || true; done
for n in "${dev_names[@]}";      do mat_alias scalable/devices "$n"   || true; done
for n in "${symplace_names[@]}"; do mat_alias symbolic/places "$n"    || true; done
for n in "${symdev_names[@]}";   do mat_alias symbolic/devices "$n"   || true; done

## index.theme listing only shipped dirs; missing names fall back Adwaita -> hicolor.
ctx(){ case "$1" in places) echo Places;; mimetypes) echo MimeTypes;; devices) echo Devices;; emblems) echo Emblems;; esac; }
{
	printf '[Icon Theme]\nName=Fluent\nComment=Windows 11 style (nemo-anywhere subset)\nInherits=Adwaita,hicolor\nExample=folder\n\n'
	dirs=(); for c in "${scal[@]}"; do dirs+=("scalable/$c"); done; for c in "${symb[@]}"; do dirs+=("symbolic/$c"); done
	printf 'Directories=%s\n\n' "$(IFS=,; echo "${dirs[*]}")"
	for c in "${scal[@]}"; do printf '[scalable/%s]\nSize=48\nMinSize=8\nMaxSize=512\nContext=%s\nType=Scalable\n\n' "$c" "$(ctx "$c")"; done
	for c in "${symb[@]}"; do printf '[symbolic/%s]\nSize=16\nMinSize=8\nMaxSize=512\nContext=%s\nType=Scalable\n\n' "$c" "$(ctx "$c")"; done
} > "$it/index.theme"

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Provenance / GPL aggregation note.
cat > "$vendor/README.md" <<-EOF
	# Vendored themes

	Regenerate with \`cicd/win/vendor-themes.bash\` - do not hand-edit.

	Fluent GTK + icon themes by vinceliuice, **GPL-3.0**, bundled as mere aggregation
	(runtime data, not linked into nemo). Each theme keeps its \`COPYING\`.

	- \`themes/Fluent\` <- $GTK_URL @ \`$gtk_sha\`
	- \`icons/Fluent\`  <- $ICON_URL @ \`$icon_sha\` (file-manager subset)
	EOF

fEcho "widget theme: $(du -sh "$wt" | cut -f1)"
fEcho "icon theme:   $(du -sh "$it" | cut -f1)"
fEcho "OK -> $vendor"
