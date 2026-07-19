#!/usr/bin/env bash

##	- Purpose: Install the tracked git hooks into this clone's .git/hooks. Run once
##	  per clone (hooks live outside version control). Idempotent.

##	Copyright © 2026 t00mietum (ID: f⍒Ê🝅ĜᛎỹqFẅ▿⍢Ŷ‡ʬẼᛏ🜣)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT


set -Eeuo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
hooksDir="$(git -C "${here}" rev-parse --absolute-git-dir)/hooks"
mkdir -p "${hooksDir}"

for hook in pre-push; do
	src="${here}/${hook}"
	[[ -f "$src" ]] || continue
	install -m 0755 "$src" "${hooksDir}/${hook}"
	echo "installed ${hook} -> ${hooksDir}/${hook}"
done
