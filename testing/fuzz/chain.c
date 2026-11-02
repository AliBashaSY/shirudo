// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2026 Authors of shirudo
 *
 * The digests of util/chain.c for the fuzz targets, without its TPM.
 */

#include <string.h>

#include "chain.h"
#include "crypto.h"

int shirudo_sha256(const void *data, size_t len,
		   __u8 out[CHAIN_DIGEST_LEN])
{
	return sha256_buffer(data, len, out);
}

int ct_eq(const void *a, const void *b, size_t n)
{
	return !memcmp(a, b, n);
}
