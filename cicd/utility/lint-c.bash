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
##	  wherever it sits. A missing python skips it the same way cppcheck does.
##	- Then the settings-handler check (cicd/utility/lint-pref-handlers.py), also
##	  whole-tree, which pairs each disconnect with the connect it belongs to.
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
	allowed+='source/libnemo-private/nemo-cache-db.c '		# a damaged cache file it is replacing
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

## A tree removal walks into a real folder only, never through a symlink or a
## junction. GIO calls a junction a folder on Windows even with NOFOLLOW, so the
## type alone let three of these walks through one until 20260918. Any function
## that lists a folder and removes things has to ask
## nemo_delete_guard_is_real_folder, or be on the list with a reason.
fCheckTreeWalks(){
	local allowed=' '
	allowed+='copy_move_directory '	# walks a link only to copy it; a move through one is turned into a copy
	allowed+='sweep_old_reports '		# its own crash-*.txt files, one level, no walk
	local bad

	bad="$(awk -v allowed="$allowed" '
		FNR == 1 { fn = "" }
		/^[a-zA-Z_][a-zA-Z0-9_]* *\(/ {
			fn = $1; sub(/\(.*/, "", fn)
			walks = 0; removes = 0; gated = 0
		}
		/(nemo_enumerate_children|g_file_enumerate_children|g_dir_open) *\(/ { walks = 1 }
		/(file_delete_wrapper|g_file_delete|g_remove|g_rmdir|g_unlink|delete_file|delete_dir|delete_trash_file|delete_real_tree|remove_target_recursively|nemo_delete_guard_remove_tree) *\(/ { removes = 1 }
		/nemo_delete_guard_is_real_folder/ { gated = 1 }
		/^}/ {
			if (fn != "" && walks && removes && !gated && index(allowed, " " fn " ") == 0)
				print FILENAME ": " fn
			fn = ""
		}
	' source/libnemo-private/*.c source/src/*.c)"

	if [[ -n "$bad" ]]; then
		fEcho "FAIL: lists a folder and removes things without nemo_delete_guard_is_real_folder"
		printf '%s\n' "$bad"
		exit 2
	fi
}
fCheckTreeWalks

## Same rule for the suite, narrowed to the shape that can do the damage: a
## function that calls itself, lists a directory and removes what it finds.
## The fixtures plant links on purpose, so one of those walking into a link
## takes out something the test never made. test-scratch.c owns the one guarded
## walk; everything else goes through it.
fCheckTestTreeWalks(){
	local bad

	bad="$(awk '
		FNR == 1 { fn = "" }
		FILENAME ~ /test-scratch\.c$/ { next }
		/^[a-zA-Z_][a-zA-Z0-9_]* *\(/ {
			fn = $1; sub(/\(.*/, "", fn)
			walks = 0; removes = 0; recurses = 0; defline = 1
		}
		/(nemo_enumerate_children|g_file_enumerate_children|g_dir_open) *\(/ { walks = 1 }
		/(g_file_delete|g_remove|g_rmdir|g_unlink) *\(/ { removes = 1 }
		fn != "" && !defline && (index($0, fn "(") > 0 || index($0, fn " (") > 0) { recurses = 1 }
		/^}/ {
			if (fn != "" && walks && removes && recurses)
				print FILENAME ": " fn
			fn = ""
		}
		{ defline = 0 }
	' source/test/*.c)"

	if [[ -n "$bad" ]]; then
		fEcho "FAIL: a test rolls its own tree removal - use test_scratch_remove_tree"
		printf '%s\n' "$bad"
		exit 2
	fi
}
fCheckTestTreeWalks

## Two test helpers that used to be copied instead of shared. The check macro
## was in the tree in two spellings across sixty-odd files, and twenty-odd
## tests each set the same three environment variables by hand to get a
## throwaway config root. A fresh copy of either drifts from the rest.
fCheckTestHelpers(){
	local bad

	bad="$(grep -ln '^#define check' source/test/*.c || true)"
	if [[ -n "$bad" ]]; then
		fEcho "FAIL: a test defines its own check macro - include test-check.h"
		printf '%s\n' "$bad"
		exit 2
	fi

	## test-scratch.c is the helper. test-nemo-config-root.c is the one test
	## about how the config root is picked, so it has to point the variables
	## at separate directories itself.
	bad="$(grep -ln 'g_setenv ("XDG_CONFIG_HOME"' source/test/*.c | grep -vE '(test-scratch|test-nemo-config-root)\.c$' || true)"
	if [[ -n "$bad" ]]; then
		fEcho "FAIL: a test points the config root by hand - use test_scratch_config_home"
		printf '%s\n' "$bad"
		exit 2
	fi
}
fCheckTestHelpers

## Row shading remembers which renderers it has already told they have no
## background, so it can skip saying it again on every redraw. That is only
## safe while cell_set_plain is the one place the property is turned off. A
## second writer would leave the memory wrong and the shading with it, quietly.
fCheckCellPlain(){
	local src='source/src/nemo-list-view.c'
	local n

	[[ -f "$src" ]] || return 0

	n="$(grep -c -F '"cell-background-set"' "$src" || true)"
	if [[ "$n" != 1 ]]; then
		fEcho "FAIL: ${src}: ${n} writes of cell-background-set, expected the 1 in cell_set_plain"
		grep -n -F '"cell-background-set"' "$src" || true
		exit 2
	fi

	n="$(grep -rl -F '"cell-background' source/src source/libnemo-private source/eel | grep -cv 'nemo-list-view.c' || true)"
	if [[ "$n" != 0 ]]; then
		fEcho "FAIL: cell-background is written outside nemo-list-view.c"
		grep -rn -F '"cell-background' source/src source/libnemo-private source/eel | grep -v 'nemo-list-view.c' || true
		exit 2
	fi
}
fCheckCellPlain

## The list view works out every column width itself and hands out widths that
## come to exactly the row. An expanding column lets GTK add more on top, from a
## share it worked out while the view was wider, and it will not give that back
## until some width really changes - so the row scrolls sideways while fitting.
## The places sidebar has its own tree view and is not covered by any of this.
fCheckColumnExpand(){
	local src='source/src/nemo-list-view.c'

	[[ -f "$src" ]] || return 0

	if grep -n -F 'gtk_tree_view_column_set_expand' "$src"; then
		fEcho "FAIL: ${src}: no column expands; the layout owns the widths"
		exit 2
	fi
}
fCheckColumnExpand

## The list view runs in fixed-height mode, which halves what a big folder
## costs to load. GTK only allows it while every column sizes FIXED, and it
## goes wrong quietly rather than loudly: a column left to size itself makes
## the tree view measure every row again, which is the cost being avoided.
## So the two are pinned together.
fCheckFixedHeight(){
	local src='source/src/nemo-list-view.c'
	local other

	[[ -f "$src" ]] || return 0

	if ! grep -q -F 'gtk_tree_view_set_fixed_height_mode' "$src"; then
		fEcho "FAIL: ${src}: the list view must ask for fixed-height mode"
		exit 2
	fi

	## The call wraps in this file, so read it up to its semicolon rather than
	## a line at a time.
	other="$(awk '
		/gtk_tree_view_column_set_sizing/ { open=1; at=NR; text="" }
		open { text = text $0 }
		open && /;/ {
			if (text !~ /GTK_TREE_VIEW_COLUMN_FIXED/) print at ": " text
			open=0
		}' "$src")"
	if [[ -n "$other" ]]; then
		echo "$other"
		fEcho "FAIL: ${src}: every column sizes FIXED, or fixed-height mode is unsafe"
		exit 2
	fi
}
fCheckFixedHeight

## The measuring cache in the list view. Each rule below is what keeps a
## remembered width honest; without one the columns come out wrong or the
## cache grows per row.
fCheckMeasureCache(){
	local src='source/src/nemo-list-view.c'
	local body

	[[ -f "$src" ]] || return 0
	grep -q -F 'samples->measured' "$src" || return 0

	body="$(awk '/^row_width_for_column/ { on=1 } on { print } on && /^}/ { exit }' "$src")"

	## Bold and light rows lay out wider or narrower at the same text.
	if ! grep -q -F 'weight == NORMAL_TEXT_WEIGHT' <<< "$body"; then
		fEcho "FAIL: ${src}: only a normal-weight cell may reuse a remembered width"
		exit 2
	fi

	## Thrown away with the samples, or a zoom leaves widths from the old font.
	if ! awk '/^column_samples_free/ { on=1 } on { print } on && /^}/ { exit }' "$src" |
	     grep -q -F 'samples->measured'; then
		fEcho "FAIL: ${src}: the remembered widths must go when the samples do"
		exit 2
	fi

	## A date is nearly all distinct values, so the table needs a ceiling.
	if ! grep -q -F 'MEASURED_TEXTS_MAX' <<< "$body"; then
		fEcho "FAIL: ${src}: remembering a width must stop at MEASURED_TEXTS_MAX"
		exit 2
	fi
}
fCheckMeasureCache

## A theme change can bring a new font, which makes every remembered width
## wrong. The handler has to send the rows back to be measured - but only on a
## change that moved something, since style-updated also fires for a state or
## a CSS class and 50,000 rows is not free.
fCheckStyleRemeasure(){
	local src='source/src/nemo-list-view.c'
	local body

	[[ -f "$src" ]] || return 0

	body="$(awk '/^tree_view_style_updated/ { on=1 } on { print } on && /^}/ { exit }' "$src")"

	if ! grep -q -F 'remeasure_rows' <<< "$body"; then
		fEcho "FAIL: ${src}: a theme change must send the rows back to be measured"
		exit 2
	fi

	if ! grep -q -F 'measure_style_id' <<< "$body"; then
		fEcho "FAIL: ${src}: remeasure only where the font or the theme sizes moved"
		exit 2
	fi
}
fCheckStyleRemeasure

## The zoom slider is the last thing in the status bar, so it needs a margin of
## its own or the trough runs into the window edge.
fCheckSliderMargin(){
	local src='source/src/nemo-statusbar.c'

	[[ -f "$src" ]] || return 0

	if ! grep -q -F 'gtk_widget_set_margin_end (GTK_WIDGET (zoom_slider)' "$src"; then
		fEcho "FAIL: ${src}: the zoom slider needs a margin at the window edge"
		exit 2
	fi
}
fCheckSliderMargin

## Sizing a folder of images reads: either the folder's own image size, which is
## already stored, or the image default, which has to stay a default so the
## preference can still move it. Writing here would pin a folder at whatever the
## setting said the first time it was opened, and in a window that is not
## remembering per folder it would follow you into the next folder.
fCheckImageDefault(){
	local src='source/src/nemo-icon-view.c'
	local body

	[[ -f "$src" ]] || return 0

	body="$(awk '/^(size_for_mostly_images|update_mostly_images)/ { on=1 } on { print } on && /^}/ { on=0 }' "$src")"

	if ! grep -q -F 'nemo_directory_is_mostly_images' <<< "$body"; then
		fEcho "FAIL: ${src}: update_mostly_images must ask what is in the folder"
		exit 2
	fi

	if grep -qE '(^|[[:space:](])set_icon_size \(|nemo_folder_settings_set|nemo_window_set_ignore_meta_icon_size' <<< "$body"; then
		fEcho "FAIL: ${src}: sizing a folder of images must not write anything back"
		exit 2
	fi
}
fCheckImageDefault

## A checksum is kept on a file in three attributes, and the order they are
## written in is the only thing standing between a torn write and a checksum
## that vouches for contents it has never seen. Reading requires the size and
## the time to match, so the time has to be written last: then a write that
## stops part way leaves the old time next to the new checksum, and the reader
## throws it away. Write the time first and a half-finished write leaves the
## old checksum under a size and time that both match, which no reader can
## catch. No test can hold this - the bad state is one a read cannot tell from
## a good one.
fCheckDigestAttrOrder(){
	local src='source/libnemo-private/nemo-file-digest.c'
	local body order

	[[ -f "$src" ]] || return 0

	body="$(awk '/^nemo_file_digest_write_attr/ { on=1 } on { print } on && /^}/ { on=0 }' "$src")"

	order="$(grep -o 'nemo_file_xattr_set (file, ATTR_[A-Z]*' <<< "$body" | sed 's/.*ATTR_//' | tr '\n' ' ')"

	if [[ "${order}" != "DIGEST BYTES MTIME " ]]; then
		fEcho "FAIL: ${src}: the checksum attributes must be written DIGEST, BYTES, MTIME (got: ${order:-none})"
		exit 2
	fi
}
fCheckDigestAttrOrder

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
	## Same reasoning: the demo recorder's settings keys and columns go stale
	## silently, and nothing C has to change for that to happen.
	"$PY" cicd/utility/lint-demo-script.py .
	## Whole-tree too: a disconnect aimed at the wrong preference group removes
	## nothing and says nothing, and the handler then runs on a freed object.
	"$PY" cicd/utility/lint-pref-handlers.py source
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

fEcho "C lint (cppcheck, check-only) over ${#files[@]} changed file(s)..."
## The suppression list is in .cppcheck-suppressions at the repo root, with the
## reason for each one written beside it. --inline-suppr stays for the handful
## that belong to a single line rather than a whole file.
cppcheck --enable=warning,portability --library=gtk --inline-suppr \
	--suppressions-list=.cppcheck-suppressions \
	--quiet --error-exitcode=2 "${files[@]}"
fEcho "OK: C lint: no findings"
