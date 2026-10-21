/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * /proc/modules as a dependency tree, for `status --include-modules`.
 */

#ifndef SHIRUDO_UTIL_KMODULES_H
#define SHIRUDO_UTIL_KMODULES_H

#include <stddef.h>

#include <shirudo/misc.h>

#define SHIRUDO_KMOD_NAME_LEN	64

struct shirudo_kmod {
	char		name[SHIRUDO_KMOD_NAME_LEN];
	__u64		size;		/* bytes */
	__u32		refcount;	/* /proc/modules column 3 */
	__u16		n_deps;
	__u16		deps_off;	/* index into the global deps[] */
	/*
	 * sysfs `/sys/module/<name>` mtime -- the moment the kernel's
	 * kobject for this module was created, i.e. when it was loaded.
	 * Zero if the stat() failed (race: rmmod between /proc/modules
	 * read and the per-module stat).
	 */
	__s64		loaded_at_sec;
	__s32		loaded_at_nsec;
};

struct shirudo_kmod_set {
	struct shirudo_kmod	*mods;	/* sorted by name, ascending */
	size_t			n;
	__u16			*deps;	/* flat: mods[i].deps_off..+n_deps */
	size_t			n_deps;
};

int  kmodules_read(struct shirudo_kmod_set *set);
/* The text of /proc/modules, modified in place, into set. */
int  kmodules_parse(char *buf, struct shirudo_kmod_set *set);
void kmodules_free(struct shirudo_kmod_set *set);

void kmodules_print_tree(const struct shirudo_kmod_set *set);

/*
 * boot_sec is the wallclock instant of boot: the since-boot column is
 * elapsed from boot, not time ago.
 */
void kmodules_print_recent(const struct shirudo_kmod_set *set,
			   size_t last_n, __s64 boot_sec);

#endif /* SHIRUDO_UTIL_KMODULES_H */
