// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include <string.h>

#include "crypto.h"
#include "unit.h"

static void sha256_buffer_zeroes_a_failed_digest(void)
{
	__u8 out[CRYPTO_SHA256_LEN], zero[CRYPTO_SHA256_LEN] = {};

	memset(out, 0xff, sizeof(out));
	EXPECT_NE(sha256_buffer(NULL, 3, out), 0);
	EXPECT_MEMEQ(out, zero, sizeof(out));
}

static const struct unit_case cases[] = {
	UNIT_CASE(sha256_buffer_zeroes_a_failed_digest),
	{}
};

UNIT_SUITE(crypto_openssl, cases);
