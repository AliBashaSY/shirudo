#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t0049 -- util/string.c: numbers parsed from a bounded, unterminated
# span, digits only, within the bounds the caller gives.

cd "$(dirname "$0")"
. ./test-lib.sh

H=helper/test-string
U64_MAX=18446744073709551615
S64_MIN=-9223372036854775808
S64_MAX=9223372036854775807

# EINVAL=22, ERANGE=34 on Linux.

test_expect_success 'leading digits are counted' '
	./$H digits 123abc | grep -qx "n=3" &&
	./$H digits abc | grep -qx "n=0" &&
	./$H digits "" | grep -qx "n=0"
'

test_expect_success 'a decimal fills its span' '
	./$H u64 $U64_MAX 0 | grep -qx "rc=0 v=0" &&
	./$H u64 $U64_MAX 007 | grep -qx "rc=0 v=7" &&
	./$H u64 $U64_MAX $U64_MAX | grep -qx "rc=0 v=$U64_MAX"
'

test_expect_success 'nothing past the span is read' '
	./$H u64 $U64_MAX 123456 3 | grep -qx "rc=0 v=123" &&
	./$H u64 $U64_MAX 123 0 | grep -qx "rc=-22 v=0"
'

test_expect_success 'a sign, blank or base prefix is not a number' '
	for t in "" -1 +1 " 1" "1 " 0x10 1.5 1e3 12a; do
		./$H u64 $U64_MAX "$t" | grep -qx "rc=-22 v=0" || return 1
	done
'

test_expect_success 'a number past its bound is out of range' '
	./$H u64 $U64_MAX 18446744073709551616 | grep -qx "rc=-34 v=0" &&
	./$H u64 10 10 | grep -qx "rc=0 v=10" &&
	./$H u64 10 11 | grep -qx "rc=-34 v=0" &&
	./$H u64 5 9 | grep -qx "rc=-34 v=0" &&
	./$H u64 0 0 | grep -qx "rc=0 v=0"
'

test_expect_success 'a malformed number is not one, however long' '
	./$H u64 $U64_MAX 99999999999999999999x | grep -qx "rc=-22 v=0"
'

test_expect_success 'hexadecimal takes either case and no prefix' '
	./$H x64 $U64_MAX ff | grep -qx "rc=0 v=255" &&
	./$H x64 $U64_MAX FF | grep -qx "rc=0 v=255" &&
	./$H x64 $U64_MAX ffffffffffffffff | grep -qx "rc=0 v=$U64_MAX" &&
	./$H x64 $U64_MAX 10000000000000000 | grep -qx "rc=-34 v=0" &&
	./$H x64 $U64_MAX 0x10 | grep -qx "rc=-22 v=0" &&
	./$H x64 $U64_MAX g | grep -qx "rc=-22 v=0"
'

test_expect_success 'a signed number spans the whole range' '
	./$H s64 $S64_MIN $S64_MAX $S64_MIN | grep -qx "rc=0 v=$S64_MIN" &&
	./$H s64 $S64_MIN $S64_MAX $S64_MAX | grep -qx "rc=0 v=$S64_MAX" &&
	./$H s64 $S64_MIN $S64_MAX -0 | grep -qx "rc=0 v=0" &&
	./$H s64 $S64_MIN $S64_MAX 9223372036854775808 |
		grep -qx "rc=-34 v=0" &&
	./$H s64 $S64_MIN $S64_MAX -9223372036854775809 |
		grep -qx "rc=-34 v=0"
'

test_expect_success 'a signed number keeps to its bounds and form' '
	./$H s64 1 10 0 | grep -qx "rc=-34 v=0" &&
	./$H s64 -5 5 -6 | grep -qx "rc=-34 v=0" &&
	./$H s64 -5 5 -5 | grep -qx "rc=0 v=-5" &&
	for t in - -- --1 +1 " -1" -x; do
		./$H s64 $S64_MIN $S64_MAX "$t" | grep -qx "rc=-22 v=0" ||
			return 1
	done
'

test_done
