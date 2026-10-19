# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo

import ctypes
import errno
import os
import platform
import sys

PR_SET_MM = 35
PR_SET_MM_EXE_FILE = 13
PR_SET_MM_MAP = 14
SHIRUDO = os.path.abspath(sys.argv[1])
SHIRUDO_ID = os.stat(SHIRUDO).st_dev, os.stat(SHIRUDO).st_ino
OTHER = next(os.path.realpath(path) for path in
             ("/bin/true", "/usr/bin/true", sys.executable)
             if os.path.isfile(path) and os.access(path, os.X_OK) and
             (os.stat(path).st_dev, os.stat(path).st_ino) != SHIRUDO_ID)

libc = ctypes.CDLL(None, use_errno=True)
libc.prctl.argtypes = [ctypes.c_int, ctypes.c_ulong, ctypes.c_ulong,
                       ctypes.c_ulong, ctypes.c_ulong]
libc.prctl.restype = ctypes.c_int
libc.syscall.restype = ctypes.c_long

BPF_SYSCALL = {"x86_64": 321, "aarch64": 280,
               "riscv64": 280}[platform.machine()]
BPF_OBJ_GET = 7
BPF_MAP_LOOKUP_ELEM = 1
BPF_F_RDONLY = 1 << 3


class ObjGetAttr(ctypes.Structure):
    _fields_ = [("pathname", ctypes.c_uint64),
                ("bpf_fd", ctypes.c_uint32),
                ("file_flags", ctypes.c_uint32)]


class MapElemAttr(ctypes.Structure):
    _fields_ = [("map_fd", ctypes.c_uint32),
                ("pad", ctypes.c_uint32),
                ("key", ctypes.c_uint64),
                ("value", ctypes.c_uint64),
                ("flags", ctypes.c_uint64)]


class ShirudoIdentity(ctypes.Structure):
    _fields_ = [("sb_dev", ctypes.c_uint32),
                ("exe_dev", ctypes.c_uint32),
                ("exe_ino", ctypes.c_uint64),
                ("keyring_serial", ctypes.c_int32),
                ("keyring_type", ctypes.c_uint32),
                ("alt_exe_dev", ctypes.c_uint32),
                ("pad", ctypes.c_uint32),
                ("alt_exe_ino", ctypes.c_uint64),
                ("n_keys", ctypes.c_uint32),
                ("pad2", ctypes.c_uint32),
                ("key_serials", ctypes.c_int32 * 16)]


def assert_frozen_executable_identity():
    path = b"/run/shirudo/bpffs/identity\0"
    name = ctypes.create_string_buffer(path)
    get_attr = ObjGetAttr(ctypes.addressof(name), 0, BPF_F_RDONLY)
    fd = libc.syscall(BPF_SYSCALL, BPF_OBJ_GET, ctypes.byref(get_attr),
                      ctypes.sizeof(get_attr))
    assert fd >= 0, ("open identity map", ctypes.get_errno())
    try:
        key = ctypes.c_uint32(0)
        value = ShirudoIdentity()
        lookup_attr = MapElemAttr(fd, 0, ctypes.addressof(key),
                                  ctypes.addressof(value), 0)
        result = libc.syscall(BPF_SYSCALL, BPF_MAP_LOOKUP_ELEM,
                              ctypes.byref(lookup_attr),
                              ctypes.sizeof(lookup_attr))
        assert result == 0, ("read identity map", ctypes.get_errno())
    finally:
        os.close(fd)
    st = os.stat(SHIRUDO)
    expected_dev = (os.major(st.st_dev) << 20) | os.minor(st.st_dev)
    assert (value.exe_dev, value.exe_ino) == (expected_dev, st.st_ino), \
        ((value.exe_dev, value.exe_ino), (expected_dev, st.st_ino))


