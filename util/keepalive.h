/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The packed fields of the keepalive body: the per-operation outcome
 * mask and the conditions word.
 */

#ifndef SHIRUDO_KEEPALIVE_H
#define SHIRUDO_KEEPALIVE_H

#include <stddef.h>

#include <shirudo/misc.h>

#include "conditions.h"

#define KEEPALIVE_MASK_OPS	512
#define KEEPALIVE_MASK_BYTES	(KEEPALIVE_MASK_OPS / 4)
#define KEEPALIVE_MASK_B64_MAX	(4 * ((KEEPALIVE_MASK_BYTES + 2) / 3) + 1)

/*
 * The first code in the highest bits, as the backend reads them; -EINVAL
 * for a code past two bits or more than KEEPALIVE_MASK_OPS codes, -E2BIG
 * for a buffer too small.
 */
int keepalive_mask(const __u8 *codes, size_t n, char *b64,
		   size_t cap) __must_check;

/* Each pair by type name, so that a condition conditions() adds moves none. */
__u16 keepalive_cond(const struct condition *conds, size_t n);

#endif /* SHIRUDO_KEEPALIVE_H */
