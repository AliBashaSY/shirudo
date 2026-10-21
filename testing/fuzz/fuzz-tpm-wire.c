// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The TPM structures in a target's evidence: key public areas and their
 * names, quote and certification attestations and their signatures.
 */

#include "fuzz.h"
#include "tpm_wire.h"

int LLVMFuzzerTestOneInput(const __u8 *data, size_t size)
{
	__u8 name[SHIRUDO_TPM_NAME_LEN], r[32], s[32];
	struct shirudo_tpm_ecc_public ecc;
	struct shirudo_tpm_public pub;
	struct shirudo_tpm_attest at;

	shirudo_tpm_public_parse(data, size, &pub);
	shirudo_tpm_ecc_public_parse(data, size, &ecc);
	shirudo_tpm_object_name(data, size, name);
	shirudo_tpm_ecdsa_signature_parse(data, size, r, s);
	shirudo_tpm_attest_parse(data, size, &at);
	return 0;
}
