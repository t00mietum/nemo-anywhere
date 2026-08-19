#!/usr/bin/env bash

##	- Fetch the bundled theme set into vendor/ - the widget themes and icon
##	  themes that ship inside the app on targets unlikely to have GTK themes
##	  installed (Windows, macOS). Re-run to refresh from upstream.
##	- Every upstream here is GPL-3.0 or MIT and is bundled as mere aggregation
##	  (data GTK reads at runtime, never linked into nemo), so nemo stays
##	  GPL-2.0-only. Each theme keeps its own license file, and the source commit
##	  is pinned in vendor/README.md.
##	- Icon themes are trimmed to the names in theme-icon-names.txt and run
##	  through svg-min.py. That is what holds a theme to a few hundred KB - the
##	  full sets are thousands of per-application icons nemo never asks for.
##	  Anything not shipped falls through Inherits to Adwaita, then hicolor.
##	- Upstream layouts all differ (src/16/places, src/scalable/places, mimes vs
##	  mimetypes, links/ aliases), so names are resolved by searching an index of
##	  the checkout rather than by hardcoding paths. Aliases are followed, real
##	  symlink or the text file a Windows checkout leaves in its place.
##	- Our own Windows-look sets are NOT fetched here - Luna, Aero, Metro and
##	  Mica live in assets/ and are built by gen-icon-theme.py. They are also
##	  why no Windows-styled icon theme is vendored: every such set that
##	  circulates draws blue folders, and Windows folders are yellow.
##	- Syntax: bash cicd/utility/vendor-themes.bash [themeId ...]   (needs git)

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"			# repo: .../github
vendor="$root/vendor"
nameList="$here/theme-icon-names.txt"
svgMin="$here/svg-min.py"

## git-for-windows isn't on the mingw64 login PATH on this box.
if [[ -d "/c/Program Files/Git/bin" ]]; then export PATH="/c/Program Files/Git/bin:$PATH"; fi
command -v git >/dev/null || { echo "[ FAILED: git not found ]"; exit 1; }

## python3 is the Store shim on Windows and answers nothing useful; test, don't assume.
python=""
for candidate in python3 python py; do
	if command -v "$candidate" >/dev/null 2>&1 && "$candidate" -c 'import sys; sys.exit(0)' >/dev/null 2>&1; then
		python="$candidate"; break
	fi
done
[[ -n "$python" ]] || { echo "[ FAILED: no working python found (needed for svg-min.py) ]"; exit 1; }

tmp="$(mktemp -d "${TMPDIR:-/tmp}/nemo-themes.XXXXXX")"
trap 'rm -rf "$tmp"' EXIT

fEcho(){ echo "[ $* ]"; }
fEcho_Clean(){ echo "$@"; }

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
##	The catalog. One record per line, pipe separated:
##
##		id | outName | style | modes | counterpart | url | ref | roots | darkFrom | fetch | inherits
##
##	roots       repo-relative dirs to search, space separated, best first. Also the
##	            sparse-checkout set, so naming Papirus's size buckets rather than
##	            the whole theme keeps the index at 15k files instead of 88k -
##	            which is the difference between two minutes and twenty.
##	darkFrom    where the dark half's art comes from, if upstream drew any:
##	            "suffix:-dark" for <name>-dark.svg beside the light one, or
##	            "roots:dark" for a separate overlay directory. Both conventions
##	            are in use and both amount to two or three files - upstream
##	            themes redraw the trash and the server for dark and nothing
##	            else, because the rest genuinely reads on either background.
##	ref         pinned commit, or HEAD to take whatever upstream has now
##	fetch       full, or sparse where the repo is far larger than what we want.
##	            Sparse is NOT the default: a blobless checkout fetches its
##	            blobs one batch at a time and measured nine times slower than
##	            one shallow packfile on a 38 MB repo. It only pays on Papirus,
##	            which is 350 MB.
##	inherits    the whole Inherits value, when the default "Adwaita,hicolor"
##	            is wrong - Adwaita is the tail of that chain itself.
##
##	A dark icon theme carries ONLY the files that actually differ and inherits
##	the rest from its light half, so a paired variant costs a few KB.
##
##	Adwaita is here for the same reason as the rest. The sysroot ships it whole,
##	AdwaitaLegacy beside it (1810 png at six sizes) and 33 X11 cursors we cannot
##	use on Windows - 2693 files to answer the ~140 names we ask of it. Adwaita
##	stopped drawing emblems and the colour mimetypes, so the legacy set stays as
##	its own theme behind it, trimmed the same way, for the couple of dozen names
##	that live nowhere else.

