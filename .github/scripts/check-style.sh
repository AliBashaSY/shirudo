#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# .github/scripts/check-style.sh -- run the kernel's checkpatch.pl, in strict
# mode, over the C sources a commit touches and over its message, the way
# cilium checks its BPF tree, plus the block-comment opener that checkpatch
# leaves to the netdev trees. The kernel checks that have no meaning in
# userspace are switched off below, each with its reason.
#
#   .github/scripts/check-style.sh [<rev-list arguments>]
#   .github/scripts/check-style.sh --diff
#   .github/scripts/check-style.sh --files [<file>...]
#   .github/scripts/check-style.sh --types
#
# The first form checks commits: the arguments go to git rev-list, a range
# such as origin/main..HEAD or -3 HEAD, and the commit at HEAD alone
# without any. --diff checks the uncommitted changes against HEAD and
# --files whole files, every C source the project writes by default.
# --types lists the checks that stay enabled.
#
# checkpatch.pl is the kernel's and stays out of the tree: CHECKPATCH names
# a copy to use, a kernel tree's scripts/checkpatch.pl for instance, and
# otherwise the pinned release below is fetched into build/checkpatch/ and
# verified before use.

set -eu

KERNEL_TAG=v7.2
CHECKPATCH_PL_SHA256=f8f32e6b559004fcac021e2f6149b6b7ea486924a8bc48297c0f317eddf5e220
SPELLING_SHA256=4095d4a8810f115bae1b7c0d8a1946beb3435f6e22d9a48ac009bb024bad1e68
CONST_STRUCTS_SHA256=ea064f6916a74763468037494aeb270aae34b7c97617e84d424ca5b8733539b2

SUBJECT_MAX=75

die() {
	printf 'check-style: %s\n' "$*" >&2
	exit 2
}

usage() {
	sed -n '/^#   /,/^#$/{s/^#   //;s/^# *//;p;}' "$0" >&2
	exit 2
}

# The checks to ignore: cilium's list first, from its checkpatch image
# (image-tools, images/checkpatch/checkpatch.sh), then what userspace
# adds.
ignored_types() {
	cat <<-EOT
	# Errors
	COMPLEX_MACRO
	GIT_COMMIT_ID
	# Split from GIT_COMMIT_ID after cilium's list was written: a message
	# here cites kernel commits, which are not in this repository.
	UNKNOWN_COMMIT_ID
	MULTISTATEMENT_MACRO_USE_DO_WHILE
	NOT_UNIFIED_DIFF
	# Warnings
	COMMIT_LOG_LONG_LINE
	COMMIT_MESSAGE
	# A string literal is not broken across lines, as the kernel's
	# coding style says of user-visible strings, so that it can be
	# grepped for; the pin and xattr names in util/paths.h run past
	# the limit for that reason.
	LONG_LINE_STRING
	CONSTANT_CONVERSION
	CONST_STRUCT
	EMAIL_SUBJECT
	FILE_PATH_CHANGES
	FROM_SIGN_OFF_MISMATCH
	JIFFIES_COMPARISON
	LINE_CONTINUATIONS
	MACRO_WITH_FLOW_CONTROL
	PRINTK_WITHOUT_KERN_LEVEL
	TRAILING_SEMICOLON
	TRAILING_STATEMENTS
	VOLATILE
	# Checks
	BIT_MACRO
	# Dropping the tolerance makes // comments an error, as cilium does;
	# the SPDX tag of a .c file is rewritten below so it is not one.
	C99_COMMENT_TOLERANCE
	# check-spdx.sh enforces the project's own tag rules, and the
	# rewrite above would fail checkpatch's form check.
	SPDX_LICENSE_TAG
	# The OpenSSL, zlib and tpm2-tss APIs are CamelCase, as is
	# _Static_assert.
	CAMELCASE
	# uint32_t and its kin are the userspace spelling, and the BPF side
	# shares headers with it.
	PREFER_KERNEL_TYPES
	# strscpy, fallthrough;, __packed and the other compiler.h macros do
	# not exist in userspace; BPF has what bpf_helpers.h defines.
	STRCPY
	STRNCPY
	PREFER_FALLTHROUGH
	PREFER_DEFINED_ATTRIBUTE_MACRO
	# SHIRUDO_BUILD_BUG_ON is a compile-time assertion; there is no
	# kernel to crash.
	AVOID_BUG
	# The task_prctl LSM hook returns -ENOSYS for "not handled" by
	# contract.
	ENOSYS
	# The usage helpers print the word usage.
	EMBEDDED_FUNCTION_NAME
	# The usage and summary tables are string literals longer than a
	# line.
	SPLIT_STRING
	# A security report arrives by mail and has no URL; Closes: follows
	# a Reported-by: when there is one.
	BAD_REPORTED_BY_LINK
	EOT
}

