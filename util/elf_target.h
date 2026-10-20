/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 */

#ifndef SHIRUDO_ELF_TARGET_H
#define SHIRUDO_ELF_TARGET_H

#include <stdint.h>

#include <shirudo/upatch.h>

struct shirudo_elf_target {
	uint8_t		build_id[SHIRUDO_UPATCH_BUILD_ID_MAX];
	uint32_t	build_id_len;
	uint64_t	*offsets;
	uint32_t	nr_offsets;
};

int shirudo_elf_target_resolve(const char *path, const char *pattern,
			       struct shirudo_elf_target *out);

int shirudo_elf_build_id_fd(int fd,
			    uint8_t build_id[SHIRUDO_UPATCH_BUILD_ID_MAX],
			    uint32_t *build_id_len);

void shirudo_elf_target_free(struct shirudo_elf_target *target);

#endif /* SHIRUDO_ELF_TARGET_H */