iconThemes=(
	"adwaita|Adwaita|Adwaita|light;dark||https://gitlab.gnome.org/GNOME/adwaita-icon-theme|HEAD|Adwaita/scalable Adwaita/symbolic Adwaita/16x16||full|AdwaitaLegacy,hicolor"
	"adwaitalegacy|AdwaitaLegacy|Adwaita|light;dark||https://gitlab.gnome.org/GNOME/adwaita-icon-theme-legacy|HEAD|AdwaitaLegacy/48x48 AdwaitaLegacy/32x32 AdwaitaLegacy/24x24 AdwaitaLegacy/22x22 AdwaitaLegacy/16x16||full|hicolor"
	"whitesur|WhiteSur|macOS|light;dark||https://github.com/vinceliuice/WhiteSur-icon-theme|HEAD|src links||full"
	"colloid|Colloid|Rounded|light|Colloid-dark|https://github.com/vinceliuice/Colloid-icon-theme|HEAD|src links|roots:dark|full"
	"tela|Tela|Circles|light;dark||https://github.com/vinceliuice/Tela-icon-theme|HEAD|src links||full"
	"qogir|Qogir|Soft|light;dark||https://github.com/vinceliuice/Qogir-icon-theme|HEAD|src links||full"
	"papirus|Papirus|Flat|light;dark||https://github.com/PapirusDevelopmentTeam/papirus-icon-theme|HEAD|Papirus/64x64 Papirus/48x48 Papirus/32x32 Papirus/24x24 Papirus/22x22 Papirus/16x16||sparse"
	"beautyline|BeautyLine|Outline|light;dark||https://github.com/gvolpe/BeautyLine|HEAD|BeautyLine||full"
	"simplyblue|Simply-Blue-Circles|Circles Blue|light;dark||https://github.com/ju1464/Simply_Circles_Icons|HEAD|Simply-Circles-GNOME/Simply-Blue-Circles||sparse"
	"simplycyan|Simply-Cyan-Circles|Circles Cyan|light;dark||https://github.com/ju1464/Simply_Circles_Icons|HEAD|Simply-Circles-GNOME/Simply-Cyan-Circles||sparse"
	"simplyorange|Simply-Orange-Circles|Circles Orange|light;dark||https://github.com/ju1464/Simply_Circles_Icons|HEAD|Simply-Circles-GNOME/Simply-Orange-Circles||sparse"
	"simplypurple|Simply-Purple-Circles|Circles Purple|light;dark||https://github.com/ju1464/Simply_Circles_Icons|HEAD|Simply-Circles-GNOME/Simply-Purple-Circles||sparse"
	"simplyred|Simply-Red-Circles|Circles Red|light;dark||https://github.com/ju1464/Simply_Circles_Icons|HEAD|Simply-Circles-GNOME/Simply-Red-Circles||sparse"
	"simplywhite|Simply-White-Circles|Circles White|dark||https://github.com/ju1464/Simply_Circles_Icons|HEAD|Simply-Circles-GNOME/Simply-White-Circles||sparse"
	"limenumix|Lime-Numix-2021|Numix Lime|light;dark||https://github.com/rtlewis88/rtl88-Themes|MBC-Icon-SuperPack|Lime-Numix-2021||sparse"
	"mblimeglow|MB-Lime-Suru-GLOW|Suru Lime|light;dark||https://github.com/rtlewis88/rtl88-Themes|MBC-Icon-SuperPack|MB-Lime-Suru-GLOW||sparse"
	"mbpistachio|Material-Black-Pistachio-Suru|Suru Pistachio|light;dark||https://github.com/rtlewis88/rtl88-Themes|MBC-Icon-SuperPack|Material-Black-Pistachio-Suru||sparse"
	"aviditydusk|Avidity-Dusk-Mixed-Suru|Suru Dusk|light;dark||https://github.com/rtlewis88/rtl88-Themes|Avidity-Icons-and-Folders|Avidity-Dusk-Mixed-Suru||sparse"
	"ffblackgreen|FF-BlackGreen|Black and Green|light;dark||https://www.opencode.net/felipefacundes/ff-blackgreen|HEAD|icons||full"
	"ffflamengo|FF-Flamengo-RJ-BR|Flamengo|light;dark||https://www.opencode.net/felipefacundes/ff-flamengo-rj-br|HEAD|icons||full"
)