# The C the project writes: not the vendored libbpf, not the BTF dump the
# kernel-cve package carries and not the headers make -C
# upatches/kernel-obs lists generates. A git command limited to it.
git_c() {
	git "$@" -- '*.c' '*.h' ':(exclude)vendor/' \
		':(exclude)upatches/kernel-cve/vmlinux.h' \
		':(exclude)*_generated.h'
}

verified() {
	[ -f "$1" ] && [ "$(sha256sum "$1" | cut -d' ' -f1)" = "$2" ]
}

# Fetch one of the kernel's scripts at the pinned tag, from kernel.org or
# its GitHub mirror, and keep it only when it hashes as expected.
fetch() {
	name=$1
	sum=$2
	dest=$3
	for url in \
	    "https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/scripts/$name?h=$KERNEL_TAG" \
	    "https://raw.githubusercontent.com/torvalds/linux/$KERNEL_TAG/scripts/$name"; do
		if curl -fsSL --retry 3 -o "$dest.tmp" "$url" &&
		   verified "$dest.tmp" "$sum"; then
			mv "$dest.tmp" "$dest"
			return 0
		fi
		rm -f "$dest.tmp"
	done
	die "could not fetch scripts/$name at $KERNEL_TAG with sha256 $sum"
}

locate_checkpatch() {
	if [ -n "${CHECKPATCH:-}" ]; then
		[ -r "$CHECKPATCH" ] || die "CHECKPATCH=$CHECKPATCH: not readable"
		printf '%s\n' "$CHECKPATCH"
		return
	fi
	dir=$PWD/build/checkpatch
	if ! verified "$dir/checkpatch.pl" "$CHECKPATCH_PL_SHA256" ||
	   ! verified "$dir/spelling.txt" "$SPELLING_SHA256" ||
	   ! verified "$dir/const_structs.checkpatch" "$CONST_STRUCTS_SHA256"; then
		command -v curl >/dev/null 2>&1 ||
			die "curl not found; set CHECKPATCH to a kernel tree's scripts/checkpatch.pl"
		mkdir -p "$dir"
		printf 'check-style: fetching the kernel'"'"'s checkpatch.pl at %s into %s\n' \
			"$KERNEL_TAG" "$dir" >&2
		fetch checkpatch.pl "$CHECKPATCH_PL_SHA256" "$dir/checkpatch.pl"
		fetch spelling.txt "$SPELLING_SHA256" "$dir/spelling.txt"
		fetch const_structs.checkpatch "$CONST_STRUCTS_SHA256" \
			"$dir/const_structs.checkpatch"
	fi
	printf '%s\n' "$dir/checkpatch.pl"
}

# checkpatch enforces the kernel's block-comment form everywhere but at the
# opener, which only the netdev trees have an opinion on, and theirs is
# the opposite of Documentation/coding-style.rst: a comment that runs on
# past its first line opens with /* alone on that line. Report an added
# line that opens a comment with text and does not close it, in the form
# checkpatch reports in. String literals are blanked first.
check_block_comments() {
	awk '
	/^\+\+\+ / { file = $0; sub(/^\+\+\+ b\//, "", file); next }
	/^@@ / {
		split($0, h, " ")
		sub(/^\+/, "", h[3])
		split(h[3], r, ",")
		line = r[1] - 1
		next
	}
	/^[ +]/ { line++ }
	/^\+/ {
		text = substr($0, 2)
		gsub(/"([^"\\]|\\.)*"/, "\"\"", text)
		while ((at = index(text, "/*")) > 0) {
			rest = substr(text, at + 2)
			if ((end = index(rest, "*/")) > 0) {
				text = substr(rest, end + 2)
				continue
			}
			sub(/^\*+/, "", rest)
			sub(/^[ \t]+/, "", rest)
			if (rest != "") {
				printf "ERROR:NETDEV_BLOCK_COMMENT: a block comment opens with /* on a line of its own\n#%d: FILE: %s:%d:\n%s\n\n", NR, file, line, $0
				bad = 1
			}
			break
		}
	}
	END { exit bad }
	' "$1"
}

# Under GitHub Actions each finding also becomes an annotation.
annotate() {
	if [ -n "${GITHUB_ACTIONS:-}" ]; then
		sed 's/^ERROR:/::error::&/; s/^WARNING:/::warning::&/; s/^CHECK:/::warning::&/'
	else
		cat
	fi
}

