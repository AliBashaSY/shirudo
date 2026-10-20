/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * /proc/modules as a dependency tree, for `status --include-modules`.
 */

#ifndef SHIRUDO_UTIL_KMODULES_H
#define SHIRUDO_UTIL_KMODULES_H

#include <stddef.h>
#include <stdint.h>

#define SHIRUDO_KMOD_NAME_LEN	64

struct shirudo_kmod {
	char		name[SHIRUDO_KMOD_NAME_LEN];
	uint64_t	size;		/* bytes */
	uint32_t	refcount;	/* /proc/modules column 3 */
	uint16_t	n_deps;
	uint16_t	deps_off;	/* index into the global deps[] */
	/*
	 * sysfs `/sys/module/<name>` mtime -- the moment the kernel's
	 * kobject for this module was created, i.e. when it was loaded.
	 * Zero if the stat() failed (race: rmmod between /proc/modules
	 * read and the per-module stat).
	 */
	int64_t		loaded_at_sec;
	int32_t		loaded_at_nsec;
};

struct shirudo_kmod_set {
	struct shirudo_kmod	*mods;	/* sorted by name, ascending */
	size_t			n;
	uint16_t		*deps;	/* flat: mods[i].deps_off..+n_deps */
	size_t			n_deps;
};

int  shirudo_kmodules_read(struct shirudo_kmod_set *set);
void shirudo_kmodules_free(struct shirudo_kmod_set *set);

void shirudo_kmodules_print_tree(const struct shirudo_kmod_set *set);

/*
 * boot_sec is the wallclock instant of boot: the since-boot column is
 * elapsed from boot, not time ago.
 */
void shirudo_kmodules_print_recent(const struct shirudo_kmod_set *set,
				   size_t last_n, int64_t boot_sec);

#endif /* SHIRUDO_UTIL_KMODULES_H */
