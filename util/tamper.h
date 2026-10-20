/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The tamper counters the LSM keeps, read back from their bpffs pin.
 */
#ifndef SHIRUDO_UTIL_TAMPER_H
#define SHIRUDO_UTIL_TAMPER_H

#include <stdint.h>

#include "bpf/lib/shirudo_tamper.h"

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
