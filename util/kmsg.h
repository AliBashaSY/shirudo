/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * /dev/kmsg records with wallclock timestamps.
 */

#ifndef SHIRUDO_UTIL_KMSG_H
#define SHIRUDO_UTIL_KMSG_H

#include <stddef.h>

#define KMSG_TS_LEN	24	/* "2026-05-28T14:32:01Z" + slack */
#define KMSG_MSG_LEN	480	/* one printk record, joined */

struct shirudo_kmsg_record {
	char	ts[KMSG_TS_LEN];	/* ISO-8601 UTC, NUL-term */
	char	msg[KMSG_MSG_LEN];	/* control bytes stripped */
};

/*
 * Records the ring buffer has dropped are skipped: EPIPE restarts the
 * read.
 */
int kmsg_tail(struct shirudo_kmsg_record *out, size_t cap);

#endif /* SHIRUDO_UTIL_KMSG_H */
