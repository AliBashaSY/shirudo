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
