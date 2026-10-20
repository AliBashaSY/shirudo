#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t0047 -- creating under a sealed directory from userspace: the
# util/seal.c helpers create relative to the opened directory and give
# a new inode the directory's label where the kernel did not, so that
# a file, a directory chain, an atomic replacement and a symlink under
# a labelled directory come out labelled, and nothing outside one does.
# The labelled cases need root on a file system with security xattrs
# and are skipped without.

cd "$(dirname "$0")"
. ./test-lib.sh
H=helper/test-seal
LABEL=94442b45cfcdc59aabc2a91364ed2ae129418e05a5c1985231b9c032e673f469
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT

test_expect_success 'a directory chain is created in one go' '
	./$H mkdir "$W/plain/a/b/c" | grep -qx "rc=0" &&
	[ -d "$W/plain/a/b/c" ] &&
	./$H mkdir "$W/plain/a/b/c" | grep -qx "rc=0"
'

test_expect_success 'a file is replaced atomically and its temporary removed' '
	./$H write "$W/plain/state" one | grep -qx "rc=0" &&
	[ "$(cat "$W/plain/state")" = one ] &&
	./$H write "$W/plain/state" two | grep -qx "rc=0" &&
	[ "$(cat "$W/plain/state")" = two ] &&
	test ! -e "$W/plain/state.tmp"
'

test_expect_success 'a file is opened for appending and created on demand' '
	./$H open "$W/plain/log" first | grep -qx "rc=0" &&
	./$H open "$W/plain/log" second | grep -qx "rc=0" &&
	[ "$(cat "$W/plain/log")" = firstsecond ]
'

test_expect_success 'a temporary that is a symlink is refused, not followed' '
	ln -s "$W/plain/state" "$W/plain/victim.tmp" &&
	! ./$H write "$W/plain/victim" x >/dev/null 2>&1 &&
	[ "$(cat "$W/plain/state")" = two ] &&
	test ! -e "$W/plain/victim"
'

test_expect_success 'nothing under an unlabelled directory gets a label' '
	./$H get "$W/plain/state" | grep -qx "label=none" &&
	./$H get "$W/plain/a/b/c" | grep -qx "label=none" &&
	./$H get "$W/plain/log" | grep -qx "label=none"
'

mkdir "$W/sealed"
if ! ./$H label "$W/sealed" >/dev/null 2>&1; then
	echo "# no security xattrs here (root on ext4 or tmpfs needed); the labelled cases are skipped"
	test_done
fi

test_expect_success 'a file written under a labelled directory carries its label' '
	./$H write "$W/sealed/state" one | grep -qx "rc=0" &&
	./$H get "$W/sealed/state" | grep -qx "label=$LABEL"
'

test_expect_success 'a file opened for appending there carries it too' '
	./$H open "$W/sealed/log" x | grep -qx "rc=0" &&
	./$H get "$W/sealed/log" | grep -qx "label=$LABEL"
'

test_expect_success 'every directory of a chain carries it, and a file below' '
	./$H mkdir "$W/sealed/a/b" | grep -qx "rc=0" &&
	./$H get "$W/sealed/a" | grep -qx "label=$LABEL" &&
	./$H get "$W/sealed/a/b" | grep -qx "label=$LABEL" &&
	./$H write "$W/sealed/a/b/f" x | grep -qx "rc=0" &&
	./$H get "$W/sealed/a/b/f" | grep -qx "label=$LABEL"
'

test_expect_success 'a symlink gets the label itself, its target untouched' '
	touch "$W/plain/target" &&
	ln -s "$W/plain/target" "$W/sealed/link" &&
	./$H inherit "$W/sealed/link" | grep -qx "rc=0" &&
	./$H get "$W/sealed/link" | grep -qx "label=$LABEL" &&
	./$H get "$W/plain/target" | grep -qx "label=none"
'

test_expect_success 'a directory that exists already is left as it is' '
	mkdir "$W/sealed/old" &&
	./$H mkdir "$W/sealed/old/new" | grep -qx "rc=0" &&
	./$H get "$W/sealed/old" | grep -qx "label=none" &&
	./$H get "$W/sealed/old/new" | grep -qx "label=none"
'

test_done
