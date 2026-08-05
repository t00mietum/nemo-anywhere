#!/usr/bin/env bash

##	- Purpose: The pipeline's output helpers, in one place. Several scripts used
##	  to carry their own one-line copy of fEcho_Clean, which lost the repeated-
##	  blank collapsing and made their section spacing differ from everything else.
##	- fEcho "msg"       -> a bracketed "[ msg ]" status line
##	- fEcho_Clean "msg" -> a plain line; a bare call emits one blank at most,
##	                       however many times it is called in a row
##	- fSection "title"  -> blank line, full-width rule, then the title
##	- fDie "msg"        -> a fatal line on stderr, then exit 1
##	- Syntax: source this file; it defines functions only.

##	Copyright © 2026 Bubbles (ID: XଌฅრX۳ᛟԃლፀƅꓩหδლც)
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT


declare -i _wasLastEchoBlank=0

fEcho_ResetBlankCounter(){ _wasLastEchoBlank=0; }
fEcho_Clean(){ if [[ -n "${1:-}" ]]; then echo -e "$*"; _wasLastEchoBlank=0; elif [[ $_wasLastEchoBlank -eq 0 ]] && echo; then _wasLastEchoBlank=1; fi; }
fEcho(){       if [[ -n "$*"     ]]; then fEcho_Clean "[ $* ]"; else fEcho_Clean ""; fi; }
fEcho_Force(){ fEcho_ResetBlankCounter; fEcho "$*"; }

_letterbox="••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••••"
fSection(){ fEcho_Clean; fEcho_Clean "${_letterbox}"; fEcho "$*"; }
fDie(){ { fEcho_Force "FAILED: $*"; } >&2; exit 1; }


##	History:
##		- 2026-08-04: Split out of cicd.bash so the helper scripts stop
##		  redefining a lesser version of it.
