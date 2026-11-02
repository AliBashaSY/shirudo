#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t0006 -- .github/scripts/check-coccinelle.sh: a rule that matches fails the
# check with file, line, rule and message, one that does not passes it,
# a rule that can rewrite does with -D patch, on a throwaway repository,
# and the tree itself.

cd "$(dirname "$0")/.."
. ./test-lib.sh
command -v git >/dev/null 2>&1 || test_skip_all "git not installed"
command -v spatch >/dev/null 2>&1 || test_skip_all "spatch not installed"
ROOT=$(cd .. && pwd)
S=$ROOT/.github/scripts/check-coccinelle.sh
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT

R=$W/repo
mkdir -p "$R/testing/coccinelle" "$R/vendor"
git init -q "$R"
cp "$ROOT/testing/coccinelle/zero_init.cocci" \
	"$ROOT/testing/coccinelle/banned.cocci" "$R/testing/coccinelle/"
printf 'int f(void)\n{\n\tint a[4] = {0};\n\n\treturn a[0];\n}\n' >"$R/bad.c"
printf 'int g(void)\n{\n\tint a[4] = {};\n\n\treturn a[0];\n}\n' >"$R/good.c"
printf 'int h(void)\n{\n\treturn rand();\n}\n' >"$R/vendor/skip.c"

check() {
	(cd "$R" && git add -A && sh "$S" "$@") >"$W/out" 2>"$W/err"
}

test_expect_success 'a match fails the check and names file, line and rule' '
	! check &&
	grep -q "^zero_init: bad.c:3:" "$W/err" &&
	grep -q "1 of 2 rules matched" "$W/err"
'

test_expect_success 'the vendored sources are not checked' '
	! grep -q "skip.c" "$W/err"
'

test_expect_success 'a rule is named alone or by path' '
	check banned &&
	grep -q "1 rules, no match" "$W/out" &&
	! check testing/coccinelle/zero_init.cocci
'

test_expect_success 'a rule rewrites what it finds with -D patch' '
	(cd "$R" && spatch -D patch --very-quiet --in-place \
		--sp-file testing/coccinelle/zero_init.cocci bad.c) &&
	grep -q "= {};" "$R/bad.c" &&
	check
'

test_expect_success 'the tree passes' '
	(cd "$ROOT" && sh "$S") >"$W/out" 2>"$W/err" &&
	grep -q "rules, no match" "$W/out"
'

test_done
