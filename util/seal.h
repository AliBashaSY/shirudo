/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * Inode labelling for the bpf/seal.c filesystem-sealing LSM.
 */

#ifndef SHIRUDO_SEAL_H
#define SHIRUDO_SEAL_H

#include <stddef.h>
#include <sys/types.h>

/*
 * The label value: SHA-256 of the xattr name, 64 hex characters, the
 * same for every build. bpf/seal.c takes the label, whatever its value,
 * as marking a protected inode; the identity map says who may touch it.
 */
#define SHIRUDO_SEAL_LABEL \
	"94442b45cfcdc59aabc2a91364ed2ae129418e05a5c1985231b9c032e673f469"

/*
 * Set security.bpf.shirudo = SHIRUDO_SEAL_LABEL on one inode, a
 * symlink's own rather than its target's. Idempotent (create-or-replace).
 * Call after creating any regular-fs file under /var/lib/shirudo/ so the
 * seal LSM protects it. Returns 0 or -errno.
 */
int shirudo_seal_label(const char *path);

/*
 * Label the running binary's inode and walk /var/lib/shirudo/ labelling
 * every inode on that filesystem (the bpffs mount is skipped -- bpf2
 * guards it). Best-effort per entry. Call BEFORE the seal LSM attaches,
 * so the bulk setxattr is not gated. Returns 0 or -errno on a fatal
 * (stat/walk) failure.
 */
int shirudo_seal_label_tree(void);

/*
 * Creating under a sealed directory. The directory decides: an inode
 * created under a labelled directory carries that directory's label,
 * which the seal writes inside the transaction that creates it, on the
 * strength of the mark the directory's open leaves. So these helpers
 * open the directory first and hold it open across the creation, which
 * also keeps its inode and the mark on it in memory, and put the label
 * on afterwards where the kernel did not: no seal attached, a file
 * system that takes no xattrs at creation. All return -errno on
 * failure, a sealed inode that could not be labelled included.
 */

/* Open or create path under its directory. Returns the fd. */
int shirudo_seal_open(const char *path, int flags, mode_t mode);

/* mkdir -p, every directory created labelled as its parent is. */
int shirudo_seal_mkdir_p(const char *path, mode_t mode);

/*
 * Replace path by data atomically: written to <path>.tmp beside it,
 * labelled and synced, renamed over path, the directory synced.
 */
int shirudo_seal_write_file(const char *path, const void *data, size_t len,
			    mode_t mode);

/* Give the inode behind fd the label of its directory, open as dfd. */
int shirudo_seal_inherit(int dfd, int fd);

/* The same by path, without following a symlink. */
int shirudo_seal_inherit_path(const char *path);

#endif /* SHIRUDO_SEAL_H */
