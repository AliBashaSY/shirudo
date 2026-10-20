#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t1133 -- labelling at creation (bpf/seal.c inode_init_security): an
# inode created under a labelled directory the seal has seen is born
# with the label, whoever creates it, and is sealed from that instant.
# The probes are made under /var/lib/shirudo/locks, which nothing
# enumerates, and stay there: a sealed inode cannot be removed by the
# test. Non-destructive otherwise.

cd "$(dirname "$0")/.."
. ./integration-lib.sh
target_guard

LABEL=94442b45cfcdc59aabc2a91364ed2ae129418e05a5c1985231b9c032e673f469
LOCKS=/var/lib/shirudo/locks
[ -e /run/shirudo/bpffs/seal_inode_init_security_link ] ||
	test_skip_all "the seal does not label at creation on this target"

label_of() {
	python3 -c '
import os, sys
try:
    print(os.getxattr(sys.argv[1], "security.bpf.shirudo",
                      follow_symlinks=False).decode())
except OSError:
    print("none")' "$1"
}

test_expect_success 'the run directory carries the label' '
	[ "$(label_of /run/shirudo)" = "$LABEL" ]
'

# Opening the directory is what marks it; the walk at init did, and so
# does this, in case the inode was evicted since. The create succeeds
# and the write open that follows it in the same call is what the seal
# refuses, so the shell's status is not looked at.
test_expect_success 'a file root creates under a sealed directory is born labelled' '
	ls /var/lib/shirudo >/dev/null &&
	mkdir -p "$LOCKS" &&
	ls "$LOCKS" >/dev/null &&
	{ : > "$LOCKS/t1133.probe"; } 2>/dev/null;
	[ -e "$LOCKS/t1133.probe" ] &&
	[ "$(label_of "$LOCKS/t1133.probe")" = "$LABEL" ]
'

test_expect_success 'and is sealed from that instant: no write, unlink or rename' '
	! sh -c "echo x > \"$LOCKS/t1133.probe\"" 2>/dev/null &&
	! rm "$LOCKS/t1133.probe" 2>/dev/null &&
	! mv "$LOCKS/t1133.probe" "$LOCKS/t1133.moved" 2>/dev/null &&
	[ -e "$LOCKS/t1133.probe" ] &&
	[ ! -s "$LOCKS/t1133.probe" ]
'

test_expect_success 'a new directory is marked in turn: its child needs no reopen' '
	mkdir "$LOCKS/t1133.dir" &&
	[ "$(label_of "$LOCKS/t1133.dir")" = "$LABEL" ] &&
	mkfifo "$LOCKS/t1133.dir/child" &&
	[ "$(label_of "$LOCKS/t1133.dir/child")" = "$LABEL" ]
'

test_expect_success 'a symlink is born labelled as well' '
	ln -s t1133.probe "$LOCKS/t1133.link" &&
	[ "$(label_of "$LOCKS/t1133.link")" = "$LABEL" ]
'

test_expect_success 'an O_TMPFILE under the tree cannot be opened for writing' '
	! python3 -c "import os,sys; os.open(sys.argv[1], os.O_TMPFILE | os.O_WRONLY, 0o600)" \
	    "$LOCKS" 2>/dev/null
'

test_expect_success 'a file created outside the tree stays unlabelled' '
	f=$(mktemp) &&
	[ "$(label_of "$f")" = none ] &&
	rm -f "$f"
'

test_done
