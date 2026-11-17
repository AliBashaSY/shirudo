#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t6154 -- bpf2 refuses what lockdown in integrity mode refuses, whatever
# the command line says, and /proc/kcore with it: an unsigned module and
# the reads and writes of the running kernel fail with EPERM and count as
# Lockdown, while the hibernation check every read of /sys/power/state
# makes counts nothing.

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
counter Lockdown >/dev/null || test_skip_all "no tamper counters"

# The guest's modules are signed; a copy without the signature is what the
# kernel asks lockdown about.
test_expect_success 'a module without its signature is refused and counted' '
	module=$(find "/lib/modules/$(uname -r)" -name "*.ko" | head -1) &&
	[ -n "$module" ] &&
	now=$(counter Lockdown) &&
	python3 - "$module" "$W/unsigned.ko" >"$W/module" <<-\PY &&
	import ctypes, errno, os, platform, sys
	MARKER = b"~Module signature appended~\n"
	FINIT_MODULE = {"x86_64": 313, "aarch64": 273, "riscv64": 273}
	data = open(sys.argv[1], "rb").read()
	if not data.endswith(MARKER):
	    sys.exit("not signed: " + sys.argv[1])
	sig_len = int.from_bytes(data[-len(MARKER) - 4:-len(MARKER)], "big")
	with open(sys.argv[2], "wb") as f:
	    f.write(data[:len(data) - len(MARKER) - 12 - sig_len])
	fd = os.open(sys.argv[2], os.O_RDONLY)
	libc = ctypes.CDLL(None, use_errno=True)
	ret = libc.syscall(FINIT_MODULE[platform.machine()], fd, b"", 0)
	print("done" if ret == 0 else errno.errorcode[ctypes.get_errno()])
	PY
	grep -qx EPERM "$W/module" &&
	[ "$(counter Lockdown)" -gt "$now" ]
'

if [ -e /proc/kcore ]; then
	test_expect_success '/proc/kcore is refused and counted' '
		now=$(counter Lockdown) &&
		[ "$(open_errno /proc/kcore)" = EPERM ] &&
		[ "$(counter Lockdown)" -gt "$now" ]
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