## Widget themes: the whole gtk-3.0 folder, normalised so gtk.css is always at
## gtk-3.0/gtk.css (upstream may only have a gtk-3.20 variant).
##
##		id | outName | style | modes | counterpart | url | ref | subdir | layout
##
##	layout  std    - a gtk-3.x folder with gtk.css and its assets beside it
##	        fluent - upstream keeps light and dark as gtk-Light/gtk-Dark.css
##	                 with the assets one level up

widgetThemes=(
	"fluent|Fluent|Windows 11|light;dark||https://github.com/vinceliuice/Fluent-gtk-theme|HEAD|src/gtk|fluent"
	"win10|Windows-10|Windows 10|light|Windows-10-dark|https://github.com/B00merang-Project/Windows-10|HEAD|.|std"
	"win10dark|Windows-10-dark|Windows 10|dark|Windows-10|https://github.com/B00merang-Project/Windows-10-Dark|HEAD|.|std"
	"win7|Windows-7|Windows 7|light||https://github.com/B00merang-Project/Windows-7|HEAD|.|std"
	"winxp|Windows-XP|Windows XP|light|Windows-XP-dark|https://github.com/B00merang-Project/Windows-XP|HEAD|Windows XP Luna|std"
	"winxpdark|Windows-XP-dark|Windows XP|dark|Windows-XP|https://github.com/B00merang-Project/Windows-XP|HEAD|Windows XP Royale Dark|std"
	"macos|macOS|macOS|light|macOS-dark|https://github.com/B00merang-Project/macOS|HEAD|.|std"
	"macosdark|macOS-dark|macOS|dark|macOS|https://github.com/B00merang-Project/macOS-Dark|HEAD|.|std"
)

