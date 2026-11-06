// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The digests of util/chain.c for the fuzz targets, without its TPM.
 */

#include "chain.h"
#include "crypto.h"

int sha256_calc(const void *data, size_t len,
		__u8 out[CHAIN_DIGEST_LEN])
{
	return sha256_buffer(data, len, out);
}

int chain_fold(__u8 chain[32], const __u8 event[32])
{
	__u8 cat[64];

	memcpy(cat, chain, 32);
	memcpy(cat + 32, event, 32);
	return sha256_calc(cat, sizeof(cat), chain);
}
