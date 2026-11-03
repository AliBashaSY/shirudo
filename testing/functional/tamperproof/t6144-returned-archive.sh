#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t6144 -- an archive root brings back is not shirudo's to restore. A
# labeled archive hard-linked out before its unload and back into the
# archive directory while the directory's mark is evicted (R15) carries
# the label, but the eventlog records its unload: status reports it,
# init does not restore it and the next upatch command removes it.

cd "$(dirname "$0")/../.."
. ./integration-lib.sh
target_guard
command -v python3 >/dev/null 2>&1 || test_skip_all "no python3"
FIXTURES=$SHIRUDO_TEST_FIXTURES_DIR
NAME=shirudo-test-basic
OTHER=shirudo-test-marker
LOAD="$FIXTURES/$NAME.upatch"
UNLOAD="$FIXTURES/$NAME.unload.upatch"
PINDIR="/run/shirudo/bpffs/upatches/$NAME"
OTHERDIR="/run/shirudo/bpffs/upatches/$OTHER"
ARCHIVE="/var/lib/shirudo/upatches/$NAME.upatch"
KEEPDIR=/var/lib/t6144
KEEP="$KEEPDIR/keep.upatch"
W=$(mktemp -d)
cleanup() {
	[ ! -e "$OTHERDIR" ] ||
		"$SHIRUDO_BIN" upatch unload "$FIXTURES/$OTHER.unload.upatch" \
			>/dev/null 2>&1 || :
	[ ! -e "$PINDIR" ] ||
		"$SHIRUDO_BIN" upatch unload "$UNLOAD" >/dev/null 2>&1 || :
	# The kept link carries the label, so root cannot unlink it: the
	# directory stays behind with it.
	rm -rf "$KEEPDIR" "$W" 2>/dev/null || :
}
trap cleanup EXIT
for f in "$LOAD" "$UNLOAD" "$FIXTURES/$OTHER.upatch" \
	 "$FIXTURES/$OTHER.unload.upatch"; do
	[ -s "$f" ] || test_skip_all "missing fixture $f"
done
[ ! -e "$PINDIR" ] || "$SHIRUDO_BIN" upatch unload "$UNLOAD" >/dev/null 2>&1
[ ! -e "$OTHERDIR" ] ||
	"$SHIRUDO_BIN" upatch unload "$FIXTURES/$OTHER.unload.upatch" \
		>/dev/null 2>&1
for d in "$PINDIR" "$OTHERDIR"; do
	[ ! -e "$d" ] || test_skip_all "$(basename "$d") cannot be unloaded"
done
[ ! -e "$ARCHIVE" ] || test_skip_all "$NAME is archived already"
mkdir -p "$KEEPDIR"

label_len() {
	python3 -c 'import os, sys
print(len(os.getxattr(sys.argv[1], "security.bpf.shirudo")))' "$1"
}

# The verdict and the UpatchesMeasured condition of a status document.
measured_condition() {
	python3 - "$1" <<'PY'
import json, sys
d = json.load(open(sys.argv[1], encoding="utf-8"))
c = [c for c in d["conditions"] if c["type"] == "UpatchesMeasured"]
print(d["status"], c[0]["status"] if c else "-", c[0]["detail"] if c else "-")
PY
}

test_expect_success 'a labeled archive can be hard-linked out of the archive directory' '
	"$SHIRUDO_BIN" upatch load "$LOAD" >/dev/null 2>&1 &&
	[ -s "$ARCHIVE" ] &&
	ln "$ARCHIVE" "$KEEP" &&
	[ "$(label_len "$KEEP")" = 64 ]
'
test_expect_success 'after its unload the archive returns through the link once the mark is evicted' '
	"$SHIRUDO_BIN" upatch unload "$UNLOAD" >/dev/null 2>&1 &&
	[ ! -e "$ARCHIVE" ] &&
	! ln "$KEEP" "$ARCHIVE" 2>/dev/null &&
	sync &&
	echo 2 >/proc/sys/vm/drop_caches &&
	ln "$KEEP" "$ARCHIVE" &&
	[ "$(label_len "$ARCHIVE")" = 64 ]
'
test_expect_success 'status reports the returned archive as tampered' '
	"$SHIRUDO_BIN" status --json >"$W/status.json" 2>/dev/null &&
	measured_condition "$W/status.json" >"$W/cond" &&
	read -r verdict cond detail <"$W/cond" &&
	[ "$verdict" = tampered ] &&
	[ "$cond" = ALERT ] &&
	case "$detail" in *"$NAME"*) true ;; *) false ;; esac
'
test_expect_success 'init does not restore the returned archive' '
	"$SHIRUDO_BIN" init >/dev/null 2>"$W/init.err" &&
	grep -q "$NAME.upatch is archived without a record of its load" \
		"$W/init.err" &&
	[ ! -e "$PINDIR" ] &&
	[ -e "$ARCHIVE" ]
'
test_expect_success 'the next upatch command removes the returned archive' '
	"$SHIRUDO_BIN" upatch load "$FIXTURES/$OTHER.upatch" >/dev/null 2>&1 &&
	[ ! -e "$ARCHIVE" ] &&
	[ -e "$OTHERDIR/link" ] &&
	"$SHIRUDO_BIN" status --json >"$W/status.json" 2>/dev/null &&
	measured_condition "$W/status.json" >"$W/cond" &&
	read -r verdict cond detail <"$W/cond" &&
	[ "$verdict" != tampered ] &&
	[ "$cond" = OK ] &&
	"$SHIRUDO_BIN" upatch unload "$FIXTURES/$OTHER.unload.upatch" \
		>/dev/null 2>&1 &&
	[ ! -e "$OTHERDIR" ]
'
test_done
