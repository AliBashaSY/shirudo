/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The tamper counters the LSM keeps, read back from their bpffs pin.
 */
#ifndef SHIRUDO_UTIL_TAMPER_H
#define SHIRUDO_UTIL_TAMPER_H

#include <shirudo/misc.h>

#include "bpf/lib/tamper.h"

struct shirudo_tamper_counts {
	__u64		count[SHIRUDO_TAMPER_MAX];
	__u64		total;
};

const char *tamper_name(enum shirudo_tamper_point point);
const char *tamper_meaning(enum shirudo_tamper_point point);

/*
 * The counters the LSM has kept this boot. -ENOENT when init has not
 * run, -EPROTO when the pinned map is not the shape this binary knows,
 * another -errno when the pin cannot be read.
 */
int tamper_read(struct shirudo_tamper_counts *out);

#endif /* SHIRUDO_UTIL_TAMPER_H */
