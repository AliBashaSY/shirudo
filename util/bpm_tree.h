/* SPDX-License-Identifier: BSD-2-Clause */
/* Copyright (c) 2026 Authors of shirudo */

#ifndef SHIRUDO_BPM_TREE_H
#define SHIRUDO_BPM_TREE_H

#include <stddef.h>

#include <shirudo/misc.h>

#include "chain.h"

#define SHIRUDO_TREE_MAX_OPS 512
#define SHIRUDO_TREE_MAX_KCONFIG 64
#define SHIRUDO_TREE_MAX_LABELS 64
#define SHIRUDO_TREE_MAX_SELECTOR_VALUES 16
#define SHIRUDO_TREE_STRING_MAX 256

struct shirudo_json_emit;

struct shirudo_tree_kv {
	char key[SHIRUDO_TREE_STRING_MAX];
	char value[SHIRUDO_TREE_STRING_MAX];
};

struct shirudo_tree_selector_kv {
	char key[SHIRUDO_TREE_STRING_MAX];
	char values[SHIRUDO_TREE_MAX_SELECTOR_VALUES][SHIRUDO_TREE_STRING_MAX];
	int n_values;
};

struct shirudo_tree_selector {
	char kernel_release_prefix[SHIRUDO_TREE_MAX_SELECTOR_VALUES][SHIRUDO_TREE_STRING_MAX];
	int n_kernel_release_prefix;
	char kernel_build_id[SHIRUDO_TREE_MAX_SELECTOR_VALUES][SHIRUDO_TREE_STRING_MAX];
	int n_kernel_build_id;
	char architecture[SHIRUDO_TREE_MAX_SELECTOR_VALUES][SHIRUDO_TREE_STRING_MAX];
	int n_architecture;
	struct shirudo_tree_selector_kv kconfig[SHIRUDO_TREE_MAX_KCONFIG];
	int n_kconfig;
	struct shirudo_tree_selector_kv labels[SHIRUDO_TREE_MAX_LABELS];
	int n_labels;
};

struct shirudo_tree_artifact {
	char repository[512];
	char manifest_digest[128];
	char layer_digest[128];
	char payload_sha256[128];
};

struct shirudo_tree_operation {
	__s64 seq;
	char op[16];
	char id[SHIRUDO_TREE_STRING_MAX];
	struct shirudo_tree_selector selector;
	struct shirudo_tree_artifact artifact;
	char expected_payload_sha256[128];
	char operation_digest[128];
};

struct shirudo_tree_publication {
	int schema;
	char tree_id[SHIRUDO_TREE_STRING_MAX];
	__s64 generation;
	char parent_manifest_digest[128];
	__s64 base_seq;
	struct shirudo_tree_operation *operations;
	int operations_capacity;
	int n_operations;
	__s64 head_seq;
};

struct shirudo_tree_state {
	int schema;
	char tree_id[SHIRUDO_TREE_STRING_MAX];
	__s64 generation;
	__s64 last_seq;
	/* Software projection is only a crash-recovery cursor when TPM is absent. */
	char projection_root[65];
	char projection_root_tpm[65];
	char manifest_digest[128];
};

int tree_parse(const char *body, size_t len,
	       struct shirudo_tree_publication *out);
int tree_publication_append(struct shirudo_tree_publication *pub,
			    const struct shirudo_tree_operation *op);
void tree_publication_free(struct shirudo_tree_publication *pub);

/*
 * Canonical forms shared by publisher and target: the operation digest
 * ("sha256:" + 64 hex) over an operation without its digest field, and
 * the publication document. The emitted document is xmalloc'd; release
 * it with xfree(out, out_len).
 */
int tree_operation_digest(const struct shirudo_tree_operation *op,
			  char out[72]);
int tree_emit_publication(const struct shirudo_tree_publication *pub,
			  char **out, size_t *out_len);

/*
 * Evaluate a publication selector against the running node.  An empty
 * selector is an unconstrained wildcard and matches every node.  1 on a
 * match, 0 without one, -errno when the node's labels cannot be read.
 */
int tree_selector_matches(const struct shirudo_tree_selector *selector);

int tree_state_read(struct shirudo_tree_state *out);
int tree_state_write(const struct shirudo_tree_state *state);
int tree_event_append(__s64 seq, const char *operation_digest,
		      const __u8 projection_root[32]);
int tree_event_replay(__u8 out[32]);

void tree_emit_status(struct shirudo_json_emit *e);

/* Whether a tree is configured, in the config image when one is active. */
bool tree_configured(void);

#endif /* SHIRUDO_BPM_TREE_H */
