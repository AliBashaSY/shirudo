#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t1009 -- the upatch build compiles for BPF instruction set v4, which
# load-acquire and store-release need, unless --extra-cflags names another
# -mcpu. A sign-extending load is one instruction, BPF_MEMSX, only in v4,
# and every clang that builds shirudo emits it there.

cd "$(dirname "$0")/../.."
. ./test-lib.sh
S=../shirudo
SHB=../tools/shirudo-build/shirudo-build
command -v clang >/dev/null 2>&1 || test_skip_all "no clang"
command -v python3 >/dev/null 2>&1 || test_skip_all "no python3"
[ -x "$S" ] || test_skip_all "shirudo not built"
[ -x "$SHB" ] || test_skip_all "shirudo-build not built (make shirudo-build)"
[ -e ../vendor/linux/vmlinux.h ] || test_skip_all "no vendor/linux/vmlinux.h"
W=$(mktemp -d)
trap 'rm -rf "$W"' EXIT
$S genkey --keys-dir "$W/keys" >/dev/null 2>&1 ||
	test_skip_all "genkey failed (OpenSSL without ML-DSA?)"

cat >"$W/isa.bpf.c" <<'EOF'
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>

char _license[] SEC("license") = "Dual BSD/GPL";

volatile __s8 shirudo_test_isa_byte;
__s64 shirudo_test_isa_wide;

SEC("fentry/do_nanosleep")
int BPF_PROG(shirudo_test_isa)
{
	shirudo_test_isa_wide = shirudo_test_isa_byte;
	return 0;
}
EOF

# Builds $W/<name>.upatch and keeps the object as $W/<name>.o.
build() {
	name=$1
	shift
	(cd .. && tools/shirudo-build/shirudo-build "$W/isa.bpf.c" \
		-o "$W/$name.upatch" --name "t1009-$name" \
		--vmlinux vendor/linux/vmlinux.h --keys-dir "$W/keys" \
		--emit-artifacts "$@") >"$W/$name.log" 2>&1
}

# Whether an executable section of the BPF object holds a BPF_MEMSX load.
memsx() {
	python3 - "$1" <<'EOF'
import struct
import sys

data = open(sys.argv[1], "rb").read()
shoff, = struct.unpack_from("<Q", data, 0x28)
shentsize, shnum = struct.unpack_from("<HH", data, 0x3a)
for i in range(shnum):
    sh = shoff + i * shentsize
    flags, = struct.unpack_from("<Q", data, sh + 0x08)
    offset, size = struct.unpack_from("<QQ", data, sh + 0x18)
    if not flags & 0x4:
        continue
    for pc in range(offset, offset + size, 8):
        if data[pc] in (0x81, 0x89, 0x91):
            sys.exit(0)
sys.exit(1)
EOF
}

test_expect_success 'the upatch build compiles for BPF instruction set v4' '
	build default &&
	[ -s "$W/default.upatch" ] &&
	memsx "$W/default.o"
'

test_expect_success '--extra-cflags overrides the instruction set' '
	build v3 --extra-cflags -mcpu=v3 &&
	[ -s "$W/v3.upatch" ] &&
	! memsx "$W/v3.o"
'

test_done