class PrctlMmMap(ctypes.Structure):
    _fields_ = [("start_code", ctypes.c_uint64),
                ("end_code", ctypes.c_uint64),
                ("start_data", ctypes.c_uint64),
                ("end_data", ctypes.c_uint64),
                ("start_brk", ctypes.c_uint64),
                ("brk", ctypes.c_uint64),
                ("start_stack", ctypes.c_uint64),
                ("arg_start", ctypes.c_uint64),
                ("arg_end", ctypes.c_uint64),
                ("env_start", ctypes.c_uint64),
                ("env_end", ctypes.c_uint64),
                ("auxv", ctypes.c_void_p),
                ("auxv_size", ctypes.c_uint32),
                ("exe_fd", ctypes.c_uint32)]


def proc_stat_values():
    stat = open("/proc/self/stat", encoding="ascii").read()
    fields = stat[stat.rfind(")") + 2:].split()
    indexes = {"start_code": 23, "end_code": 24, "start_stack": 25,
               "start_data": 42, "end_data": 43, "start_brk": 44,
               "brk": 45, "arg_start": 46, "arg_end": 47,
               "env_start": 48, "env_end": 49}
    return {name: int(fields[index]) for name, index in indexes.items()}


def current_mm_map(exe_fd):
    values = proc_stat_values()
    auxv_data = open("/proc/self/auxv", "rb").read()
    auxv = ctypes.create_string_buffer(auxv_data)
    values["auxv"] = ctypes.addressof(auxv)
    values["auxv_size"] = len(auxv_data)
    values["exe_fd"] = exe_fd
    return PrctlMmMap(**values), auxv


def call_prctl(option, arg2, arg3=0, arg4=0, arg5=0):
    ctypes.set_errno(0)
    result = libc.prctl(option, arg2, arg3, arg4, arg5)
    return result, ctypes.get_errno()


def file_identity(path):
    st = os.stat(path)
    return st.st_dev, st.st_ino


def child_succeeds(fn):
    pid = os.fork()
    if pid == 0:
        try:
            fn()
        except Exception as exc:
            print(f"child assertion failed: {exc}", file=sys.stderr)
            os._exit(1)
        os._exit(0)
    _, status = os.waitpid(pid, 0)
    return os.WIFEXITED(status) and os.WEXITSTATUS(status) == 0


def exe_file_result(path):
    before = file_identity("/proc/self/exe")
    fd = os.open(path, os.O_RDONLY | os.O_CLOEXEC)
    try:
        result, err = call_prctl(PR_SET_MM, PR_SET_MM_EXE_FILE, fd)
    finally:
        os.close(fd)
    after = file_identity("/proc/self/exe")
    assert result == -1 and err == errno.EPERM, (result, err)
    assert after == before, (before, after)


def mm_map_result(path):
    before = file_identity("/proc/self/exe")
    fd = os.open(path, os.O_RDONLY | os.O_CLOEXEC)
    try:
        mm, auxv = current_mm_map(fd)
        result, err = call_prctl(PR_SET_MM, PR_SET_MM_MAP,
                                 ctypes.addressof(mm), ctypes.sizeof(mm))
        assert auxv is not None
    finally:
        os.close(fd)
    after = file_identity("/proc/self/exe")
    assert result == -1 and err == errno.EPERM, (result, err)
    assert after == before, (before, after)


assert_frozen_executable_identity()

assert child_succeeds(lambda: exe_file_result(SHIRUDO)), \
    "PR_SET_MM_EXE_FILE was allowed for a non-Shirudo caller"
assert child_succeeds(lambda: exe_file_result(OTHER)), \
    "PR_SET_MM_EXE_FILE accepted an unrelated executable"
assert child_succeeds(lambda: mm_map_result(SHIRUDO)), \
    "PR_SET_MM_MAP was allowed for a non-Shirudo caller"
assert child_succeeds(lambda: mm_map_result(OTHER)), \
    "PR_SET_MM_MAP accepted an unrelated executable"


def invalid_fd_then_exec():
    result, err = call_prctl(PR_SET_MM, PR_SET_MM_EXE_FILE, 1 << 30)
    assert result == -1 and err == errno.EPERM, (result, err)
    os.execve(SHIRUDO, [SHIRUDO, "status", "--json"], os.environ.copy())


assert child_succeeds(invalid_fd_then_exec), \
    "failed PR_SET_MM left state that blocked ordinary Shirudo execution"
print("PR_SET_MM identity guard passed")
