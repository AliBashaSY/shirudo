#!/bin/sh
# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo
#
# t1130 -- direct BPF syscalls from a root process with a different
# executable cannot acquire writable map fds or mutate Shirudo's maps.
# PR_SET_MM executable spoofing is covered separately by t1131.

cd "$(dirname "$0")/.."
. ./integration-lib.sh
target_guard
command -v python3 >/dev/null 2>&1 || test_skip_all "no python3"

FIXTURES=${SHIRUDO_TEST_FIXTURES_DIR:?target fixture directory is not set}
NAME=kernel-obs
LOAD="$FIXTURES/$NAME.upatch"
UNLOAD="$FIXTURES/$NAME.unload.upatch"
PIN="/run/shirudo/bpffs/upatches/$NAME/link_0"
MAPS="/run/shirudo/bpffs/upatches/$NAME/maps"
WORK=$(mktemp -d)

cleanup()
{
	if [ -e "$PIN" ] || [ -e "/run/shirudo/bpffs/upatches/$NAME/link" ]; then
		"$SHIRUDO_BIN" upatch unload "$UNLOAD" --no-precheck \
			>/dev/null 2>&1 || true
	fi
	rm -f "/var/lib/shirudo/upatches/$NAME.upatch"
	rm -rf "$WORK"
}
trap cleanup EXIT HUP INT TERM

[ -s "$LOAD" ] || test_skip_all "missing kernel-obs fixture"
[ -s "$UNLOAD" ] || test_skip_all "missing kernel-obs unload fixture"

cat >"$WORK/check.py" <<'PYTHON'
import ctypes
import errno
import json
import os
import platform
import subprocess
import sys

BPF_MAP_UPDATE_ELEM = 2
BPF_PROG_TEST_RUN = 10
BPF_MAP_FREEZE = 22
BPF_MAP_GET_FD_BY_ID = 14
BPF_OBJ_GET = 7
BPF_OBJ_GET_INFO_BY_FD = 15
BPF_F_RDONLY = 1 << 3
BPF_ANY = 0
PROT_READ = 1
PROT_WRITE = 2
MAP_SHARED = 1
PAGE_SIZE = os.sysconf("SC_PAGE_SIZE")
SYSCALLS = {"x86_64": 321, "aarch64": 280, "riscv64": 280}

try:
    BPF_SYSCALL = SYSCALLS[platform.machine()]
except KeyError as exc:
    machine = platform.machine()
    raise SystemExit(f"unsupported syscall architecture: {machine}") from exc

libc = ctypes.CDLL(None, use_errno=True)
libc.syscall.restype = ctypes.c_long
libc.mmap.restype = ctypes.c_void_p
libc.munmap.argtypes = [ctypes.c_void_p, ctypes.c_size_t]


class ObjGetAttr(ctypes.Structure):
    _fields_ = [("pathname", ctypes.c_uint64),
                ("bpf_fd", ctypes.c_uint32),
                ("file_flags", ctypes.c_uint32)]


class ObjInfoAttr(ctypes.Structure):
    _fields_ = [("bpf_fd", ctypes.c_uint32),
                ("info_len", ctypes.c_uint32),
                ("info", ctypes.c_uint64)]


class MapInfoPrefix(ctypes.Structure):
    _fields_ = [("type", ctypes.c_uint32),
                ("id", ctypes.c_uint32),
                ("key_size", ctypes.c_uint32),
                ("value_size", ctypes.c_uint32),
                ("max_entries", ctypes.c_uint32),
                ("map_flags", ctypes.c_uint32)]


class MapFdByIdAttr(ctypes.Structure):
    _fields_ = [("map_id", ctypes.c_uint32),
                ("next_id", ctypes.c_uint32),
                ("open_flags", ctypes.c_uint32)]


class MapElemAttr(ctypes.Structure):
    _fields_ = [("map_fd", ctypes.c_uint32),
                ("pad", ctypes.c_uint32),
                ("key", ctypes.c_uint64),
                ("value", ctypes.c_uint64),
                ("flags", ctypes.c_uint64)]


class SerializerCtx(ctypes.Structure):
    _fields_ = [("magic", ctypes.c_uint32),
                ("version", ctypes.c_uint32),
                ("flags", ctypes.c_uint32),
                ("max_chunks", ctypes.c_uint32),
                ("chunk_payload_size", ctypes.c_uint32),
                ("page_index", ctypes.c_uint32),
                ("cursor", ctypes.c_uint64),
                ("invocation_id", ctypes.c_uint64)]


class ProgTestRunAttr(ctypes.Structure):
    _fields_ = [("prog_fd", ctypes.c_uint32),
                ("retval", ctypes.c_uint32),
                ("data_size_in", ctypes.c_uint32),
                ("data_size_out", ctypes.c_uint32),
                ("data_in", ctypes.c_uint64),
                ("data_out", ctypes.c_uint64),
                ("repeat", ctypes.c_uint32),
                ("duration", ctypes.c_uint32),
                ("ctx_size_in", ctypes.c_uint32),
                ("ctx_size_out", ctypes.c_uint32),
                ("ctx_in", ctypes.c_uint64),
                ("ctx_out", ctypes.c_uint64),
                ("flags", ctypes.c_uint32),
                ("cpu", ctypes.c_uint32),
                ("batch_size", ctypes.c_uint32)]


def bpf(cmd, attr):
    ctypes.set_errno(0)
    result = libc.syscall(BPF_SYSCALL, cmd, ctypes.byref(attr),
                          ctypes.sizeof(attr))
    return result, ctypes.get_errno()


