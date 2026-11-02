// SPDX-License-Identifier: BSD-2-Clause
/* Copyright (c) 2026 Authors of shirudo */

#include "chain.h"
#include "hex.h"
#include "unit.h"

static void sha256_matches_fips_180_vector(void)
{
	static const char want[] =
		"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
	char hex[2 * 32 + 1];
	__u8 d[32];

	ASSERT_EQ(sha256_calc("abc", 3, d), 0);
	hex_encode(d, sizeof(d), hex);
	EXPECT_STREQ(hex, want);
}

static const struct unit_case cases[] = {
	UNIT_CASE(sha256_matches_fips_180_vector),
	{}
};

UNIT_SUITE(chain, cases);
