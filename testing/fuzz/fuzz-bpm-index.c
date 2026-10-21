// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * bpm_index_parse() on a repository index.
 */

#include "bpm_oci.h"
#include "bpm_package.h"
#include "fuzz.h"

int LLVMFuzzerTestOneInput(const __u8 *data, size_t size)
{
	static struct bpm_index_entry entries[BPM_MAX_ENTRIES];
	int n = 0;

	bpm_index_parse(data, size, entries, &n);
	return 0;
}