## Only these ids, when the caller named some.
wanted=("$@")
fWanted(){
	local id="$1" w
	(( ${#wanted[@]} == 0 )) && return 0
	for w in "${wanted[@]}"; do [[ "$w" == "$id" ]] && return 0; done
	return 1
}

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Clone once per url, shallow, blobless where the repo is huge. Echoes the path.

## The already-cloned test is the checkout itself, not a variable: fClone is
## called through command substitution, so anything it assigns dies with the
## subshell and the same repo would be fetched once per theme that uses it.
##
## @3 is a space separated list of top-level dirs to check out, or empty for the
## whole repo. Papirus alone is 350 MB, and we want a couple of hundred files out
## of it, so a blobless sparse checkout is the difference between seconds and
## minutes - blobs arrive only for what the sparse patterns actually place.
fClone(){
	local url="$1" ref="$2" roots="${3:-}" fetch="${4:-full}" key dest
	## Keyed on the ref as well as the url: rtl88-Themes keeps one theme family
	## per branch, so the same repo is cloned once per branch we want.
	key="$(printf '%s@%s' "$url" "$ref" | tr -c 'A-Za-z0-9' '_')"
	dest="$tmp/$key"

	if [[ -d "$dest/.git" ]]; then
		## Several themes can come out of one sparse checkout - the six Simply
		## Circles colours do - and each names only its own directory, so widen
		## the checkout rather than reusing one that is missing the others.
		if [[ "$fetch" == "sparse" && -n "$roots" && "$roots" != "." ]]; then
			git -C "$dest" sparse-checkout add $roots >/dev/null 2>&1 || true
		fi
		printf '%s' "$dest"; return 0
	fi

	## Every one of these repos has case-only filename pairs, so a checkout here
	## prints a screenful of "paths have collided" that says nothing we do not
	## already handle. Hold the output and show it only if the clone fails.
	local log="$tmp/clone-$key.log"

	if [[ "$fetch" != "sparse" || -z "$roots" || "$roots" == "." ]]; then
		git clone --depth 1 -q "$url" "$dest" > "$log" 2>&1 || { cat "$log" >&2; return 1; }
	else
		git clone --depth 1 --filter=blob:none --sparse -q "$url" "$dest" > "$log" 2>&1 \
			|| { cat "$log" >&2; return 1; }
		## Cone mode, deliberately: --no-cone's gitignore-style patterns place
		## nothing here, and cone mode brings the top-level license files along
		## with the named directories, which is exactly what we want.
		git -C "$dest" sparse-checkout set $roots > "$log" 2>&1 || { cat "$log" >&2; return 1; }
	fi

	if [[ "$ref" != "HEAD" ]]; then
		git -C "$dest" fetch --depth 1 -q origin "$ref" > "$log" 2>&1 || { cat "$log" >&2; return 1; }
		git -C "$dest" checkout -q FETCH_HEAD > "$log" 2>&1 || { cat "$log" >&2; return 1; }
	fi

	## A couple of upstreams commit the icon set as a tar rather than as files -
	## the theme is one blob in a repo that otherwise holds a GTK theme and some
	## screenshots. Unpacking it in place turns it back into an ordinary
	## checkout, which is all the indexer downstream wants.
	while IFS= read -r archive; do
		[[ -n "$archive" ]] || continue
		tar -xf "$archive" -C "$(dirname "$archive")" 2>/dev/null || true
	done < <( find "$dest" -name '*.tar' -type f )

	printf '%s' "$dest"
}

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Index of every candidate file under the search roots, keyed by basename.
##
## Held in an associative array rather than grepped out of a file per lookup:
## a full theme is 26,000 files and 180 lookups, and the inner loop below runs
## once per candidate. Everything from here down is deliberately fork-free -
## the first cut shelled out for the path scoring and took five and a half
## minutes for one theme.

declare -gA iconIndex=()
declare -gA darkIndex=()
declare -gi scored=0
declare -g scoreBias=large

## @3 names the array to fill, so a dark overlay gets an index of its own
## rather than the light one being rebuilt once per icon.
fBuildIndex(){
	local repo="$1" roots="$2" r path base
	local -n target="${3:-iconIndex}"

	target=()
	for r in $roots; do
		[[ -d "$repo/$r" ]] || continue
		while IFS= read -r path; do
			base="${path##*/}"
			target[$base]="${target[$base]:-}${path}"$'\n'
		done < <( cd "$repo" && find "$r" -type f \( -name '*.svg' -o -name '*.png' \) -printf '%p\n' )
	done
}

## Context names differ between upstreams; accept every spelling we have seen.
fContextPattern(){
	case "$1" in
		## A dark overlay is a flat folder of a handful of files with no context
		## directories at all, so there is nothing to match on - and nothing to
		## confuse either, since the names in it are unique.
		any)       echo '' ;;
		mimetypes) echo 'mimetypes|mimes|mime' ;;
		places)    echo 'places|filesystems' ;;
		devices)   echo 'devices|apps/devices' ;;
		emblems)   echo 'emblems|emotes' ;;
		## Adwaita files its widget glyphs under ui/ and its stock ones under
		## legacy/, and puts a few of what we call actions under categories/.
		actions)   echo 'actions|ui|legacy|categories' ;;
		status)    echo 'status|animations|ui' ;;
		*)         echo "$1" ;;
	esac
}