def obj_get(path, flags):
    encoded = os.fsencode(path) + b"\0"
    name = ctypes.create_string_buffer(encoded)
    attr = ObjGetAttr(ctypes.addressof(name), 0, flags)
    return bpf(BPF_OBJ_GET, attr)


def map_id_from_fd(fd):
    info = MapInfoPrefix()
    attr = ObjInfoAttr(fd, ctypes.sizeof(info), ctypes.addressof(info))
    result, error = bpf(BPF_OBJ_GET_INFO_BY_FD, attr)
    if result < 0:
        raise AssertionError(f"map info: errno={error}")
    return info.id


def map_fd_by_id(map_id, flags):
    attr = MapFdByIdAttr(map_id, 0, flags)
    return bpf(BPF_MAP_GET_FD_BY_ID, attr)


def assert_denied(result, label):
    fd, error = result
    if fd >= 0:
        os.close(fd)
        raise AssertionError(f"{label}: unexpectedly allowed")
    if error not in (errno.EACCES, errno.EPERM):
        raise AssertionError(f"{label}: errno={error}, expected EACCES/EPERM")


for map_name in ("shirudo_ser", "obs_i_arena"):
    path = os.path.join(sys.argv[1], map_name)
    assert_denied(obj_get(path, 0), f"writable fd for {map_name}")

    readonly_fd, error = obj_get(path, BPF_F_RDONLY)
    if readonly_fd < 0:
        raise AssertionError(f"read-only fd for {map_name}: errno={error}")
    try:
        map_id = map_id_from_fd(readonly_fd)
        assert_denied(map_fd_by_id(map_id, 0),
                      f"writable fd by id for {map_name}")
        readonly_id_fd, error = map_fd_by_id(map_id, BPF_F_RDONLY)
        if readonly_id_fd < 0:
            raise AssertionError(
                f"read-only fd by id for {map_name}: errno={error}")
        os.close(readonly_id_fd)

        freeze_arg = ctypes.c_uint32(readonly_fd)
        result, error = bpf(BPF_MAP_FREEZE, freeze_arg)
        if result == 0 or error != errno.EACCES:
            raise AssertionError(
                f"freeze {map_name}: result={result}, errno={error}; "
                "expected LSM EACCES")
        if map_name == "shirudo_ser":
            key = ctypes.c_uint32(3)
            value = ctypes.create_string_buffer(PAGE_SIZE)
            attr = MapElemAttr(readonly_fd, 0, ctypes.addressof(key),
                               ctypes.addressof(value), BPF_ANY)
            result, error = bpf(BPF_MAP_UPDATE_ELEM, attr)
            if result == 0 or error != errno.EACCES:
                raise AssertionError(
                    f"serializer map update: result={result}, errno={error}; "
                    "expected LSM EACCES")
        else:
            ctypes.set_errno(0)
            mapped = libc.mmap(None, PAGE_SIZE, PROT_READ | PROT_WRITE,
                               MAP_SHARED, readonly_fd, 0)
            error = ctypes.get_errno()
            if mapped != ctypes.c_void_p(-1).value:
                libc.munmap(mapped, PAGE_SIZE)
                raise AssertionError("arena writable mmap unexpectedly allowed")
            if error not in (errno.EACCES, errno.EPERM):
                raise AssertionError(
                    f"arena writable mmap: errno={error}, expected EACCES/EPERM")
    finally:
        os.close(readonly_fd)

serializer_path = os.path.join(os.path.dirname(sys.argv[1]),
                               "serializers/exec")
serializer_fd, error = obj_get(serializer_path, 0)
if serializer_fd < 0:
    raise AssertionError(f"exec serializer fd: errno={error}")
try:
    ctx = SerializerCtx(0x5A4C5253, 3, 0, 1, 4032, 0, 0, 1)
    attr = ProgTestRunAttr(serializer_fd, 0, 0, 0, 0, 0, 1, 0,
                           ctypes.sizeof(ctx), 0,
                           ctypes.addressof(ctx), 0, 0, 0, 0)
    result, error = bpf(BPF_PROG_TEST_RUN, attr)
    if result == 0 or error != errno.EACCES:
        raise AssertionError(
            f"serializer BPF_PROG_TEST_RUN: result={result}, errno={error}; "
            "expected LSM EACCES")
finally:
    os.close(serializer_fd)

output = os.path.join(sys.argv[3], "exec-after-test-run.ndjson")
subprocess.run([sys.argv[2], "upatch", "map", "kernel-obs", "exec",
                f"--json={output}"], check=True, stdout=subprocess.DEVNULL)
documents = [json.loads(line) for line in open(output, encoding="utf-8")]
records = [record for document in documents
           if isinstance(document, dict) and document.get("map") == "exec"
           for record in document.get("records", [])]
python_exe = os.path.realpath(sys.executable)
if not any(record.get("path") == python_exe for record in records):
    raise AssertionError(
        f"Python exec record was lost after denied serializer run: {records!r}")

print("root BPF map mutation and serializer test-run denied; export preserved")
PYTHON

test_expect_success 'root map mutation and serializer test-run are denied' '
	"$SHIRUDO_BIN" upatch load "$LOAD" --no-precheck >/dev/null 2>&1 &&
	python3 "$WORK/check.py" "$MAPS" "$SHIRUDO_BIN" "$WORK"
'

test_done
