# SPDX-License-Identifier: BSD-2-Clause
# Copyright (c) 2026 Authors of shirudo

import ctypes
import errno
import os
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


def exe_file_result(path, expect_denied):
    before = file_identity("/proc/self/exe")
    fd = os.open(path, os.O_RDONLY | os.O_CLOEXEC)
    try:
        result, err = call_prctl(PR_SET_MM, PR_SET_MM_EXE_FILE, fd)
    finally:
        os.close(fd)
    after = file_identity("/proc/self/exe")
    if expect_denied:
        assert result == -1 and err == errno.EPERM, (result, err)
        assert after == before, (before, after)
    else:
        assert result == 0, (result, err)
        assert after == file_identity(path), (after, file_identity(path))


def mm_map_result(path, expect_denied):
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
    if expect_denied:
        assert result == -1 and err == errno.EPERM, (result, err)
        assert after == before, (before, after)
    else:
        assert result == 0, (result, err)
        assert after == file_identity(path), (after, file_identity(path))


assert child_succeeds(lambda: exe_file_result(SHIRUDO, True)), \
    "PR_SET_MM_EXE_FILE accepted Shirudo's executable"
assert child_succeeds(lambda: exe_file_result(OTHER, False)), \
    "PR_SET_MM_EXE_FILE rejected an unrelated executable"
assert child_succeeds(lambda: mm_map_result(SHIRUDO, True)), \
    "PR_SET_MM_MAP accepted Shirudo's executable"
assert child_succeeds(lambda: mm_map_result(OTHER, False)), \
    "PR_SET_MM_MAP rejected an unrelated executable"


def invalid_fd_then_exec():
    result, err = call_prctl(PR_SET_MM, PR_SET_MM_EXE_FILE, 1 << 30)
    assert result == -1 and err == errno.EBADF, (result, err)
    os.execve(SHIRUDO, [SHIRUDO, "status", "--json"], os.environ.copy())


assert child_succeeds(invalid_fd_then_exec), \
    "failed PR_SET_MM left state that blocked ordinary Shirudo execution"
print("PR_SET_MM identity guard passed")
