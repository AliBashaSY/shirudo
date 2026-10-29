#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# Run the target-side BPM tree integration tests.

set -eu

cd "$(dirname "$0")"

fail=0
: "${SHIRUDO_TEST_SUITE_FAILURES_FILE:=}"
	for t in \
	functional/bpm/t7101-bpm-tree.sh \
	functional/bpm/t7102-bpm-tree-sequential.sh \
	functional/bpm/t7103-bpm-tree-delete.sh
do
	if sh "$t"; then
		:
	else
		fail=$((fail + 1))
		if [ -n "$SHIRUDO_TEST_SUITE_FAILURES_FILE" ]; then
			printf '%s\n' "$t" >>"$SHIRUDO_TEST_SUITE_FAILURES_FILE" 2>/dev/null || :
		fi
	fi
done

exit "$fail"
