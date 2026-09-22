#!/usr/bin/env bash

##	- Purpose: the lint stage. Runs every checker in turn and fails on the first
##	  one that reports something.
##	- Each checker decides for itself what to do about a tool that is not
##	  installed, so there is nothing to probe for out here. That matters: while
##	  the stage was gated on cppcheck, a box without it ran no checks at all,
##	  which is how the Bash checker came to run nowhere.
##	- vendor-themes.bash's self-test rides along here rather than in the test
##	  stage: it needs git, which the build container does not have, and it runs
##	  in well under a second.
##	- Syntax: lint.bash [base-branch]   (passed through to the C check)

##	Copyright (c) 2026 Bubbles
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

bash "${here}/lint-c.bash" "$@"
bash "${here}/lint-bash.bash"
bash "${here}/lint-python.bash"
bash "${here}/lint-powershell.bash"
bash "${here}/lint-identity.bash"
bash "${here}/lint-prose.bash"
bash "${here}/vendor-themes.bash" --self-test

## The app icons are cut from assets/logo.png by hand, so a new logo can sit there
## with the old icons still shipping. Needs Pillow, which not every box has.
if python3 -c 'import PIL' 2>/dev/null; then
	python3 "${here}/gen-app-icon.py" --check
else
	echo "[ app icon check skipped: no Pillow ]"
fi

## The content scrub is kept outside the repo, so a fresh clone without the
## private tree still lints.
scrub="$(cd "${here}/../.." && pwd)/../private/hooks/scrub.bash"
[[ -x "$scrub" ]] && bash "$scrub"

exit 0
