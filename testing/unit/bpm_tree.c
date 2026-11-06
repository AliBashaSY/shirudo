// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <errno.h>

#include <shirudo/string.h>

#include "bpm_tree.h"
#include "malloc.h"
#include "unit.h"

#define DIGEST "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"

static void set(char *dst, size_t cap, const char *src)
{
	ASSERT_TRUE(strscpy(dst, src, cap) >= 0);
}

static struct tree_operation *operation(void)
{
	struct tree_operation *op = xcalloc(1, sizeof(*op));
	struct tree_selector_kv *label;

	ASSERT_TRUE(op);
	op->seq = 1;
	set(op->op, sizeof(op->op), "add");
	set(op->id, sizeof(op->id), "t");
	set(op->artifact.repository, sizeof(op->artifact.repository),
	    "oci://registry.example/t");
	set(op->artifact.manifest_digest, sizeof(op->artifact.manifest_digest),
	    DIGEST);
	set(op->artifact.layer_digest, sizeof(op->artifact.layer_digest),
	    DIGEST);
	set(op->artifact.payload_sha256, sizeof(op->artifact.payload_sha256),
	    DIGEST);
	op->selector.n_labels = 1;
	label = &op->selector.labels[0];
	set(label->key, sizeof(label->key), "site");
	label->n_values = 1;
	set(label->values[0], sizeof(label->values[0]), "a b");
	return op;
}

/*
 * The node's parser refuses a control character, so a publication
 * holding one would go out, signed, and fail every node's sync.
 */
static void operation_takes_no_control_character(void)
{
	struct tree_operation *op = operation();
	struct tree_selector_kv *label = &op->selector.labels[0];
	char digest[72];

	EXPECT_EQ(tree_operation_digest(op, digest), 0);
	label->values[0][1] = '\t';
	EXPECT_EQ(tree_operation_digest(op, digest), -EINVAL);
	label->values[0][1] = ' ';
	label->key[0] = '\n';
	EXPECT_EQ(tree_operation_digest(op, digest), -EINVAL);
	label->key[0] = 's';
	op->artifact.repository[0] = '\r';
	EXPECT_EQ(tree_operation_digest(op, digest), -EINVAL);
	xfree(op, sizeof(*op));
}

/* What the publisher writes, the node reads back to the same digest. */
static void publication_reads_back(void)
{
	struct tree_publication pub = { .schema = 1, .generation = 1 };
	struct tree_publication back = {};
	struct tree_operation *op = operation();
	char *doc = NULL, again[72];
	size_t len = 0;

	set(pub.tree_id, sizeof(pub.tree_id), "t");
	ASSERT_EQ(tree_operation_digest(op, op->operation_digest), 0);
	ASSERT_EQ(tree_publication_append(&pub, op), 0);
	pub.head_seq = 1;
	ASSERT_EQ(tree_emit_publication(&pub, &doc, &len), 0);
	ASSERT_EQ(tree_parse(doc, len, &back), 0);
	ASSERT_EQ(back.n_operations, 1);
	ASSERT_EQ(tree_operation_digest(&back.operations[0], again), 0);
	EXPECT_STREQ(again, op->operation_digest);
	EXPECT_STREQ(back.operations[0].selector.labels[0].values[0], "a b");
	xfree(doc, len);
	tree_publication_free(&pub);
	tree_publication_free(&back);
	xfree(op, sizeof(*op));
}

static const struct unit_case cases[] = {
	UNIT_CASE(operation_takes_no_control_character),
	UNIT_CASE(publication_reads_back),
	{}
};

UNIT_SUITE(bpm_tree, cases);
