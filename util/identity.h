/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The identity init froze into the kernel, read back from its bpffs pin.
 */

#ifndef SHIRUDO_UTIL_IDENTITY_H
#define SHIRUDO_UTIL_IDENTITY_H

#include <shirudo/misc.h>

/* The keyring kinds the kernel names in a verdict (enum bpf_sig_keyring). */
#define SHIRUDO_SIG_KEYRING_USER	4
#define SHIRUDO_SIG_KEYRING_BPF		5

/* The most keys the anchor keyring holds; util/keys.h bounds the set. */
#define SHIRUDO_IDENTITY_KEYS_MAX	16

/*
 * Must match struct shirudo_sb_id in bpf/lib/shirudo_identity.h byte for
 * byte. exe is the binary that keeps running the target; alt_exe, when
 * set, the initrd's copy that ran init and is gone after the switch to
 * the root. key_serials are the anchor keyring's keys, which
 * lsm/key_permission guards with the keyring itself.
 */
struct shirudo_identity {
	__u32		sb_dev;
	__u32		exe_dev;
	__u64		exe_ino;
	__s32		keyring_serial;
	__u32		keyring_type;
	__u32		alt_exe_dev;
	__u32		pad;
	__u64		alt_exe_ino;
	__u32		n_keys;
	__u32		pad2;
	__s32		key_serials[SHIRUDO_IDENTITY_KEYS_MAX];
};

/*
 * The identity map init populated, froze and pinned this boot. -ENOENT
 * when init has not run, another -errno when the pin cannot be read.
 */
int identity_read(struct shirudo_identity *out);

#endif /* SHIRUDO_UTIL_IDENTITY_H */
