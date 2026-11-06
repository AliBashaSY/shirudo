// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * tpm_attest_parse() of util/tpm_wire.c, which attest verify runs on the
 * quote and certification a target sends, for inputs of every size: it
 * reads nothing outside them, and what it accepts is a quote or an NV
 * certification spanning the whole input, its sized fields inside it.
 */

#define PROOF_MEMCPY_BYTEWISE
#include "proof.h"
#include "../../../util/tpm_wire.c"

/* An NV certification of the longest names, extra data and contents. */
#define ATTEST_MAX	(4u + 2 + 2 + 34 + 2 + 64 + 27 + 2 + 34 + 2 + 2 + 2048)

void harness(void)
{
	size_t len, head, extra_at;
	struct tpm_attest a;
	const __u8 *buf;

	buf = any_bytes(min(len, ATTEST_MAX));
	if (tpm_attest_parse(buf, len, &a))
		return;
	__CPROVER_assert(get_be32(buf) == TPM_GENERATED, "TPM-generated");
	__CPROVER_assert(a.signer_len <= TPM_NAME_LEN,
			 "a signer's name that fits its buffer");
	extra_at = 8 + a.signer_len + 2;
	__CPROVER_assert(a.extra_len <= 64 && a.extra == buf + extra_at,
			 "extra data inside the input, at its place");
	head = extra_at + a.extra_len + 8 + 4 + 4 + 1 + 8;
	if (a.type == TPM_ST_ATTEST_QUOTE) {
		__CPROVER_assert(len == head + 4 + 2 + 1 + TPM_PCR_SELECT_LEN +
				 2 + 32, "a quote spans the whole input");
		__CPROVER_assert(a.pcr_mask < 1u << 24,
				 "one SHA-256 bank of 24 PCRs");
		return;
	}
	__CPROVER_assert(a.type == TPM_ST_ATTEST_NV, "a quote or an NV one");
	__CPROVER_assert(a.index_name_len <= TPM_NAME_LEN,
			 "an index name that fits its buffer");
	__CPROVER_assert(a.nv_len <= 2048 &&
			 len == head + 2 + a.index_name_len + 2 + 2 +
				a.nv_len &&
			 a.nv_data == buf + len - a.nv_len,
			 "the NV contents end the input");
}
