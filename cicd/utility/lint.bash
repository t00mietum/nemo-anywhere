#!/usr/bin/env bash

##	- Purpose: the lint stage. Runs every checker in turn and fails on the first
##	  one that reports something.
##	- Each checker decides for itself what to do about a tool that is not
##	  installed, so there is nothing to probe for out here. That matters: while
##	  the stage was gated on cppcheck, a box without it ran no checks at all,
##	  which is how the Bash checker came to run nowhere.
##	- Syntax: lint.bash [base-branch]   (passed through to the C check)

##	Copyright (c) 2026 Bubbles
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

bash "${here}/lint-c.bash" "$@"
bash "${here}/lint-bash.bash"
