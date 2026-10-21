#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t0050 -- util/conditions.c: the KeyringSealed condition names the kind
# of keyring the anchor is in, and a keyring other than the one init
# froze makes the status tampered.

cd "$(dirname "$0")"
. ./test-lib.sh

H=helper/test-conditions

test_expect_success 'the bpf keyring init froze is the production anchor' '
	out=$(./$H 1 bpf 100 100 match) &&
	echo "$out" | grep -qx "status=ok" &&
	echo "$out" | grep -qx "KeyringSealed OK serial=100 in the bpf keyring, pin matches"
'

test_expect_success 'a staged user keyring warns' '
	out=$(./$H 1 user 200 200 match) &&
	echo "$out" | grep -qx "status=ok" &&
	echo "$out" | grep -q "^KeyringSealed WARN staging mode: serial=200 in the user keyring;"
'

test_expect_success 'a user keyring without the LSM is not called the bpf keyring' '
	./$H 0 user 300 0 none |
		grep -qx "KeyringSealed OK serial=300 in the user keyring, no pin recorded"
'

test_expect_success 'before init the keyring is pending, not tampered' '
	out=$(./$H 0 none -1 0 none) &&
	echo "$out" | grep -qx "status=lsm-off" &&
	echo "$out" | grep -qx "KeyringSealed WARN shirudo keyring absent (not init'"'"'d?)"
'

test_expect_success 'a swapped keyring is tampered' '
	out=$(./$H 1 bpf 101 100 match) &&
	echo "$out" | grep -qx "status=tampered" &&
	echo "$out" | grep -qx "reason=keyring serial swap detected" &&
	echo "$out" | grep -qx "KeyringSealed ALERT pin serial=100 != live serial=101 (swapped?)"
'

test_done
