#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t6112 -- the trust anchors in the kernel's keyrings are root's to look
# at and nobody's but shirudo's to change: keyctl cannot unlink, clear,
# revoke, invalidate or re-permission the sealed bpf and dm-verity
# keyrings init froze or the certificate keys they hold, while viewing
# and listing them and loading against them keep working.

cd "$(dirname "$0")/../.."
. ./integration-lib.sh
target_guard

command -v keyctl >/dev/null 2>&1 || test_skip_all "no keyctl"
RING=$("$SHIRUDO_BIN" status --json 2>/dev/null | report_field keyring_serial)
case "$RING" in
	''|None|0|-*) test_skip_all "no anchor keyring frozen this boot" ;;
esac
KEYS=$(keyctl rlist "$RING" 2>/dev/null)
[ -n "$KEYS" ] || test_skip_all "cannot list the anchor keyring $RING"
KEY=$(set -- $KEYS; echo "$1")
N_BEFORE=$(set -- $KEYS; echo $#)
keys_now() { set -- $(keyctl rlist "$RING" 2>/dev/null); echo $#; }
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT

test_expect_success 'the anchor keyring and its keys are visible' '
	keyctl describe "$RING" >/dev/null &&
	keyctl describe "$KEY" >/dev/null &&
	keyctl rlist "$RING" | grep -q "$KEY" &&
	grep -q "^0*$(printf %08x "$KEY") " /proc/keys
'

test_expect_success 'root cannot unlink a key from the anchor keyring' '
	! keyctl unlink "$KEY" "$RING" 2>"$W/err" &&
	grep -qi "permission denied" "$W/err" &&
	[ "$(keys_now)" = "$N_BEFORE" ]
'

test_expect_success 'root cannot clear the anchor keyring' '
	! keyctl clear "$RING" 2>/dev/null &&
	[ "$(keys_now)" = "$N_BEFORE" ]
'

test_expect_success 'root cannot revoke or invalidate a key' '
	! keyctl revoke "$KEY" 2>/dev/null &&
	! keyctl invalidate "$KEY" 2>/dev/null &&
	! keyctl invalidate "$RING" 2>/dev/null &&
	keyctl describe "$KEY" >/dev/null &&
	[ "$(keys_now)" = "$N_BEFORE" ]
'

test_expect_success 'root cannot change the permissions or the timeout' '
	! keyctl setperm "$KEY" 0x3f3f3f3f 2>/dev/null &&
	! keyctl setperm "$RING" 0x3f3f3f3f 2>/dev/null &&
	! keyctl timeout "$KEY" 1 2>/dev/null &&
	! keyctl chown "$KEY" 1 2>/dev/null &&
	sleep 2 &&
	keyctl describe "$KEY" >/dev/null
'

test_expect_success 'root cannot link a key into the anchor keyring' '
	k=$(keyctl add user shirudo-test-t6112 x @s) &&
	! keyctl link "$k" "$RING" 2>/dev/null &&
	keyctl unlink "$k" @s &&
	[ "$(keys_now)" = "$N_BEFORE" ]
'

DM_RING=$(awk '$8 == "keyring" && $9 == ".dm-verity:" { print $1 }' /proc/keys)
[ -n "$DM_RING" ] && DM_RING=$((0x$DM_RING))
DM_KEY=$([ -n "$DM_RING" ] && keyctl rlist "$DM_RING" 2>/dev/null)
dm_keys_now() { set -- $(keyctl rlist "$DM_RING" 2>/dev/null); echo $#; }

test_expect_success 'the dm-verity keyring holds the config key alone' '
	[ -n "$DM_RING" ] && [ -n "$DM_KEY" ] &&
	[ "$(dm_keys_now)" = 1 ] &&
	keyctl rdescribe "$DM_KEY" | grep -q "^asymmetric;"
'

test_expect_success 'root cannot unlink, clear, revoke or invalidate it' '
	[ -n "$DM_RING" ] && [ -n "$DM_KEY" ] &&
	! keyctl unlink "$DM_KEY" "$DM_RING" 2>/dev/null &&
	! keyctl clear "$DM_RING" 2>/dev/null &&
	! keyctl revoke "$DM_KEY" 2>/dev/null &&
	! keyctl revoke "$DM_RING" 2>/dev/null &&
	! keyctl invalidate "$DM_KEY" 2>/dev/null &&
	! keyctl invalidate "$DM_RING" 2>/dev/null &&
	keyctl describe "$DM_KEY" >/dev/null &&
	[ "$(dm_keys_now)" = 1 ]
'

test_expect_success 'root cannot re-permission it or link into it' '
	[ -n "$DM_RING" ] && [ -n "$DM_KEY" ] &&
	! keyctl setperm "$DM_KEY" 0x3f3f3f3f 2>/dev/null &&
	! keyctl setperm "$DM_RING" 0x3f3f3f3f 2>/dev/null &&
	! keyctl timeout "$DM_KEY" 1 2>/dev/null &&
	! keyctl timeout "$DM_RING" 1 2>/dev/null &&
	! keyctl chown "$DM_RING" 1 2>/dev/null &&
	k=$(keyctl add user shirudo-test-t6112 x @s) &&
	! keyctl link "$k" "$DM_RING" 2>/dev/null &&
	keyctl unlink "$k" @s &&
	sleep 2 &&
	keyctl describe "$DM_RING" >/dev/null &&
	[ "$(dm_keys_now)" = 1 ]
'

test_expect_success 'status still finds the anchor it froze' '
	m=$("$SHIRUDO_BIN" status --json 2>/dev/null | report_field keyring_serial_matches) &&
	[ "$m" = "True" ]
'

test_done