# Check one patch stream, at the 80 columns of Documentation/coding-style.rst
# rather than checkpatch's 100. The // SPDX tag of a .c file becomes the
# /* */ form first, so the C99 check, which cannot tell the two uses of
# // apart, sees only real comments. checkpatch seeds a CamelCase cache
# from the include/ under its root and writes it where it runs, strict
# mode or not: it runs in the work directory, over an empty include/.
check_patch() {
	sed -E 's|^([-+ ])// (SPDX-License-Identifier: .*)$|\1/* \2 */|' "$1" >"$work/patch"
	shift
	rc=0
	(cd "$work" && perl "$checkpatch" --no-tree --root="$work" --strict \
		--quiet --show-types --max-line-length=80 --ignore "$IGNORE" \
		"$@" "$work/patch") >"$work/out" 2>&1 || rc=1
	check_block_comments "$work/patch" >>"$work/out" || rc=1
	annotate <"$work/out"
	return $rc
}

# A commit that touches no C source still has a message to check, and
# checkpatch reads a message only ahead of a diff: give it one.
fake_diff() {
	printf '%s\n' 'diff --git a/dev/null b/dev/null' \
		'index 000000000000..000000000001 100644' \
		'--- a/dev/null' '+++ b/dev/null' '@@ -1,1 +1,1 @@' '-' '+.'
}

check_subject() {
	subject=$(git log -1 --format=%s "$1")
	if [ "${#subject}" -gt "$SUBJECT_MAX" ]; then
		printf 'ERROR:SUBJECT_LENGTH: the subject runs to %d characters, the limit is %d\n\n' \
			"${#subject}" "$SUBJECT_MAX" | annotate
		return 1
	fi
}

check_commits() {
	if [ $# -eq 0 ]; then
		set -- -1 HEAD
	fi
	commits=$(git rev-list --reverse --no-merges "$@")
	n=$(printf '%s\n' "$commits" | grep -c . || :)
	i=0
	bad=0
	for c in $commits; do
		i=$((i + 1))
		printf '[%d/%d] %s\n' "$i" "$n" "$(git log -1 --format='%h ("%s")' "$c")"
		git_c show --format=email "$c" >"$work/raw"
		if ! grep -q '^diff --git ' "$work/raw"; then
			{ git log -1 --format=email "$c"; fake_diff; } >"$work/raw"
		fi
		failed=0
		check_patch "$work/raw" || failed=1
		check_subject "$c" || failed=1
		bad=$((bad + failed))
	done
	if [ "$bad" -gt 0 ]; then
		printf '%d of %d commits have style problems\n' "$bad" "$n" >&2
		exit 1
	fi
	printf 'style: %d commits checked\n' "$n"
}

check_diff() {
	git_c diff HEAD >"$work/raw"
	if ! grep -q '^diff --git ' "$work/raw"; then
		printf 'style: no uncommitted changes to C sources\n'
		return
	fi
	check_patch "$work/raw" --no-signoff || exit 1
	printf 'style: the uncommitted changes are clean\n'
}

# Whole files, each as a patch that adds it. git diff exits 1 when the
# file has content, which is the point.
# Every type checkpatch knows, less the ignored ones.
list_types() {
	perl "$checkpatch" --list-types | awk 'NR > 2 && NF >= 2 { print $2 }' |
		sort >"$work/all"
	ignored_types | grep -v '^#' | sort >"$work/ignored"
	comm -23 "$work/all" "$work/ignored"
}

check_files() {
	if [ $# -eq 0 ]; then
		set -- $(git_c ls-files)
	fi
	: >"$work/raw"
	for f in "$@"; do
		[ -f "$f" ] || die "$f: no such file"
		git diff --no-index -- /dev/null "$f" >>"$work/raw" || :
	done
	check_patch "$work/raw" --no-signoff || exit 1
	printf 'style: %d files checked\n' "$#"
}

# checkpatch runs away from the checkout, so its path must not be
# relative to where the caller stood.
if [ -n "${CHECKPATCH:-}" ]; then
	case $CHECKPATCH in
	/*)
		;;
	*)
		CHECKPATCH=$PWD/$CHECKPATCH
		;;
	esac
fi

cd "$(git rev-parse --show-toplevel)"

mode=commits
case "${1:-}" in
--files)
	mode=files
	shift
	;;
--diff)
	mode='diff'
	shift
	;;
--types)
	mode=types
	shift
	;;
-h|--help)
	usage
	;;
esac

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
mkdir "$work/include"

checkpatch=$(locate_checkpatch)
IGNORE=$(ignored_types | grep -v '^#' | paste -sd, -)

case "$mode" in
commits)
	check_commits "$@"
	;;
diff)
	check_diff
	;;
files)
	check_files "$@"
	;;
types)
	list_types
	;;
esac
