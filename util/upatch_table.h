/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The upatches-loaded table of `shirudo status`.
 */

#ifndef SHIRUDO_UTIL_UPATCH_TABLE_H
#define SHIRUDO_UTIL_UPATCH_TABLE_H

#include <stddef.h>
#include <stdint.h>

#include "upatch_resolve.h"

struct shirudo_json_emit;

struct shirudo_upatch_table_row {
	char		name[64];
	char		location[SHIRUDO_UPATCH_LOCATION_MAX];	/* SEC()-style */
	char		loaded_at[32];	/* ISO-8601 UTC, "" if unknown */
	char		status[64];	/* success or a tree skip reason */
	uint32_t	n_maps;
	int64_t		age_sec;	/* derived from loaded_at; -1 if unknown */
	char		payload_sha256[65];	/* "" if unavailable; JSON only */
};

void shirudo_upatch_table_print(const struct shirudo_upatch_table_row *rows,
				size_t n);

void shirudo_upatch_table_emit_json(struct shirudo_json_emit *e,
				    const struct shirudo_upatch_table_row *rows,
				    size_t n);

int64_t shirudo_upatch_age_sec(const char *ts);

#endif /* SHIRUDO_UTIL_UPATCH_TABLE_H */
