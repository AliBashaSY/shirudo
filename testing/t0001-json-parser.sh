#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t0001 -- util/json schema parser: extraction, skipping, bounds,
# and robustness against malformed input.

cd "$(dirname "$0")"
. ./test-lib.sh

H=helper/test-json-parser

# EINVAL=22, E2BIG=7, ENOSPC=28 on Linux.

test_expect_success 'valid event: extracts digest, skips nested set[]' '
	out=$(printf "%s" "{\"schema\":1,\"digest\":\"abcdef\",\"type\":\"init\",\"set\":[{\"name\":\"x\"}]}" | ./$H) &&
	echo "$out" | grep -qx "rc=0" &&
	echo "$out" | grep -q "digest=abcdef" &&
	echo "$out" | grep -q "type=init" &&
	echo "$out" | grep -q "schema=1"
'

test_expect_success 'unknown keys and nested objects are skipped' '
	printf "%s" "{\"a\":{\"b\":[1,2,3]},\"digest\":\"ff\",\"c\":true}" | ./$H | grep -q "digest=ff"
'

test_expect_success 'unconfigured schema entries skip unknown keys without crashing' '
	printf "%s" "{\"unknown\":true,\"digest\":\"ff\"}" | ./$H | grep -q "digest=ff"
'

test_expect_success 'discarded object key colliding with schema does not mis-dispatch' '
	out=$(printf "%s" "{\"a\":{\"digest\":123,\"type\":true},\"digest\":\"ff\",\"type\":\"init\"}" | ./$H) &&
	echo "$out" | grep -qx "rc=0" &&
	echo "$out" | grep -q "digest=ff" &&
	echo "$out" | grep -q "type=init"
'

# A value read as a C string would end at the NUL and pass checks
# meant for the whole value; a key would be dispatched as its prefix.
test_expect_success 'an escaped NUL in a value or a key -> EINVAL' '
	printf "%s" "{\"digest\":\"ab\\u0000cd\"}" | ./$H | grep -qx "rc=-22" &&
	printf "%s" "{\"digest\\u0000x\":\"ff\"}" | ./$H | grep -qx "rc=-22" &&
	printf "%s" "{\"digest\":\"ab\\u0041\"}" | ./$H | grep -q "digest=abA"
'

test_expect_success 'unterminated string -> EINVAL' '
	printf "%s" "{\"digest\":\"abc" | ./$H | grep -qx "rc=-22"
'

test_expect_success 'trailing garbage -> EINVAL' '
	printf "%s" "{\"digest\":\"a\"}xyz" | ./$H | grep -qx "rc=-22"
'

test_expect_success 'type mismatch (digest as number) -> EINVAL' '
	printf "%s" "{\"digest\":123}" | ./$H | grep -qx "rc=-22"
'

test_expect_success 'over-long string value -> ENOSPC' '
	big=$(printf "%0200d" 0) &&
	printf "%s" "{\"digest\":\"$big\"}" | ./$H | grep -qx "rc=-28"
'

test_expect_success 'over-deep nesting -> E2BIG (no crash)' '
	doc=$(python3 -c "print(\"{\\\"a\\\":\"*40 + \"1\" + \"}\"*40, end=\"\")") &&
	printf "%s" "$doc" | ./$H | grep -qx "rc=-7"
'

test_expect_success 'non-JSON garbage -> clean error, no crash' '
	head -c 4096 /dev/urandom | ./$H >/dev/null 2>&1; rc=$?; [ "$rc" -ne 139 ]
'

test_expect_success 'empty object parses, no fields seen' '
	printf "%s" "{}" | ./$H | grep -qx "rc=0"
'

test_expect_success 'string array dispatched element-by-element' '
	out=$(printf "%s" "{\"names\":[\"alpha\",\"beta\",\"gamma\"]}" | ./$H) &&
	echo "$out" | grep -qx "rc=0" &&
	echo "$out" | grep -q "names=3 first_name=alpha"
'

test_expect_success 'object array dispatched into element sub-schema' '
	out=$(printf "%s" "{\"events\":[{\"type\":\"init\",\"digest\":\"00\"},{\"type\":\"upatch_load\",\"digest\":\"ff\"}]}" | ./$H) &&
	echo "$out" | grep -qx "rc=0" &&
	echo "$out" | grep -q "events=2 last_type=upatch_load"
'

test_expect_success 'empty array parses, zero elements' '
	printf "%s" "{\"names\":[]}" | ./$H | grep -q "names=0 "
'

test_expect_success 'array nested in object element, plus trailing scalars' '
	out=$(printf "%s" "{\"events\":[{\"type\":\"init\",\"digest\":\"aa\",\"set\":[{\"name\":\"x\"}]}],\"digest\":\"top\"}" | ./$H) &&
	echo "$out" | grep -q "digest=top" &&
	echo "$out" | grep -q "events=1 last_type=init"
'

test_expect_success 'array element type mismatch -> EINVAL (no crash)' '
	printf "%s" "{\"names\":[123]}" | ./$H | grep -qx "rc=-22"
'

test_expect_success 'unterminated array -> EINVAL (no crash)' '
	printf "%s" "{\"names\":[\"a\",\"b\"" | ./$H | grep -qx "rc=-22"
'

test_expect_success 'an integer field takes an integer as JSON spells one' '
	for v in 1.9 1e9 1E9 7-3 1+1 - . 01 -01 1. .5 1e --1 0x1 +1; do
		printf "%s" "{\"schema\":$v}" | ./$H | grep -qx "rc=-22" ||
			return 1
	done
'

test_expect_success 'an integer field holds any __s64 and nothing past it' '
	printf "%s" "{\"schema\":-9223372036854775808}" | ./$H |
		grep -q "schema=-9223372036854775808 " &&
	printf "%s" "{\"schema\":9223372036854775807}" | ./$H |
		grep -q "schema=9223372036854775807 " &&
	printf "%s" "{\"schema\":-0}" | ./$H | grep -q "schema=0 " &&
	printf "%s" "{\"schema\":9223372036854775808}" | ./$H |
		grep -qx "rc=-22" &&
	printf "%s" "{\"schema\":-9223372036854775809}" | ./$H |
		grep -qx "rc=-22"
'

test_expect_success 'a number in a discarded value follows the grammar too' '
	printf "%s" "{\"x\":[1.5e+10,-0.0E-1,2e5,0],\"schema\":2}" | ./$H |
		grep -q "schema=2 " &&
	for v in 1. 01 1e+ -.5 1.e5; do
		printf "%s" "{\"x\":$v,\"schema\":2}" | ./$H |
			grep -qx "rc=-22" || return 1
	done
'

test_done
