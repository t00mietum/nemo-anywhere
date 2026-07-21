#!/usr/bin/env bash

#  shellcheck disable=2086  ## 'Double quote to prevent globbing and word splitting.' (OK for the dep word-lists.)

##	- Purpose: Assemble a mingw-w64 GTK3 sysroot for cross-building the Windows target.
##	  Resolves the transitive dependency closure of a few root packages from the MSYS2
##	  pacman database, then downloads and unpacks each prebuilt package into the sysroot.
##	- No pacman needed: we parse the .db (a tarball of 'desc' files) ourselves and pull
##	  the .pkg.tar.zst files straight from the mirror. Keeps the whole GTK3 Windows stack
##	  out of both the host and the repo - it lives only inside the build image.
##	- Syntax: fetch-sysroot.bash [SYSROOT_DIR]   (default /opt/win-sysroot)
##	  Result layout: $SYSROOT/mingw64/{include,lib,bin,...}; .pc files keep prefix=/mingw64,
##	  so the meson cross file points PKG_CONFIG_SYSROOT_DIR at $SYSROOT.

set -euo pipefail

MIRROR="https://mirror.msys2.org/mingw/mingw64"
SYSROOT="${1:-/opt/win-sysroot}"
PFX="mingw-w64-x86_64-"

##	Roots: gtk3 drags in the bulk of the stack (glib2, atk, cairo, gdk-pixbuf2, pango,
##	json-glib, libepoxy, gettext, icon theme). The rest are our extra meson deps.
##	Exempi (XMP) is not packaged for mingw - we cross-build with -Dxmp=false instead.
ROOTS=(gtk3 json-glib libexif libgsf)

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

fEcho(){ echo "[ $* ]"; }

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
#	Pull + unpack the pacman database
#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••

fEcho "Fetching package database"
curl -fsSL "$MIRROR/mingw64.db" -o "$work/db.tar.gz"
mkdir -p "$work/db"
tar -xf "$work/db.tar.gz" -C "$work/db"

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
#	Parse every 'desc' into name -> filename, name -> depends, provides -> name.
#	One gawk pass over all desc files (BEGINFILE/ENDFILE) - version constraints stripped.
#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••

fEcho "Indexing packages"
declare -A FILEOF DEPSOF PROVIDER
while IFS='|' read -r kind a b; do
	case "$kind" in
		F) FILEOF[$a]=$b ;;
		D) DEPSOF[$a]=$b ;;
		V) PROVIDER[$a]=$b ;;
	esac
done < <(gawk '
	BEGINFILE { name=""; file=""; deps=""; provs=""; sec="" }
	/^%FILENAME%$/ { sec="F"; next }
	/^%NAME%$/     { sec="N"; next }
	/^%DEPENDS%$/  { sec="D"; next }
	/^%PROVIDES%$/ { sec="V"; next }
	/^%/           { sec="";  next }
	sec=="F" && NF { file=$0 }
	sec=="N" && NF { name=$0 }
	sec=="D" && NF { s=$0; sub(/[<>=].*/,"",s); deps=deps" "s }
	sec=="V" && NF { s=$0; sub(/[<>=].*/,"",s); provs=provs" "s }
	ENDFILE {
		if (name!="") {
			print "F|" name "|" file
			print "D|" name "|" deps
			n=split(provs,pv," "); for(i=1;i<=n;i++) print "V|" pv[i] "|" name
		}
	}
' "$work"/db/*/desc)

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
#	Breadth-first closure from the roots. Unknown names resolve via PROVIDES.
#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••

declare -A SEEN
queue=()
for r in "${ROOTS[@]}"; do queue+=("$PFX$r"); done

resolved=()
while [[ ${#queue[@]} -gt 0 ]]; do
	pkg="${queue[0]}"; queue=("${queue[@]:1}")
	[[ -n "${SEEN[$pkg]:-}" ]] && continue
	real="$pkg"
	[[ -z "${FILEOF[$real]:-}" && -n "${PROVIDER[$pkg]:-}" ]] && real="${PROVIDER[$pkg]}"
	if [[ -z "${FILEOF[$real]:-}" ]]; then
		echo "  warn: no package for '$pkg' (skipped)" >&2
		continue
	fi
	SEEN[$pkg]=1; SEEN[$real]=1
	resolved+=("$real")
	for d in ${DEPSOF[$real]:-}; do
		[[ -n "${SEEN[$d]:-}" ]] || queue+=("$d")
	done
done

fEcho "Closure: ${#resolved[@]} packages"

#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••
#	Download + unpack each into the sysroot (skip pacman metadata members).
#•••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••

mkdir -p "$SYSROOT"
for pkg in "${resolved[@]}"; do
	file="${FILEOF[$pkg]}"
	fEcho "$pkg"
	curl -fsSL "$MIRROR/$file" -o "$work/$file"
	tar --zstd -xf "$work/$file" -C "$SYSROOT" \
		--exclude='.PKGINFO' --exclude='.BUILDINFO' --exclude='.MTREE' --exclude='.INSTALL'
	rm -f "$work/$file"
done

fEcho "Sysroot ready: $SYSROOT ($(du -sh "$SYSROOT" | cut -f1))"
