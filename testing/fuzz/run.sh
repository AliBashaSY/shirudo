#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# testing/fuzz/run.sh -- run the named libFuzzer targets, or one for each
# fuzz-*.c here, for the given seconds on their seed corpora, keeping what
# they find in a scratch directory. A crash fails the run and leaves the
# input that caused it in testing/fuzz.
#
#   testing/fuzz/run.sh <seconds> [<target>...]

set -eu

cd "$(dirname "$0")"
seconds=${1:?usage: run.sh <seconds> [<target>...]}
shift
if [ $# -eq 0 ]; then
	for c in fuzz-*.c; do
		t=${c#fuzz-}
		set -- "$@" "${t%.c}"
	done
fi

work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

rc=0
for t in "$@"; do
	mkdir -p "$work/$t"
	if ./fuzz-"$t" -max_total_time="$seconds" -timeout=10 \
		-rss_limit_mb=2048 -artifact_prefix="$PWD/crash-$t-" \
		"$work/$t" "corpus/$t" >"$work/$t.log" 2>&1; then
		printf 'fuzz: %s: %s\n' "$t" "$(tail -n 1 "$work/$t.log")"
	else
		tail -n 40 "$work/$t.log" >&2
		printf 'fuzz: %s failed; the input is in testing/fuzz/crash-%s-*\n' \
			"$t" "$t" >&2
		rc=1
	fi
done
exit $rc
