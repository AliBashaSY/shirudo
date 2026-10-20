/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The tamper counters the LSM keeps, read back from their bpffs pin.
 */
#ifndef SHIRUDO_UTIL_TAMPER_H
#define SHIRUDO_UTIL_TAMPER_H

#include <stdint.h>

/*
 * One counter per point at which a guard refuses a caller. Must match
 * enum shirudo_tamper_point in bpf/lib/shirudo_tamper.h in order and
 * number; the reader refuses a map of another size, and the names and
 * meanings status reports are the table in tamper.c.
 */
enum shirudo_tamper_point {
	SHIRUDO_TAMPER_LOAD_NOT_LOADER,
	SHIRUDO_TAMPER_LOAD_UNSIGNED,
	SHIRUDO_TAMPER_LOAD_UNVERIFIED,
	SHIRUDO_TAMPER_LOAD_KEYRING_KIND,
	SHIRUDO_TAMPER_LOAD_KEYRING,
	SHIRUDO_TAMPER_MAP_CREATE,
	SHIRUDO_TAMPER_MAP_WRITE,
	SHIRUDO_TAMPER_MAP_FD,
	SHIRUDO_TAMPER_PROG_RUN,
	SHIRUDO_TAMPER_ATTACH,
	SHIRUDO_TAMPER_OBJ_PIN,
	SHIRUDO_TAMPER_BPF_CMD,
	SHIRUDO_TAMPER_EXE_IDENTITY,
	SHIRUDO_TAMPER_KEYRING,
	SHIRUDO_TAMPER_PIN_UNLINK,
	SHIRUDO_TAMPER_PIN_RENAME,
	SHIRUDO_TAMPER_PIN_XATTR,
	SHIRUDO_TAMPER_BPFFS_UMOUNT,
	SHIRUDO_TAMPER_BPFFS_UMOUNT_ABOVE,
	SHIRUDO_TAMPER_BPFFS_MOUNT,
	SHIRUDO_TAMPER_SEAL_WRITE,
	SHIRUDO_TAMPER_SEAL_UNLINK,
	SHIRUDO_TAMPER_SEAL_RENAME,
	SHIRUDO_TAMPER_SEAL_SETATTR,
	SHIRUDO_TAMPER_SEAL_XATTR,
	SHIRUDO_TAMPER_SEAL_LABEL,
	SHIRUDO_TAMPER_SEAL_PTRACE,
	SHIRUDO_TAMPER_SEAL_PERF,
	SHIRUDO_TAMPER_MAX
};

struct shirudo_tamper_counts {
	uint64_t	count[SHIRUDO_TAMPER_MAX];
	uint64_t	total;
};

/* The name status and the keepalive report a point under. */
const char *shirudo_tamper_name(enum shirudo_tamper_point point);
/* What a count at that point means. */
const char *shirudo_tamper_meaning(enum shirudo_tamper_point point);

/*
 * The counters the LSM has kept this boot. -ENOENT when init has not
 * run, -EPROTO when the pinned map is not the shape this binary knows,
 * another -errno when the pin cannot be read.
 */
int shirudo_tamper_read(struct shirudo_tamper_counts *out);

#endif /* SHIRUDO_UTIL_TAMPER_H */
