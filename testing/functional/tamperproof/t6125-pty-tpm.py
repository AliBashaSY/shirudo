#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# A TPM 2.0 emulator behind a pseudo-terminal, whose slave is a character
# device a bind mount can put at /dev/tpmrm0; or, as probe, one command
# sent to a device path to show the emulator answers there.

import os
import pty
import select
import struct
import sys
import tty


def probe(path):
    fd = os.open(path, os.O_RDWR | os.O_NONBLOCK)
    # TPM2_GetCapability(TPM_CAP_TPM_PROPERTIES, TPM_PT_MANUFACTURER, 1)
    cmd = struct.pack(">HII", 0x8001, 22, 0x17A) + struct.pack(">III", 6, 0x105, 1)
    if os.write(fd, cmd) != len(cmd):
        return 1
    if not select.select([fd], [], [], 10)[0]:
        return 1
    rsp = os.read(fd, 4096)
    tag, size, rc = struct.unpack(">HII", rsp[:10])
    return 0 if tag == 0x8001 and size == len(rsp) and rc == 0 else 1


def serve(state, swtpm):
    master, slave = pty.openpty()
    tty.setraw(master)
    tty.setraw(slave)
    # The emulator keeps the slave open too, or the master hangs up.
    os.set_inheritable(master, True)
    os.set_inheritable(slave, True)
    with open(os.path.join(state, "slave"), "w") as f:
        f.write(os.ttyname(slave))
    os.environ["LD_LIBRARY_PATH"] = swtpm
    os.execv(os.path.join(swtpm, "swtpm"), [
        "swtpm", "chardev", "--tpm2", "--fd", str(master),
        "--tpmstate", "dir=" + state,
        "--flags", "not-need-init,startup-clear",
        "--log", "file=" + os.path.join(state, "log") + ",level=1"])


if sys.argv[1] == "probe":
    sys.exit(probe(sys.argv[2]))
serve(sys.argv[1], sys.argv[2])
