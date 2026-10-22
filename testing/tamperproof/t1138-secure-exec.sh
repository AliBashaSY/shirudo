#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t1138 -- shirudo's executable runs in secure-exec mode, so the dynamic
# loader takes nothing from the environment of whoever started it.

cd "$(dirname "$0")/.."
. ./integration-lib.sh
target_guard
grep -qa '/ld-linux' "$SHIRUDO_BIN" ||
	test_skip_all "shirudo has no dynamic loader"
ENV=$(command -v env) || test_skip_all "no env"
W=$(mktemp -d)
trap 'rm -f "$W/out" "$W/control" "$W/script"; rmdir "$W"' EXIT HUP INT TERM

test_expect_success 'the loader shows other programs their auxiliary vector' '
	LD_SHOW_AUXV=1 "$ENV" true >"$W/out" &&
	grep -q "^AT_SECURE: *0$" "$W/out"
'

test_expect_success 'shirudo runs with AT_SECURE and its loader ignores LD_SHOW_AUXV' '
	LD_SHOW_AUXV=1 "$SHIRUDO_BIN" version >"$W/out" &&
	! grep -q "^AT_" "$W/out"
'

test_expect_success 'shirudo started as a script interpreter runs with AT_SECURE too' '
	printf "#!%s true\n" "$ENV" >"$W/control" &&
	printf "#!%s\n" "$SHIRUDO_BIN" >"$W/script" &&
	chmod 0755 "$W/control" "$W/script" &&
	LD_SHOW_AUXV=1 "$W/control" >"$W/out" &&
	grep -q "^AT_SECURE: *0$" "$W/out" &&
	{ LD_SHOW_AUXV=1 "$W/script" >"$W/out" 2>&1 || :; } &&
	! grep -q "^AT_" "$W/out"
'

test_done