## Rank a candidate path: bigger is better. Prefers real art over an alias, the
## scalable directory over a fixed size, and the largest fixed size otherwise.
## Answers through a global rather than stdout - a command substitution here is
## a fork, and this runs tens of thousands of times.
fScore(){
	local path="$1" size
	scored=0

	## The directory a theme drew the real artwork in outranks everything. The
	## first cut had it the other way round - an alias under links/ was penalised
	## hard enough that a 24px line variant of the same name won, so Fluent, Tela
	## and Qogir all shipped outline folders instead of their real ones. An alias
	## still loses a tie to real art in the same directory, and nothing more.
	[[ "$path" == *.svg ]] && scored=$(( scored + 2000 ))
	[[ "$path" == */scalable/* ]] && scored=$(( scored + 1000 ))
	[[ "$path" == */symbolic/* ]] && scored=$(( scored + 1000 ))
	[[ "$path" == links/* ]] && scored=$(( scored - 20 ))

	## A path component that is a bare number is a fixed icon size. Normally the
	## largest wins; the monochrome fallback below wants the smallest instead,
	## because a 16px glyph is what a toolbar icon was drawn as.
	if [[ "$path" =~ /([0-9]+)(x[0-9]+)?/ ]]; then
		size="${BASH_REMATCH[1]}"
		(( size > 512 )) && size=512
		if [[ "$scoreBias" == "small" ]]; then
			(( size > 32 )) && scored=$(( scored - 200 ))
			scored=$(( scored + (512 - size) / 8 ))
		else
			scored=$(( scored + size / 8 ))
		fi
	fi
}

## Resolve one wanted name to a real file. Echoes an absolute path or nothing.
fResolve(){
	local repo="$1" ctx="$2" file="$3" wantSymbolic="$4"
	local pattern best bestScore path isSymbolic hops=0
	local -n idx="${6:-iconIndex}"

	scoreBias="${5:-large}"

	pattern="$(fContextPattern "$ctx")"

	while :; do
		best=""; bestScore=-9999
		while IFS= read -r path; do
			[[ -n "$path" ]] || continue
			## A monochrome glyph is one in a symbolic/ directory or named
			## <name>-symbolic. Matching "symbolic" anywhere in the path threw
			## away emblem-symbolic-link, which is a colour emblem for a
			## symlink and the one every symlinked file in the view wears.
			isSymbolic=0
			[[ "$path" == */symbolic/* || "${path##*/}" == *-symbolic.* ]] && isSymbolic=1
			[[ "$isSymbolic" == "$wantSymbolic" ]] || continue
			[[ -z "$pattern" ]] || [[ "$path" =~ /($pattern)/ ]] || continue
			fScore "$path"
			if (( scored > bestScore )); then bestScore=$scored; best="$path"; fi
		done <<< "${idx[$file]:-}"

		[[ -n "$best" ]] || return 1

		## An alias: a real symlink, or the text file a Windows checkout leaves.
		if [[ -L "$repo/$best" ]] || fIsLinkStub "$repo/$best"; then
			(( ++hops > 6 )) && return 1
			if [[ -L "$repo/$best" ]] && [[ -f "$repo/$best" ]]; then
				printf '%s' "$repo/$best"; return 0
			fi
			file="$(basename "$(tr -d '[:space:]' < "$repo/$best")")"
			[[ -n "$file" ]] || return 1
			continue
		fi

		printf '%s' "$repo/$best"
		return 0
	done
}

## A checked-out symlink on Windows: one short line naming another file.
fIsLinkStub(){
	local f="$1" size
	[[ -f "$f" ]] || return 1
	size=$(stat -c%s "$f" 2>/dev/null || echo 999999)
	(( size > 0 && size < 256 )) || return 1
	grep -q '<' "$f" && return 1
	[[ "$(tr -d '[:space:]' < "$f")" == *.svg || "$(tr -d '[:space:]' < "$f")" == *.png ]] || return 1
	return 0
}

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Emit index.theme for a finished icon theme directory.

fWriteIconIndex(){
	local dir="$1" name="$2" style="$3" modes="$4" counterpart="$5" inheritExtra="$6"
	local sub dirs=() inherits

	while IFS= read -r sub; do dirs+=("$sub"); done < <(
		cd "$dir" && find . -mindepth 2 -maxdepth 2 -type d -printf '%P\n' | sort
	)
	(( ${#dirs[@]} )) || return 0

	inherits="${inheritExtra:-Adwaita,hicolor}"

	{
		printf '[Icon Theme]\n'
		printf 'Name=%s\n' "$name"
		printf 'Comment=%s style, trimmed for nemo-anywhere\n' "$style"
		printf 'X-Nemo-Style=%s\n' "$style"
		printf 'X-Nemo-Modes=%s\n' "$modes"
		[[ -n "$counterpart" ]] && printf 'X-Nemo-Counterpart=%s\n' "$counterpart"
		printf 'Inherits=%s\n' "$inherits"
		printf 'Example=folder\n'
		printf 'Directories=%s\n\n' "$(IFS=,; printf '%s' "${dirs[*]}")"
		for sub in "${dirs[@]}"; do
			printf '[%s]\n' "$sub"
			printf 'Size=%s\n' "$([[ "$sub" == symbolic/* ]] && echo 16 || echo 48)"
			printf 'MinSize=8\nMaxSize=512\n'
			printf 'Context=%s\n' "$(fIconContext "${sub##*/}")"
			printf 'Type=Scalable\n\n'
		done
	} > "$dir/index.theme"
}

fIconContext(){
	case "$1" in
		places)    echo Places ;;
		mimetypes) echo MimeTypes ;;
		devices)   echo Devices ;;
		emblems)   echo Emblems ;;
		actions)   echo Actions ;;
		status)    echo Status ;;
		*)         echo Applications ;;
	esac
}

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Build one icon theme (and its dark half, when the record names one).

