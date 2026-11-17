#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t6154 -- bpf2 refuses what lockdown in integrity mode refuses, whatever
# the command line says, and /proc/kcore with it: the reads and writes of
# the running kernel fail with EPERM and count as Lockdown, while the
# hibernation check every read of /sys/power/state makes counts nothing.

cd "$(dirname "$0")/../.."
. ./integration-lib.sh
target_guard
command -v python3 >/dev/null 2>&1 || test_skip_all "no python3"
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
counter() {
	"$SHIRUDO_BIN" status --json | python3 -c '
import json, sys
print(json.load(sys.stdin)["tamper_attempts"]["counters"][sys.argv[1]])
' "$1"
}
# Prints the errno name an open of $1 for reading fails with, or done.
open_errno() {
	python3 -c '
import errno, os, sys
try:
    os.close(os.open(sys.argv[1], os.O_RDONLY))
    print("done")
except OSError as e:
    print(errno.errorcode[e.errno])
' "$1"
}
before=$(counter Lockdown) || test_skip_all "no tamper counters"

if [ -e /proc/kcore ]; then
	test_expect_success '/proc/kcore is refused and counted' '
		[ "$(open_errno /proc/kcore)" = EPERM ] &&
		[ "$(counter Lockdown)" -gt "$before" ]
	'
fi
CONFIG=$(ls /sys/bus/pci/devices/*/config 2>/dev/null | head -1)
if [ -n "$CONFIG" ]; then
	# The interrupt line byte written back as it is, should the write pass.
	test_expect_success 'a PCI config write is refused and counted' '
		now=$(counter Lockdown) &&
		python3 - "$CONFIG" >"$W/pci" <<-\PY &&
		import errno, sys
		with open(sys.argv[1], "r+b", buffering=0) as f:
		    f.seek(0x3c)
		    line = f.read(1)
		    f.seek(0x3c)
		    try:
		        f.write(line)
		        print("done")
		    except OSError as e:
		        print(errno.errorcode[e.errno])
		PY
		grep -qx EPERM "$W/pci" &&
		[ "$(counter Lockdown)" -gt "$now" ]
	'
fi
if [ -e /dev/mem ]; then
	test_expect_success '/dev/mem is refused' '
		[ "$(open_errno /dev/mem)" = EPERM ]
	'
fi
test_expect_success 'reading /sys/power/state counts nothing' '
	now=$(counter Lockdown) &&
	cat /sys/power/state >/dev/null 2>&1 || :
	[ "$(counter Lockdown)" -eq "$now" ]
'
test_done
