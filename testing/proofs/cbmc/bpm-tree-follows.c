// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * tree_publication_follows() of util/bpm_tree.c, which decides whether
 * bpm tree sync may apply a signed publication over the state the node
 * recorded, for every sequence number and generation a __s64 holds: it
 * accepts exactly a publication of the same tree, of a later generation
 * unless it is the same manifest, naming the recorded parent when it is
 * the next generation, with the node's cursor between its base and head,
 * and without a state one that starts at 0. Strings are of up to two
 * bytes.
 */

#include <stdbool.h>

#include "proof.h"
#include "../../../util/hex.c"
#include "../../../util/bpm_tree.c"

static void short_string(const char *s)
{
	__CPROVER_assume(s[2] == '\0');
}

static void follows_the_state(void)
{
	struct tree_publication pub;
	struct tree_state state;
	char manifest[3];
	bool have, same, next;
	int ret;

	short_string(pub.tree_id);
	short_string(pub.parent_manifest_digest);
	short_string(state.tree_id);
	short_string(state.manifest_digest);
	short_string(manifest);
	ret = tree_publication_follows(&pub, have ? &state : NULL, manifest);
	if (!have) {
		__CPROVER_assert(!ret == (pub.base_seq == 0),
				 "a first publication starts at 0");
		return;
	}
	same = !strcmp(state.manifest_digest, manifest);
	next = pub.generation > state.generation &&
	       pub.generation - 1 == state.generation;
	__CPROVER_assert(ret || !strcmp(pub.tree_id, state.tree_id),
			 "the same tree");
	__CPROVER_assert(ret || same || pub.generation > state.generation,
			 "never an earlier or the same generation");
	__CPROVER_assert(ret || same || !next ||
			 !pub.parent_manifest_digest[0] ||
			 !strcmp(pub.parent_manifest_digest,
				 state.manifest_digest),
			 "the next generation names the recorded parent");
	__CPROVER_assert(ret || (pub.head_seq >= state.last_seq &&
				 pub.base_seq <= state.last_seq),
			 "the cursor between base and head");
	__CPROVER_assert(!ret || strcmp(pub.tree_id, state.tree_id) ||
			 (!same && pub.generation <= state.generation) ||
			 (!same && next && pub.parent_manifest_digest[0] &&
			  strcmp(pub.parent_manifest_digest,
				 state.manifest_digest)) ||
			 pub.head_seq < state.last_seq ||
			 pub.base_seq > state.last_seq,
			 "and refused for nothing else");
}

void harness(void)
{
	follows_the_state();
}