fBuildIconTheme(){
	local record="$1"
	local id outName style modes counterpart url ref roots darkFrom fetch inherits
	IFS='|' read -r id outName style modes counterpart url ref roots darkFrom fetch inherits <<< "$record"

	fWanted "$id" || return 0

	fEcho_Clean ""
	fEcho "Icon theme: $outName ($style)"

	local repo staged darkStaged ctx name out darkOut src darkSrc symbolic
	repo="$(fClone "$url" "$ref" "$roots" "$fetch")"

	fBuildIndex "$repo" "$roots"
	if [[ "$darkFrom" == roots:* ]]; then
		fBuildIndex "$repo" "${darkFrom#roots:}" darkIndex
	else
		darkIndex=()
	fi

	staged="$tmp/stage-$id"
	darkStaged="$tmp/stage-$id-dark"
	rm -rf "$staged" "$darkStaged"

	local found=0 missing=0 darkFound=0 borrowed=0
	while read -r ctx name; do
		[[ -z "$ctx" || "$ctx" == \#* ]] && continue

		symbolic=0
		[[ "$name" == *-symbolic ]] && symbolic=1

		src="$(fResolve "$repo" "$ctx" "$name.svg" "$symbolic" || true)"

		## Themes disagree about which context an icon belongs to - Adwaita
		## files folder-open under status, inode-directory under mimetypes and
		## media-eject under actions. The basename is unique enough on its own,
		## so a context miss retries with the filter off rather than giving up.
		if [[ -z "$src" ]]; then
			src="$(fResolve "$repo" any "$name.svg" "$symbolic" || true)"
		fi

		## Some names were never redrawn as vector - Adwaita's emblems and the
		## whole legacy set are bitmap only. Better a bitmap than a hole.
		if [[ -z "$src" ]]; then
			src="$(fResolve "$repo" "$ctx" "$name.png" "$symbolic" || true)"
		fi
		if [[ -z "$src" ]]; then
			src="$(fResolve "$repo" any "$name.png" "$symbolic" || true)"
		fi

		## Not every theme has a symbolic set - Papirus has none at all, only
		## size buckets - so a miss retries the plain name at a toolbar size.
		## GTK still treats the installed file as symbolic because of how it is
		## named, and a flat 16px glyph in the theme's own colours beats
		## dropping the whole toolbar back to Adwaita.
		if [[ -z "$src" && "$symbolic" == "1" ]]; then
			src="$(fResolve "$repo" "$ctx" "${name%-symbolic}.svg" 0 small || true)"
			[[ -n "$src" ]] && borrowed=$(( borrowed + 1 ))
		fi

		if [[ -z "$src" ]]; then missing=$(( missing + 1 )); continue; fi

		## Keep the source extension: a handful of names exist only as bitmaps
		## upstream, and a png under an .svg name renders as nothing.
		out="$staged/$([[ $symbolic == 1 ]] && echo symbolic || echo scalable)/$ctx/$name.${src##*.}"

		mkdir -p "$(dirname "$out")"
		cp -f "$src" "$out"
		found=$(( found + 1 ))

		## The dark half takes only what upstream actually drew differently.
		if [[ -n "$darkFrom" && -n "$counterpart" ]]; then
			if [[ "$darkFrom" == roots:* ]]; then
				darkSrc="$(fResolve "$repo" any "$name.svg" "$symbolic" large darkIndex || true)"
			else
				darkSrc="$(fResolve "$repo" "$ctx" "$name${darkFrom#suffix:}.svg" "$symbolic" || true)"
			fi
			if [[ -n "$darkSrc" ]]; then
				darkOut="$darkStaged/$([[ $symbolic == 1 ]] && echo symbolic || echo scalable)/$ctx/$name.${darkSrc##*.}"
				mkdir -p "$(dirname "$darkOut")"
				cp -f "$darkSrc" "$darkOut"
				darkFound=$(( darkFound + 1 ))
			fi
		fi
	done < <(grep -vE '^\s*(#|$)' "$nameList")

	(( found > 0 )) || { fEcho "SKIPPED: $outName resolved nothing"; return 0; }

	local dest="$vendor/icons/$outName"
	rm -rf "$dest"; mkdir -p "$dest"
	"$python" "$svgMin" --tree "$staged" "$dest" >/dev/null
	fCopyLicense "$repo" "$dest"
	fWriteIconIndex "$dest" "$outName" "$style" "$modes" "$counterpart" "$inherits"

	fEcho_Clean "    $found icons ($borrowed borrowed for a missing symbolic set), $missing left to the fallback chain, $(fSize "$dest")"

	if (( darkFound > 0 )); then
		local darkDest="$vendor/icons/$counterpart"
		rm -rf "$darkDest"; mkdir -p "$darkDest"
		"$python" "$svgMin" --tree "$darkStaged" "$darkDest" >/dev/null
		fCopyLicense "$repo" "$darkDest"
		fWriteIconIndex "$darkDest" "$counterpart" "$style" "dark" "$outName" "$outName,Adwaita,hicolor"
		fEcho_Clean "    $counterpart: $darkFound tuned icons over $outName, $(fSize "$darkDest")"
	elif [[ -n "$counterpart" ]]; then
		## Nothing was drawn for dark, so the one theme serves both after all.
		fWriteIconIndex "$dest" "$outName" "$style" "light;dark" "" "$inherits"
		fEcho_Clean "    no dark art upstream - $outName covers both modes"
	fi
}

fCopyLicense(){
	local repo="$1" dest="$2" f
	for f in COPYING LICENSE LICENSE.md LICENSE.txt COPYING.md; do
		[[ -f "$repo/$f" ]] && { cp -f "$repo/$f" "$dest/COPYING"; return 0; }
	done
	return 0
}

## The catalog record a finished theme directory came from, by its own name or
## as some other record's counterpart. Echoes "style|url|ref".
fRecordFor(){
	local kind="$1" name="$2" record id outName style modes cp url ref
	local arr=()

	if [[ "$kind" == "themes" ]]; then arr=("${widgetThemes[@]}"); else arr=("${iconThemes[@]}"); fi

	## Two passes, and the order matters: a dark half that has a record of its
	## own must be credited to ITS upstream, not to the repo its light half
	## happens to name it as a counterpart in. Getting this backwards credited
	## macOS-dark and Windows-10-dark to the wrong repositories.
	for record in "${arr[@]}"; do
		IFS='|' read -r id outName style modes cp url ref _rest <<< "$record"
		if [[ "$outName" == "$name" ]]; then
			printf '%s|%s|%s' "$style" "$url" "$ref"
			return 0
		fi
	done

	for record in "${arr[@]}"; do
		IFS='|' read -r id outName style modes cp url ref _rest <<< "$record"
		if [[ "$cp" == "$name" ]]; then
			printf '%s|%s|%s' "$style" "$url" "$ref"
			return 0
		fi
	done

	return 1
}

fSize(){
	find "$1" -type f -printf '%s\n' | awk '{s+=$1;n++} END {printf "%d files, %d KB", n, s/1024}'
}

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Build one widget theme.

fBuildWidgetTheme(){
	local record="$1"
	local id outName style modes counterpart url ref subdir layout
	IFS='|' read -r id outName style modes counterpart url ref subdir layout <<< "$record"

	fWanted "$id" || return 0

	fEcho_Clean ""
	fEcho "Widget theme: $outName ($style)"

	local repo base src dest d
	repo="$(fClone "$url" "$ref")"
	base="$repo/$subdir"
	[[ -d "$base" ]] || { fEcho "SKIPPED: $subdir not in $url"; return 0; }

	dest="$vendor/themes/$outName"
	rm -rf "$dest"; mkdir -p "$dest/gtk-3.0"

	if [[ "$layout" == "fluent" ]]; then
		## 44 rules reference assets/, 14 more assets/scalable/.
		cp -r "$base/assets"   "$dest/gtk-3.0/assets"
		cp -r "$base/scalable" "$dest/gtk-3.0/assets/scalable"
		cp -f "$base/3.0/gtk-Light.css" "$dest/gtk-3.0/gtk.css"
		cp -f "$base/3.0/gtk-Dark.css"  "$dest/gtk-3.0/gtk-dark.css"
	else
		## Prefer the newest gtk-3.x sheet upstream ships; normalise it to gtk-3.0.
		src=""
		for d in gtk-3.24 gtk-3.22 gtk-3.20 gtk-3.0 3.0; do
			if [[ -f "$base/$d/gtk.css" ]]; then src="$base/$d"; break; fi
		done
		[[ -n "$src" ]] || { fEcho "SKIPPED: no gtk.css under $subdir"; rm -rf "$dest"; return 0; }

		## Everything the sheet can reference, and nothing else - no gtk-2.0,
		## metacity, xfwm4, gnome-shell or cinnamon parts, which we never load.
		( cd "$src" && find . -maxdepth 1 -mindepth 1 \
			! -name 'gtk-3.*' ! -name 'thumbnail.png' -exec cp -r {} "$dest/gtk-3.0/" \; )
	fi

	[[ -f "$dest/gtk-3.0/gtk.css" ]] || { fEcho "SKIPPED: $outName produced no gtk.css"; rm -rf "$dest"; return 0; }

	## A theme with its own dark sheet needs no counterpart - GTK swaps for it.
	## Where the catalog names one anyway the pair wins, because that dark half
	## is a theme upstream drew in its own right rather than the light one's
	## variant sheet - and the redundant sheet goes, or both halves claim dark
	## and the picker lists the style twice.
	if [[ -f "$dest/gtk-3.0/gtk-dark.css" ]]; then
		if [[ -n "$counterpart" ]]; then
			rm -f "$dest/gtk-3.0/gtk-dark.css"
		else
			modes="light;dark"
		fi
	fi

	fCopyLicense "$repo" "$dest"

	{
		printf '[Desktop Entry]\n'
		printf 'Type=X-GNOME-Metatheme\n'
		printf 'Name=%s\n' "$outName"
		printf 'Comment=%s style\n' "$style"
		printf 'X-Nemo-Style=%s\n' "$style"
		printf 'X-Nemo-Modes=%s\n' "$modes"
		[[ -n "$counterpart" ]] && printf 'X-Nemo-Counterpart=%s\n' "$counterpart"
		printf '\n[X-GNOME-Metatheme]\n'
		printf 'GtkTheme=%s\n' "$outName"
	} > "$dest/index.theme"

	fEcho_Clean "    $(fSize "$dest")"
}

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••

fEcho "Vendoring bundled themes into $vendor"

for record in "${widgetThemes[@]}"; do fBuildWidgetTheme "$record"; done
for record in "${iconThemes[@]}";   do fBuildIconTheme   "$record"; done

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## Provenance. Only the themes section is rewritten - the rest of the file
## records the other vendored code and its licenses.

if (( ${#wanted[@]} == 0 )); then
	{
		if [[ -f "$vendor/README.md" ]] && grep -q '^## Themes' "$vendor/README.md"; then
			sed '/^## Themes/,$d' "$vendor/README.md"
		else
			printf '# Vendored\n\n'
		fi

		printf '## Themes\n\n'
		printf 'Regenerate with `cicd/utility/vendor-themes.bash` - do not hand-edit. Bundled as\n'
		printf 'mere aggregation: GTK reads them at runtime, nothing is linked into nemo. Each\n'
		printf "theme keeps its own \`COPYING\`. Our own Luna and Aero icon sets are not here -\n"
		printf 'they are first-party art in `assets/icons`, built by `gen-icon-theme.py`.\n\n'
		printf '| Theme | Kind | Style | Upstream | Commit |\n'
		printf '| :-- | :-- | :-- | :-- | :-- |\n'

		## Driven off what is on disk rather than off the catalog, so a dark
		## half that exists only as another record's counterpart still gets a
		## row, and nothing gets two.
		for kind in themes icons; do
			for dir in "$vendor/$kind"/*/; do
				[[ -d "$dir" ]] || continue
				name="$(basename "$dir")"
				meta="$(fRecordFor "$kind" "$name" || true)"
				[[ -n "$meta" ]] || continue
				IFS='|' read -r style url ref <<< "$meta"
				key="$(printf '%s' "$url" | tr -c 'A-Za-z0-9' '_')"
				sha="$(git -C "$tmp/$key" rev-parse HEAD 2>/dev/null || echo "$ref")"
				printf '| `%s` | %s | %s | %s | `%s` |\n' \
					"$name" "$([[ "$kind" == themes ]] && echo Widget || echo Icon)" \
					"$style" "$url" "$sha"
			done
		done
	} > "$vendor/README.md.new"
	mv -f "$vendor/README.md.new" "$vendor/README.md"
fi

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
## The manifest that carries all of this inside the binary. Regenerated here so
## it cannot drift from what is actually in vendor/ - a theme added and not
## listed would simply be missing from the build with nothing to say so.

fEcho_Clean ""
"$python" "$here/gen-theme-resources.py" "$root" | while read -r line; do fEcho_Clean "    $line"; done

fEcho_Clean ""
fEcho "Totals: icons $(fSize "$vendor/icons"), widgets $(fSize "$vendor/themes")"
fEcho "OK -> $vendor"
