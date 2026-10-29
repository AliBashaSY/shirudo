#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t3011 -- names keep their terminator and are never cut into others: a
# pointer type whose name fills the buffer, a signed name section with
# no terminator, an upatch name the targets refuse or the header cannot
# hold, and a tree repository longer than its field.

cd "$(dirname "$0")/../.."
. ./test-lib.sh

S=../shirudo
B=../tools/shirudo-build/shirudo-build
R=./helper/test-upatch-rodata
[ -x "$S" ] && [ -x "$B" ] && [ -x "$R" ] || test_skip_all "binaries not built"
command -v python3 >/dev/null 2>&1 || test_skip_all "no python3"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
"$S" genkey --keys-dir "$WORK/keys" >/dev/null 2>&1 ||
	test_skip_all "cannot generate a signing key"
PAYLOAD=$(printf 'a%.0s' $(seq 64))

test_expect_success 'a pointer type name that fills the buffer is refused, terminated' '
	"$R" 253 1 >"$WORK/fits" && grep -q "off=255 terminated=1" "$WORK/fits" &&
	! "$R" 254 1 >"$WORK/full" && grep -q "rc=-28 .*terminated=1" "$WORK/full" &&
	! "$R" 252 2 >"$WORK/full2" && grep -q "terminated=1" "$WORK/full2"
'

test_expect_success 'an upatch name the targets refuse is not signed' '
	! "$B" --unload ../x -o "$WORK/bad.upatch" --payload-sha256 "$PAYLOAD" \
		--keys-dir "$WORK/keys" >/dev/null 2>"$WORK/bad.err" &&
	grep -q "is not 1 to 255 characters" "$WORK/bad.err" &&
	[ ! -e "$WORK/bad.upatch" ]
'

test_expect_success 'an upatch name the header cannot hold is not cut and signed' '
	long=$(printf "n%.0s" $(seq 256)) &&
	! "$B" --unload "$long" -o "$WORK/long.upatch" --payload-sha256 "$PAYLOAD" \
		--keys-dir "$WORK/keys" >/dev/null 2>&1 &&
	[ ! -e "$WORK/long.upatch" ] &&
	short=$(printf "n%.0s" $(seq 255)) &&
	"$B" --unload "$short" -o "$WORK/short.upatch" --payload-sha256 "$PAYLOAD" \
		--keys-dir "$WORK/keys" >/dev/null 2>&1
'

test_expect_success 'a signed name section without its terminator is no name' '
	"$B" --unload good -o "$WORK/good.upatch" --payload-sha256 "$PAYLOAD" \
		--keys-dir "$WORK/keys" >/dev/null 2>&1 &&
	"$S" upatch info "$WORK/good.upatch" | grep -q "Name: *good" &&
	python3 - "$WORK/good.upatch" "$WORK/unterminated.upatch" <<"PY" &&
import sys
data = bytearray(open(sys.argv[1], "rb").read())
name = b"good" + bytes(252)
# The header, the template data and last the signed sections carry it.
at = data.rfind(name)
assert at > 16, "no signed name section"
data[at:at + 256] = b"A" * 256
open(sys.argv[2], "wb").write(data)
PY
	"$S" upatch info "$WORK/unterminated.upatch" >"$WORK/info" 2>&1;
	grep -q "Name: *(none)" "$WORK/info" && ! grep -q "AAAA" "$WORK/info"
'

test_expect_success 'a tree repository longer than its field is refused, not cut' '
	repo="oci://registry.invalid/$(printf "r%.0s" $(seq 600))" &&
	! "$B" bpm tree append "$WORK/absent.bee" "$repo" --keys-dir "$WORK/keys" \
		>/dev/null 2>"$WORK/tree.err" &&
	grep -q "repository is longer than" "$WORK/tree.err"
'

test_done
