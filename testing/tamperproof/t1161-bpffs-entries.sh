#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t1161 -- the private bpffs takes no entry shirudo did not make. bpffs
# lets a caller make directories, symbolic links and hard links, and a
# symbolic link of root's in place of a pin directory sent the pins of
# the next load into a bpffs of root's own, out of the guards' reach,
# where a hard link to a pin would keep a link alive past its unlink.
# Each is refused and counted as PinEntry. Non-destructive.

cd "$(dirname "$0")/.."
. ./integration-lib.sh
target_guard
command -v python3 >/dev/null 2>&1 || test_skip_all "no python3"
B=/run/shirudo/bpffs
[ -e "$B/lsm_link" ] || test_skip_all "no lsm_link pin"

cleanup() {
	rmdir "$B/t1161-dir" 2>/dev/null || true
	rm -f "$B/t1161-symlink" "$B/t1161-hardlink" 2>/dev/null || true
}
trap cleanup EXIT

counter() {
	"$SHIRUDO_BIN" status --json | python3 -c '
import json, sys
print(json.load(sys.stdin)["tamper_attempts"]["counters"]["PinEntry"])
'
}

test_expect_success 'root cannot make a directory in the private bpffs' '
	before=$(counter) &&
	! mkdir "$B/t1161-dir" 2>/dev/null &&
	[ ! -e "$B/t1161-dir" ] &&
	[ "$(counter)" -gt "$before" ]
'

test_expect_success 'root cannot make a symbolic link in the private bpffs' '
	before=$(counter) &&
	! ln -s /tmp "$B/t1161-symlink" 2>/dev/null &&
	[ ! -L "$B/t1161-symlink" ] &&
	[ "$(counter)" -gt "$before" ]
'

test_expect_success 'root cannot hard-link a pin in the private bpffs' '
	before=$(counter) &&
	! ln "$B/lsm_link" "$B/t1161-hardlink" 2>/dev/null &&
	[ ! -e "$B/t1161-hardlink" ] &&
	[ "$(counter)" -gt "$before" ]
'

test_done
