#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# testing/run-tests.sh -- the unit tests, then every host test (t?0??) of
# ghchecks/, functional/ and e2e/ in number order. CI-friendly; the exit
# status counts the suites that failed.

set -eu

cd "$(dirname "$0")"

host_tests()
{
	for t in ghchecks/t*.sh functional/*/t*.sh e2e/t*.sh; do
		case "${t##*/}" in
		t[0-9]0[0-9][0-9]-*.sh) printf '%s %s\n' "${t##*/}" "$t" ;;
		esac
	done | sort | cut -d' ' -f2
}

fail=0
: "${SHIRUDO_TEST_SUITE_FAILURES_FILE:=}"
for t in ../build/unit/* $(host_tests); do
	case "$t" in
	*.sh) set -- sh "$t" ;;
	*) [ -f "$t" ] && [ -x "$t" ] || continue; set -- "$t" ;;
	esac
	if "$@"; then
		:
	else
		fail=$((fail + 1))
		if [ -n "$SHIRUDO_TEST_SUITE_FAILURES_FILE" ]; then
			printf '%s\n' "$t" >>"$SHIRUDO_TEST_SUITE_FAILURES_FILE" 2>/dev/null || :
		fi
	fi
done

exit "$fail"
