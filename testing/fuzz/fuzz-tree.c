// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * tree_parse() on an upatch tree publication, the canonical
 * digest of every operation it accepts and the publication emitted again,
 * which must read back with the same operations and digests.
 */

#include <stdlib.h>

#include "fuzz.h"

#include <shirudo/string.h>

#include "bpm_tree.h"
#include "malloc.h"

int LLVMFuzzerTestOneInput(const __u8 *data, size_t size)
{
	struct tree_publication pub, again;
	char digest[72], back[72], *out;
	size_t len;

	if (tree_parse((const char *)data, size, &pub))
		return 0;
	if (tree_emit_publication(&pub, &out, &len)) {
		tree_publication_free(&pub);
		return 0;
	}
	if (tree_parse(out, len, &again) ||
	    again.n_operations != pub.n_operations ||
	    strcmp(again.tree_id, pub.tree_id) ||
	    again.generation != pub.generation ||
	    again.head_seq != pub.head_seq)
		abort();
	for (int i = 0; i < pub.n_operations; i++) {
		if (tree_operation_digest(&pub.operations[i], digest) !=
		    tree_operation_digest(&again.operations[i], back) ||
		    strcmp(digest, back))
			abort();
	}
	tree_publication_free(&again);
	xfree(out, len);
	tree_publication_free(&pub);
	return 0;
}
