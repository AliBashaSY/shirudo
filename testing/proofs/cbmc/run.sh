#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# testing/proofs/cbmc/run.sh -- check with CBMC that shirudo's parsers keep the
# properties the harnesses in this directory state, for every input within
# the bounds each harness names, and fail when one does not hold. Each
# harness defines harness() and includes the source it proves; proofs.list
# gives its CBMC options, and LEN=<a>-<b>,<c> there runs it once for each
# length the list of lengths and ranges names, as CBMC handles a fixed
# length far faster than one it has to keep symbolic. -v prints the whole CBMC output and the trace of a
# failed check.
#
#   testing/proofs/cbmc/run.sh [-v] [<proof>...]

set -eu

verbose=
if [ "${1:-}" = -v ]; then
	verbose=--trace
	shift
fi

cd "$(dirname "$0")"
root=$(cd ../../.. && pwd)
command -v cbmc >/dev/null 2>&1 || {
	printf 'proofs: cbmc not found; install cbmc\n' >&2
	exit 2
}

# --unsigned-char as the build's -funsigned-char.
checks='--unsigned-char --bounds-check --pointer-check
	--pointer-primitive-check --div-by-zero-check --signed-overflow-check
	--undefined-shift-check --pointer-overflow-check --conversion-check
	--unwinding-assertions'

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

wanted() {
	name=$1
	shift 2
	[ $# -eq 0 ] && return 0
	for p in "$@"; do
		[ "$p" = "$name" ] && return 0
	done
	return 1
}

# One CBMC run; a function CBMC has no body for returns anything, which
# proves nothing about the code that calls it.
prove() {
	log=$1
	shift
	# shellcheck disable=SC2086
	cbmc --function harness -I "$root/include" -I "$root/util" \
		-D_GNU_SOURCE $checks "$@" $verbose >"$log" 2>&1 &&
		! grep -q 'no body for function' "$log"
}

failed=0
ran=0
grep -v '^[[:space:]]*\(#\|$\)' proofs.list >"$work/list"
while read -r name opts; do
	wanted "$name" -- "$@" || continue
	ran=$((ran + 1))
	lens=-
	rest=
	for o in $opts; do
		case $o in
		LEN=*) lens=${o#LEN=} ;;
		*) rest="$rest $o" ;;
		esac
	done
	[ "$lens" = - ] ||
		lens=$(printf '%s\n' "$lens" | tr ',' '\n' |
			while IFS=- read -r from to; do
				seq "$from" "${to:-$from}"
			done)
	start=$(date +%s)
	checked=0
	bad=
	for len in $lens; do
		log=$work/$name.$len.log
		define=
		[ "$len" = - ] || define="-DLEN=$len"
		# shellcheck disable=SC2086
		if prove "$log" $define $rest "$name.c"; then
			n=$(grep -o 'of [0-9]* failed' "$log" | tail -1 | tr -dc '0-9')
			checked=$((checked + ${n:-0}))
			continue
		fi
		bad=${define:-all}
		break
	done
	if [ -z "$bad" ]; then
		printf 'ok      %-28s %6d checks, %ss\n' "$name" "$checked" \
			"$(($(date +%s) - start))"
		continue
	fi
	failed=$((failed + 1))
	printf 'FAILED  %s (%s)\n' "$name" "$bad"
	if [ -n "$verbose" ]; then
		cat "$log"
	else
		grep -E 'FAILURE|ERROR|error:|no body for function|out of memory' \
			"$log" | head -20 | sed 's/^/        /'
	fi
done <"$work/list"

[ "$ran" -gt 0 ] || {
	printf 'proofs: no proof named %s\n' "$*" >&2
	exit 2
}
if [ "$failed" -gt 0 ]; then
	printf 'proofs: %d of %d failed\n' "$failed" "$ran" >&2
	exit 1
fi
printf 'proofs: %d hold\n' "$ran"
