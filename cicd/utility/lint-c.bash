#!/usr/bin/env bash

##	- Purpose: Check-only C lint over the CHANGED .c/.h files, never the whole
##	  inherited tree (upstream nemo has no house style, and a full-tree pass
##	  would drown real findings in legacy noise). Nothing is rewritten.
##	- Changed = commits since the merge base with the integration branch (dev,
##	  else main) plus anything uncommitted (tracked, staged, untracked). On the
##	  integration branch itself only the uncommitted changes are checked.
##	- cppcheck findings (error/warning/portability) fail the gate. A missing
##	  cppcheck skips with a warning so an unprovisioned box can't hard-block a
##	  push; CPPCHECK_STRICT=1 turns that miss into a hard failure.
##	- Then the UI-case check (cicd/utility/lint-ui-case.py), which is whole-tree
##	  rather than diff-scoped: the tree is already clean, so there is no legacy
##	  noise to drown in, and a Title Case label pasted from upstream is caught
##	  wherever it lands. A missing python skips it the same way cppcheck does.
##	- Runs the same everywhere bash + git + cppcheck exist (Linux host, MSYS2).
##	- Syntax: lint-c.bash [base-branch]

##	Copyright (c) 2026 Bubbles
##	Licensed under The MIT License (MIT). Full text at:
##		https://mit-license.org/
##	SPDX-License-Identifier: MIT

set -Eeuo pipefail

base="${1:-}"
strict="${CPPCHECK_STRICT:-0}"
cd "$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

fEcho(){ echo "[ $* ]"; }

## Every delete in nemo-file-operations.c goes through file_delete_wrapper, so
## the delete guard sees it. Two calls in make_link_copy sat outside it until
## 20260917 and were reachable with no guard at all. Whole-tree, since the rule
## is about that one file whether or not this change touched it.
fCheckDeleteWrapper(){
	local src='source/libnemo-private/nemo-file-operations.c'
	local n

	[[ -f "$src" ]] || return 0

	n="$(grep -c -F 'g_file_delete (' "$src" || true)"
	if [[ "$n" != 1 ]]; then
		fEcho "FAIL: ${src}: ${n} g_file_delete calls, expected the 1 in file_delete_wrapper"
		grep -n -F 'g_file_delete (' "$src" || true
		exit 2
	fi
}
fCheckDeleteWrapper

