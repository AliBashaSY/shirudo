// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * shirudo_tree_parse() on an upatch tree publication, the canonical
 * digest of every operation it accepts and the publication emitted again.
 */

#include "bpm_tree.h"
#include "fuzz.h"
#include "malloc.h"

int LLVMFuzzerTestOneInput(const __u8 *data, size_t size)
{
	struct shirudo_tree_publication pub;
	char digest[72], *out;
	size_t len;

	if (shirudo_tree_parse((const char *)data, size, &pub))
		return 0;
	for (int i = 0; i < pub.n_operations; i++)
		shirudo_tree_operation_digest(&pub.operations[i], digest);
	if (!shirudo_tree_emit_publication(&pub, &out, &len))
		xfree(out, len);
	shirudo_tree_publication_free(&pub);
	return 0;
}