## G_FILE_COPY_OVERWRITE destroys the target inside glib, where there is nothing
## of ours to hook, so the test guard has to ask before the flag goes on. Both
## uses sit a few lines after the ask at their retry label; anything further
## away has grown a path that reaches the flag without asking. Whole-tree, same
## reason as above.
fCheckOverwriteAsk(){
	local src='source/libnemo-private/nemo-file-operations.c'
	local bad

	[[ -f "$src" ]] || return 0

	bad="$(awk '
		/testguard_allows_overwrite \(/ { asked = NR }
		/G_FILE_COPY_OVERWRITE/ {
			if (NR - asked > 15) print NR ": " $0
		}
	' "$src")"

	if [[ -n "$bad" ]]; then
		fEcho "FAIL: ${src}: G_FILE_COPY_OVERWRITE with no test guard ask above it"
		printf '%s\n' "$bad"
		exit 2
	fi
}
fCheckOverwriteAsk

## Nothing may trash, delete or move a person's files unless it started in a
## window. Another program must have no way in: the bus exported CopyURIs,
## MoveURIs and EmptyTrash, left over from the Nemo desktop, until 20260917.
## These four checks hold that. A new entry on any of their lists wants a
## reason, and whoever adds one should be able to say what window starts it.

## Bus methods: the freedesktop file manager interface only, none of which
## touches a file.
fCheckBusMethods(){
	local allowed=' ShowFolders ShowItems ShowItemProperties '
	local name bad=""

	while read -r name; do
		[[ -z "$name" ]] && continue
		[[ "$allowed" == *" ${name} "* ]] || bad+="${name} "
	done <<< "$(grep -rhoE "<method name=['\"][A-Za-z0-9_]+" source | sed -E "s/.*=['\"]//" | sort -u || true)"

	if [[ -n "$bad" ]]; then
		fEcho "FAIL: bus methods not on the list in lint-c.bash: ${bad% }"
		grep -rnE "<method name=['\"]" source || true
		exit 2
	fi
}
fCheckBusMethods

## Application actions are exported on the bus too. Only quit.
fCheckAppActions(){
	local hits

	hits="$(grep -rnE 'g_simple_action_new|GActionEntry|g_action_map_add_action_entries' \
		source/src source/libnemo-private source/eel 2>/dev/null \
		| grep -v -F 'g_simple_action_new ("quit"' || true)"
	if [[ -n "$hits" ]]; then
		fEcho "FAIL: a new application action; another program can call it over the bus"
		printf '%s\n' "$hits"
		exit 2
	fi
}
fCheckAppActions

## Who may start a trash, delete, empty trash or move job. Each file here is
## reached from something done in a window.
fCheckJobCallers(){
	local allowed=' '
	allowed+='source/src/nemo-view.c '				# trash and delete commands, drops, paste
	allowed+='source/src/nemo-tree-sidebar.c '		# trash and delete commands in the tree
	allowed+='source/src/nemo-places-sidebar.c '		# empty trash, drops
	allowed+='source/src/nemo-trash-bar.c '			# empty trash button
	allowed+='source/src/nemo-mime-actions.c '			# broken link dialog
	allowed+='source/src/nemo-template-config-widget.c '	# remove button in preferences
	allowed+='source/libnemo-private/nemo-archive.c '	# compress dialog, delete originals
	allowed+='source/libnemo-private/nemo-file-undo-operations.c '	# undo
	allowed+='source/libnemo-private/nemo-dnd-win32.c '	# a move dragged out of a window
	local file bad=""

	while read -r file; do
		[[ -z "$file" ]] && continue
		[[ "$allowed" == *" ${file} "* ]] || bad+="${file} "
	done <<< "$(grep -rlE 'nemo_file_operations_(trash_or_delete|delete|empty_trash|move|copy_move)(_by_user)?[[:space:]]*\(' \
		source --include='*.c' \
		| grep -v -e '^source/test/' -e '^source/libnemo-private/nemo-file-operations\.c$' || true)"

	if [[ -n "$bad" ]]; then
		fEcho "FAIL: trash, delete or move started from a file not on the list in lint-c.bash: ${bad% }"
		exit 2
	fi
}
fCheckJobCallers

## Raw deletes outside the jobs. What is left only touches files the app made
## for itself.
fCheckRawDeletes(){
	local allowed=' '
	allowed+='source/libnemo-private/nemo-file-operations.c '	# the jobs
	allowed+='source/libnemo-private/nemo-delete-guard.c '		# the guarded delete the jobs use
	allowed+='source/libnemo-private/nemo-trash-win32.c '		# the recycle bin, behind the jobs
	allowed+='source/libnemo-private/nemo-link-win32.c '		# its own probe, and a link it failed to finish
	allowed+='source/libnemo-private/nemo-archive.c '			# an archive it was writing
	allowed+='source/libnemo-private/nemo-crash.c '			# old crash reports
	allowed+='source/libnemo-private/nemo-thumbnail-prune.c '	# thumbnail cache
	allowed+='source/libnemo-private/nemo-desktop-thumbnail.c '	# thumbnail cache
	allowed+='source/libnemo-private/nemo-file.c '			# thumbnail cache
	allowed+='source/src/nemo-bookmark-list.c '			# --reset
	allowed+='source/src/nemo-main-application.c '			# --reset
	local file bad=""

	while read -r file; do
		[[ -z "$file" ]] && continue
		[[ "$allowed" == *" ${file} "* ]] || bad+="${file} "
	done <<< "$(grep -rlE '(^|[^>.[:alnum:]_])(g_file_delete|g_file_delete_async|g_file_trash|g_file_trash_async|g_unlink|g_remove|g_rmdir|unlink|rmdir|remove|_wunlink|_wremove|_wrmdir|DeleteFileW|DeleteFileA|RemoveDirectoryW|RemoveDirectoryA|SHFileOperationW|SHEmptyRecycleBinW)[[:space:]]*\(' \
		source/src source/libnemo-private source/libnemo-extension source/eel --include='*.c' || true)"

	if [[ -n "$bad" ]]; then
		fEcho "FAIL: a raw delete in a file not on the list in lint-c.bash: ${bad% }"
		exit 2
	fi
}
fCheckRawDeletes

## Under MSYS2, use the Windows git that made this checkout - the msys one has
## its own HOME/config, so its line-ending view marks every CRLF file modified.
GIT=(git)
if [[ "$(uname -o 2>/dev/null)" == "Msys" ]]; then
	for cand in "/c/Program Files/Git/cmd/git.exe" "/c/Program Files (x86)/Git/cmd/git.exe"; do
		[[ -x "$cand" ]] && { GIT=("$cand"); break; }
	done
fi
## Read-only use; keep eol-normalization advice out of the gate output.
GIT+=(-c core.safecrlf=false)

## UI case first, and unconditionally: the checks below bail early when nothing
## C changed, and a label is just as wrong on a .glade-only change.
PY=""
for cand in python3 python; do
	command -v "$cand" >/dev/null 2>&1 && { PY="$cand"; break; }
done
if [[ -n "$PY" ]]; then
	"$PY" cicd/utility/lint-ui-case.py source
else
	fEcho "WARNING: UI case SKIPPED: no python" >&2
fi

if ! command -v cppcheck >/dev/null 2>&1; then
	if [[ "$strict" == "1" ]]; then
		fEcho "FAILED: C lint: cppcheck not installed" >&2
		exit 1
	fi
	fEcho "WARNING: C lint SKIPPED: cppcheck not installed" >&2
	exit 0
fi

## Integration branch: explicit arg wins, else dev if it exists, else main.
if [[ -z "$base" ]]; then
	if "${GIT[@]}" show-ref --verify --quiet refs/heads/dev; then base="dev"; else base="main"; fi
fi

## Collect candidates: branch commits since the merge base (skipped when we ARE
## the base), then everything not yet committed. Deletions can't be linted.
candidates=""
head_branch="$("${GIT[@]}" rev-parse --abbrev-ref HEAD)"
if [[ "$head_branch" != "$base" ]] && "${GIT[@]}" rev-parse --verify --quiet "$base" >/dev/null; then
	candidates+="$("${GIT[@]}" diff --name-only --diff-filter=d "${base}...HEAD")"$'\n'
fi
candidates+="$("${GIT[@]}" diff --name-only --diff-filter=d HEAD)"$'\n'
candidates+="$("${GIT[@]}" diff --cached --name-only --diff-filter=d)"$'\n'
candidates+="$("${GIT[@]}" ls-files --others --exclude-standard)"

## Keep C sources that still exist, dedup. Vendored code is upstream's to
## fix, so bumping it must not light up our gate.
files=()
while IFS= read -r f; do
	[[ "$f" == *.c || "$f" == *.h ]] || continue
	[[ "$f" == vendor/* ]] && continue
	[[ -f "$f" ]] && files+=("$f")
done < <(printf '%s\n' "$candidates" | LC_ALL=C sort -u)

if ((${#files[@]} == 0)); then
	fEcho "OK: C lint: no changed C files"
	exit 0
fi

## assertWithSideEffect misfires on the idiomatic g_assert(g_hash_table_...)
## pattern all over this codebase - not worth per-site suppressions.
## nullPointerOutOfMemory (and its cross-TU twin ctunullpointerOutOfMemory)
## assume an allocator can return NULL; glib's abort instead, so every one of
## these is wrong by construction here.
## normalCheckLevelMaxBranches just says a big file was analyzed shallowly.
## It is not a finding, but --error-exitcode counts it, so any change touching
## a large file would fail the gate on it alone.
## The per-file entries below are inherited-legacy findings, each confirmed
## present on dev - they surfaced only because a sweep touched those files.
## unknownMacro: cppcheck can't expand the EEL self-check X-macro prototype
## (nemo-lib-self-check-functions.h), so it fires for any .c that includes it.
## The two nemo-dnd.c items are inherited-legacy noise in the gnome-icon-list
## drag encoder/parser, surfaced only because a change touched that big file.
## The nemo-mime-actions.c trio is the same story in the activation code path.
## nemo-window-bookmarks.c and nemo-file-undo-operations.c joined that list when
## a label sweep touched them: both were confirmed present on dev first, and the
## undo-operations pair only shows at all in a whole-program run, never when the
## file is linted on its own.
fEcho "C lint (cppcheck, check-only) over ${#files[@]} changed file(s)..."
cppcheck --enable=warning,portability --library=gtk --inline-suppr \
	--suppress=missingInclude --suppress=assertWithSideEffect \
	--suppress=unknownMacro \
	--suppress=nullPointerOutOfMemory \
	--suppress=ctunullpointerOutOfMemory \
	--suppress=normalCheckLevelMaxBranches \
	--suppress=invalidPrintfArgType_uint:*nemo-dnd.c \
	--suppress=nullPointerRedundantCheck:*nemo-dnd.c \
	--suppress=CastAddressToIntegerAtReturn:*nemo-mime-actions.c \
	--suppress=uselessAssignmentPtrArg:*nemo-mime-actions.c \
	--suppress=nullPointerRedundantCheck:*nemo-mime-actions.c \
	--suppress=memleak:*nemo-thumbnails.c \
	--suppress=leakNoVarFunctionCall:*nemo-thumbnails.c \
	--suppress=memleak:*nemo-query-editor.c \
	--suppress=memleak:*nemo-window-slot.c \
	--suppress=deallocuse:*nemo-properties-window.c \
	--suppress=deallocuse:*nemo-icon-view.c \
	--suppress=nullPointerRedundantCheck:*nemo-icon-view-container.c \
	--suppress=nullPointer:*nemo-tree-sidebar.c \
	--suppress=ctunullpointer:*nemo-tree-sidebar.c \
	--suppress=invalidPrintfArgType_sint:*nemo-icon-canvas-item.c \
	--suppress=invalidPrintfArgType_sint:*nemo-properties-window.c \
	--suppress=invalidPrintfArgType_sint:*nemo-window-bookmarks.c \
	--suppress=memleak:*nemo-file-undo-operations.c \
	--quiet --error-exitcode=2 "${files[@]}"
fEcho "OK: C lint: no findings"
